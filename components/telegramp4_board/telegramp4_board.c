#include "telegramp4_board.h"
#include "esp_log.h"

static const char *TAG = "TAG_BOARD";

void telegramp4_board_print_banner(void)
{
    ESP_LOGI(TAG, "TelegramP4 starting...");
    ESP_LOGI(TAG, "Board: %s", TELEGRAMP4_BOARD_NAME);
    ESP_LOGI(TAG, "Firmware version: %s", TELEGRAMP4_FIRMWARE_VERSION);
}
