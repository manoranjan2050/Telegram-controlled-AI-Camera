#include <string.h>
#include <stdlib.h>
#include "telegramp4_gpio.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "TAG_GPIO";

static int  s_whitelist[TELEGRAMP4_GPIO_MAX_WHITELISTED];
static bool s_state[TELEGRAMP4_GPIO_MAX_WHITELISTED];
static int  s_count = 0;

esp_err_t telegramp4_gpio_init(void)
{
    char buf[128];
    strncpy(buf, CONFIG_TELEGRAMP4_GPIO_WHITELIST, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *token = strtok_r(buf, ", ", &saveptr);
    while (token != NULL && s_count < TELEGRAMP4_GPIO_MAX_WHITELISTED) {
        char *end = NULL;
        long pin = strtol(token, &end, 10);
        if (end != token) {
            gpio_config_t io_conf = {
                .pin_bit_mask = 1ULL << pin,
                .mode = GPIO_MODE_OUTPUT,
            };
            if (gpio_config(&io_conf) == ESP_OK) {
                gpio_set_level((gpio_num_t) pin, 0);
                s_whitelist[s_count] = (int) pin;
                s_state[s_count] = false;
                s_count++;
            } else {
                ESP_LOGW(TAG, "Failed to configure whitelisted GPIO %ld as output", pin);
            }
        }
        token = strtok_r(NULL, ", ", &saveptr);
    }

    ESP_LOGI(TAG, "GPIO whitelist: %d pin(s) configured", s_count);
    return ESP_OK;
}

int telegramp4_gpio_get_whitelist(int *out_pins, int max)
{
    int n = s_count < max ? s_count : max;
    for (int i = 0; i < n; i++) {
        out_pins[i] = s_whitelist[i];
    }
    return n;
}

static int find_index(int pin)
{
    for (int i = 0; i < s_count; i++) {
        if (s_whitelist[i] == pin) {
            return i;
        }
    }
    return -1;
}

bool telegramp4_gpio_is_whitelisted(int pin)
{
    return find_index(pin) >= 0;
}

esp_err_t telegramp4_gpio_set(int pin, bool on)
{
    int idx = find_index(pin);
    if (idx < 0) {
        ESP_LOGW(TAG, "Rejected /gpio request for non-whitelisted pin %d", pin);
        return ESP_ERR_NOT_FOUND;
    }
    gpio_set_level((gpio_num_t) pin, on ? 1 : 0);
    s_state[idx] = on;
    return ESP_OK;
}

bool telegramp4_gpio_get(int pin)
{
    int idx = find_index(pin);
    return idx >= 0 ? s_state[idx] : false;
}
