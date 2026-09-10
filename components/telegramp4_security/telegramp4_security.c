#include <string.h>
#include <stdlib.h>
#include "telegramp4_security.h"
#include "esp_log.h"

static const char *TAG = "TAG_SECURITY";

#define MAX_ALLOWED_CHAT_IDS 16

static int64_t s_allowed_ids[MAX_ALLOWED_CHAT_IDS];
static int     s_allowed_count = -1; /* -1 = not parsed yet */
static char    s_chat_ids_csv[128] = {0};

void telegramp4_security_configure(const char *chat_ids_csv)
{
    strncpy(s_chat_ids_csv, chat_ids_csv ? chat_ids_csv : "", sizeof(s_chat_ids_csv) - 1);
    s_allowed_count = -1; /* force reparse on next check */
}

static void parse_allowed_ids(void)
{
    s_allowed_count = 0;

    char buf[256];
    const char *source = strlen(s_chat_ids_csv) > 0 ? s_chat_ids_csv : CONFIG_TELEGRAMP4_TELEGRAM_ALLOWED_CHAT_IDS;
    strncpy(buf, source, sizeof(buf) - 1);
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

int telegramp4_security_get_allowed_ids(int64_t *out_ids, int max)
{
    if (s_allowed_count < 0) {
        parse_allowed_ids();
    }
    int n = s_allowed_count < max ? s_allowed_count : max;
    for (int i = 0; i < n; i++) {
        out_ids[i] = s_allowed_ids[i];
    }
    return n;
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
