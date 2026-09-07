#include "telegramp4_motion.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TAG_MOTION";

static volatile bool s_armed = false;
static TaskHandle_t s_task_handle = NULL;
static telegramp4_motion_cb_t s_callback = NULL;

static void IRAM_ATTR pir_isr_handler(void *arg)
{
    BaseType_t higher_prio_woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_task_handle, &higher_prio_woken);
    if (higher_prio_woken) {
        portYIELD_FROM_ISR();
    }
}

static void motion_task(void *arg)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (s_armed && s_callback) {
            s_callback();
        }
    }
}

esp_err_t telegramp4_motion_init(telegramp4_motion_cb_t cb)
{
#if !CONFIG_TELEGRAMP4_MOTION_ENABLED
    ESP_LOGI(TAG, "Motion detection disabled (TelegramP4 Configuration -> Motion -> Enable)");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (CONFIG_TELEGRAMP4_MOTION_PIR_GPIO < 0) {
        ESP_LOGE(TAG, "Motion enabled but no PIR GPIO configured (TELEGRAMP4_MOTION_PIR_GPIO)");
        return ESP_ERR_INVALID_STATE;
    }

    s_callback = cb;

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << CONFIG_TELEGRAMP4_MOTION_PIR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE, /* assumes active-high PIR output - verify against your module */
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        return err;
    }

    /* 16KB: the registered callback does camera capture + AI + HTTPS
     * broadcast to Telegram (see app_main.cpp on_motion_detected()), which
     * needs real headroom - see the stack-overflow note on telegram_poll_task
     * in telegramp4_telegram.c for why 2-8KB isn't enough for HTTPS calls. */
    xTaskCreate(motion_task, "motion_task", 16384, NULL, 6, &s_task_handle);

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { /* INVALID_STATE = already installed by another module */
        return err;
    }
    return gpio_isr_handler_add((gpio_num_t) CONFIG_TELEGRAMP4_MOTION_PIR_GPIO, pir_isr_handler, NULL);
#endif
}

void telegramp4_motion_arm(void)
{
    s_armed = true;
    ESP_LOGI(TAG, "Motion detection: ARMED");
}

void telegramp4_motion_disarm(void)
{
    s_armed = false;
    ESP_LOGI(TAG, "Motion detection: DISARMED");
}

bool telegramp4_motion_is_armed(void)
{
    return s_armed;
}
