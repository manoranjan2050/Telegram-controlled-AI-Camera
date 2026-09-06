#include <string.h>
#include <stdlib.h>
#include "telegramp4_security.h"
#include "esp_log.h"

static const char *TAG = "TAG_SECURITY";

#define MAX_ALLOWED_CHAT_IDS 16

static int64_t s_allowed_ids[MAX_ALLOWED_CHAT_IDS];
static int     s_allowed_count = -1; /* -1 = not parsed yet */

static void parse_allowed_ids(void)
{
    s_allowed_count = 0;

    char buf[256];
    strncpy(buf, CONFIG_TELEGRAMP4_TELEGRAM_ALLOWED_CHAT_IDS, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *token = strtok_r(buf, ", ", &saveptr);
    while (token != NULL && s_allowed_count < MAX_ALLOWED_CHAT_IDS) {
        char *end = NULL;
        long long id = strtoll(token, &end, 10);
        if (end != token) {
            s_allowed_ids[s_allowed_count++] = (int64_t) id;
        }
        token = strtok_r(NULL, ", ", &saveptr);
    }

    if (s_allowed_count == 0) {
        ESP_LOGW(TAG, "No allowed chat IDs configured - every command will be denied. "
                       "Set TELEGRAMP4_TELEGRAM_ALLOWED_CHAT_IDS in idf.py menuconfig.");
    } else {
        ESP_LOGI(TAG, "Loaded %d allowed chat ID(s)", s_allowed_count);
    }
}

bool telegramp4_security_is_authorized(int64_t chat_id)
{
    if (s_allowed_count < 0) {
        parse_allowed_ids();
    }
    for (int i = 0; i < s_allowed_count; i++) {
        if (s_allowed_ids[i] == chat_id) {
            return true;
        }
    }
    return false;
}
