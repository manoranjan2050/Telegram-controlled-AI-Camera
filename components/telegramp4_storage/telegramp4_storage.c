#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "telegramp4_storage.h"

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_default_configs.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
 * Real hardware finding (2026-09-07), confirmed from DFRobot's DFR1172
 * schematic (page 4/8, "SD" sheet): the MicroSD socket's VDD is switched by a
 * P-MOSFET (Q1, AO3401) whose gate is driven by GPIO45 ("SD1_PWRN" net,
 * active-low - drive LOW to power the card on). Our mount call was never
 * powering the card at all, which is why it timed out
 * (sdmmc_init_ocr/send_op_cond 0x107) even with correct CLK/CMD/D0-D3 pins.
 */
#define TELEGRAMP4_SD_PWRN_GPIO 45

static const char *TAG = "TAG_STORAGE";

static sdmmc_card_t *s_card = NULL;
static bool s_mounted = false;

static const char *kSubdirs[] = {"photos", "videos", "audio", "received", "ai", "logs"};

esp_err_t telegramp4_storage_init(void)
{
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false, /* never silently reformat a user's card */
        /*
         * Lowered from 8 (2026-09-10): the global VFS file-descriptor table
         * is shared across every VFS user - stdio, the camera's video device
         * nodes, this FATFS mount, and lwip's socket range
         * (CONFIG_LWIP_MAX_SOCKETS). Once the camera and SD init order fix
         * let both actually succeed at boot for the first time, their
         * combined FD usage pushed lwip's own socket-range registration
         * over the shared table's limit (`esp_vfs_lwip_sockets_register`
         * aborting with ESP_ERR_NO_MEM). The app never opens more than 1-2
         * files at once, so 8 was never needed.
         */
        .max_files = 3,
        .allocation_unit_size = 16 * 1024,
    };

    /*
     * Real hardware finding (2026-09-07): ESP32-P4's WiFi connectivity goes
     * through the onboard ESP32-C6 over SDIO using the SDMMC host peripheral's
     * SLOT_1 (confirmed in the boot log: "SDIO master: Slot 1" from
     * ESP-Hosted). SDMMC_HOST_DEFAULT() also defaults to SLOT_1, so
     * initializing the SD card with the default host config collided with the
     * WiFi transport and crashed/reset the C6 co-processor in a boot loop.
     * Using SLOT_0 for the SD card avoids the conflict.
     */
    /* Power on the SD card before touching the bus at all. */
    gpio_config_t pwrn_conf = {
        .pin_bit_mask = 1ULL << TELEGRAMP4_SD_PWRN_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&pwrn_conf);
    /*
     * Tested both polarities on real hardware (2026-09-07): driving GPIO45
     * LOW and HIGH produced byte-for-byte identical mount failures
     * (same 0x107 timeout at the same point), which means this pin is very
     * likely not the blocking factor at all - see the note in
     * docs/hardware.md and docs/lessons/07-microsd.md for the leading
     * hypothesis (no card inserted / bad card / not FAT32) and what to check
     * next. Left at active-low (0), the electrically sensible default for a
     * P-MOSFET high-side switch, since neither polarity is proven wrong.
     */
    gpio_set_level((gpio_num_t) TELEGRAMP4_SD_PWRN_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100)); /* let the card's supply rail stabilize before enumeration */

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    /*
     * Untried until now (2026-09-10): the CMD/D0-D3 lines need pull-ups to
     * idle high, or the card never reliably answers the very first
     * identification commands - which matches our exact failure mode
     * (sdmmc_init_ocr/send_op_cond timing out, 0x107). Whether DFRobot
     * populated external pull-ups on this board is unconfirmed; enabling the
     * driver's internal ones is free to try and is what ESP-IDF's own
     * examples/storage/sd_card/sdmmc example does unconditionally for every
     * target including ESP32-P4.
     */
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(TELEGRAMP4_SD_MOUNT_POINT, &host, &slot_config,
                                             &mount_config, &s_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. Card may need formatting, or is not FAT32.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card: %s (check SDMMC pin config against docs/hardware.md)",
                      esp_err_to_name(ret));
        }
        return ret;
    }

    s_mounted = true;
    ESP_LOGI(TAG, "SD card mounted at %s", TELEGRAMP4_SD_MOUNT_POINT);

    for (size_t i = 0; i < sizeof(kSubdirs) / sizeof(kSubdirs[0]); i++) {
        char path[64];
        snprintf(path, sizeof(path), "%s/%s", TELEGRAMP4_SD_MOUNT_POINT, kSubdirs[i]);
        if (mkdir(path, 0777) != 0) {
            /* EEXIST is expected on every boot after the first - not an error. */
            struct stat st;
            if (stat(path, &st) != 0) {
                ESP_LOGW(TAG, "Could not create/verify directory %s", path);
            }
        }
    }

    return ESP_OK;
}

telegramp4_storage_status_t telegramp4_storage_get_status(void)
{
    telegramp4_storage_status_t status = {0};
    status.mounted = s_mounted;
    if (s_mounted) {
        uint64_t total = 0, free_bytes = 0;
        if (esp_vfs_fat_info(TELEGRAMP4_SD_MOUNT_POINT, &total, &free_bytes) == ESP_OK) {
            status.total_bytes = total;
            status.free_bytes = free_bytes;
            status.used_bytes = total - free_bytes;
        }
    }
    return status;
}

/**
 * Returns true if `subdir` is one of the fixed, known-safe subdirectories this
 * component created. Never accept a caller-supplied subdir string directly.
 */
static bool is_known_subdir(const char *subdir)
{
    for (size_t i = 0; i < sizeof(kSubdirs) / sizeof(kSubdirs[0]); i++) {
        if (strcmp(subdir, kSubdirs[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool telegramp4_storage_sanitize_path(const char *subdir, const char *raw_name, char *out, size_t out_len)
{
    if (!is_known_subdir(subdir)) {
        ESP_LOGE(TAG, "Rejected unknown subdir: %s", subdir);
        return false;
    }
    if (raw_name == NULL || raw_name[0] == '\0') {
        return false;
    }

    /* Strip to just the final path component: reject '/', '\\', and any ".."
     * segment outright rather than trying to "clean" a traversal attempt. */
    if (strstr(raw_name, "..") != NULL) {
        ESP_LOGW(TAG, "Rejected filename containing '..': %s", raw_name);
        return false;
    }

    char safe_name[128];
    size_t o = 0;
    for (size_t i = 0; raw_name[i] != '\0' && o < sizeof(safe_name) - 1; i++) {
        char c = raw_name[i];
        bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        if (allowed) {
            safe_name[o++] = c;
        }
        /* Anything else (including '/', '\\', spaces, control chars) is dropped
         * rather than rejecting the whole request - keeps the UX simple for
         * filenames like Telegram-supplied captions that may contain spaces. */
    }
    safe_name[o] = '\0';

    if (o == 0 || safe_name[0] == '.') {
        ESP_LOGW(TAG, "Filename empty or unsafe after sanitization (raw: %s)", raw_name);
        return false;
    }

    int written = snprintf(out, out_len, "%s/%s/%s", TELEGRAMP4_SD_MOUNT_POINT, subdir, safe_name);
    if (written < 0 || (size_t) written >= out_len) {
        return false;
    }
    return true;
}
