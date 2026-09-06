#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "telegramp4_stt.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

static const char *TAG = "TAG_STT";

#define STT_MULTIPART_BOUNDARY "TelegramP4STTBoundary9e2b"
#define STT_RESPONSE_MAX (8 * 1024)

typedef struct {
    char   *buf;
    size_t  len;
} resp_buf_t;

esp_err_t telegramp4_stt_transcribe(const uint8_t *audio_data, size_t len, char *out_text, size_t out_text_len)
{
#if !CONFIG_TELEGRAMP4_STT_ENABLED
    (void) audio_data; (void) len; (void) out_text; (void) out_text_len;
    ESP_LOGW(TAG, "STT disabled (TelegramP4 Configuration -> Speech-to-Text -> Enable)");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (strlen(CONFIG_TELEGRAMP4_STT_API_KEY) == 0 ||
        strcmp(CONFIG_TELEGRAMP4_STT_API_KEY, "sk-REPLACE_WITH_YOUR_API_KEY") == 0) {
        ESP_LOGE(TAG, "STT enabled but no API key configured");
        return ESP_ERR_INVALID_STATE;
    }

    /* multipart/form-data: "model" field + "file" field (audio bytes) */
    char part1[64];
    int part1_len = snprintf(part1, sizeof(part1),
        "--" STT_MULTIPART_BOUNDARY "\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-1\r\n");

    char part2[160];
    int part2_len = snprintf(part2, sizeof(part2),
        "--" STT_MULTIPART_BOUNDARY "\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"voice.ogg\"\r\n"
        "Content-Type: audio/ogg\r\n\r\n");

    static const char part3[] = "\r\n--" STT_MULTIPART_BOUNDARY "--\r\n";
    int part3_len = sizeof(part3) - 1;

    size_t content_length = (size_t) part1_len + (size_t) part2_len + len + (size_t) part3_len;

    esp_http_client_config_t config = {
        .url = "https://api.openai.com/v1/audio/transcriptions",
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30 * 1000,
    };
    resp_buf_t resp = {0};
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_FAIL;
    }

    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", CONFIG_TELEGRAMP4_STT_API_KEY);
    esp_http_client_set_header(client, "Authorization", auth_header);

    char content_type[64];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", STT_MULTIPART_BOUNDARY);
    esp_http_client_set_header(client, "Content-Type", content_type);

    esp_err_t err = esp_http_client_open(client, (int) content_length);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }
    if (esp_http_client_write(client, part1, part1_len) < 0 ||
        esp_http_client_write(client, part2, part2_len) < 0 ||
        esp_http_client_write(client, (const char *) audio_data, (int) len) < 0 ||
        esp_http_client_write(client, part3, part3_len) < 0) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int content_len = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (content_len > 0 && content_len <= STT_RESPONSE_MAX) {
        resp.buf = (char *) malloc((size_t) content_len + 1);
        if (resp.buf) {
            int total = 0;
            int r;
            while (total < content_len &&
                   (r = esp_http_client_read(client, resp.buf + total, content_len - total)) > 0) {
                total += r;
            }
            resp.buf[total] = '\0';
            resp.len = (size_t) total;
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status != 200 || !resp.buf) {
        ESP_LOGE(TAG, "Transcription request failed (HTTP %d)", status);
        free(resp.buf);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(resp.buf);
    free(resp.buf);
    if (!root) {
        return ESP_FAIL;
    }
    cJSON *text = cJSON_GetObjectItem(root, "text");
    if (!text || !cJSON_IsString(text)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    strncpy(out_text, text->valuestring, out_text_len - 1);
    out_text[out_text_len - 1] = '\0';
    cJSON_Delete(root);
    return ESP_OK;
#endif
}
