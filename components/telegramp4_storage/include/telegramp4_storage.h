/**
 * telegramp4_storage — MicroSD mount, directory layout, and safe file access
 * (Phase 7).
 *
 * ⚠️ Pin mapping status: SDMMC pins default to ESP-IDF's SoC-level default for
 * ESP32-P4 (SDMMC_SLOT_CONFIG_DEFAULT(): CLK=43 CMD=44 D0=39 D1=40 D2=41 D3=42),
 * exposed as overridable Kconfig options. This is Espressif's own reference
 * default for the chip, not a confirmed FireBeetle 2-specific pinout — DFRobot's
 * board may route the SD slot differently. Confirm against DFRobot's schematic
 * before trusting this on real hardware (see docs/hardware.md).
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TELEGRAMP4_SD_MOUNT_POINT "/sdcard"

typedef struct {
    bool     mounted;
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
} telegramp4_storage_status_t;

/** Mounts the SD card and creates photos/videos/audio/received/ai/logs subdirs. */
esp_err_t telegramp4_storage_init(void);

telegramp4_storage_status_t telegramp4_storage_get_status(void);

/**
 * Sanitizes a filename coming from Telegram input (received files, /delete
 * arguments, gallery callbacks) before it ever touches the filesystem.
 * Rejects/strips path separators, "..", and anything that would let the caller
 * escape `subdir`. Writes the safe, absolute path into `out` (e.g.
 * "/sdcard/photos/received_20260906.jpg"). Returns false if `raw_name` cannot
 * be made safe (empty after sanitization, etc.) — callers must not proceed with
 * the file operation in that case.
 */
bool telegramp4_storage_sanitize_path(const char *subdir, const char *raw_name, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
