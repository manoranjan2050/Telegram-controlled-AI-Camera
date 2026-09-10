#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "telegramp4_video.h"
#include "telegramp4_camera.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char *TAG = "TAG_VIDEO";

/* 1 Mbps is plenty for an 800x800 clip and keeps the file small enough for
 * a quick Telegram upload over the ESP32-C6's Wi-Fi link. */
#define TELEGRAMP4_VIDEO_BITRATE_BPS (1 * 1000 * 1000)

/* Generous cap so a long recording can't run away with all of PSRAM;
 * comfortably above what CONFIG_TELEGRAMP4_VIDEO_MAX_DURATION_S seconds at
 * the bitrate above would ever produce. */
#define TELEGRAMP4_VIDEO_MAX_BYTES (8 * 1024 * 1024)

esp_err_t telegramp4_video_record(uint32_t duration_s, telegramp4_video_result_t *out_result)
{
    if (!out_result) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out_result, 0, sizeof(*out_result));

    esp_err_t err = telegramp4_camera_start_video_mode(TELEGRAMP4_VIDEO_BITRATE_BPS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start video mode: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t *buf = (uint8_t *) heap_caps_malloc(TELEGRAMP4_VIDEO_MAX_BYTES, MALLOC_CAP_SPIRAM);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes for video buffer", (unsigned) TELEGRAMP4_VIDEO_MAX_BYTES);
        telegramp4_camera_stop_video_mode();
        return ESP_ERR_NO_MEM;
    }

    size_t written = 0;
    uint32_t frame_count = 0;
    int64_t start_us = esp_timer_get_time();
    int64_t end_us = start_us + (int64_t) duration_s * 1000000;

    while (esp_timer_get_time() < end_us) {
        telegramp4_camera_frame_t frame = {0};
        if (telegramp4_camera_read_video_frame(&frame) != ESP_OK) {
            ESP_LOGW(TAG, "Frame read failed after %" PRIu32 " frame(s), stopping recording early", frame_count);
            break;
        }
        if (written + frame.len > TELEGRAMP4_VIDEO_MAX_BYTES) {
            ESP_LOGW(TAG, "Video buffer full after %" PRIu32 " frame(s), stopping recording early", frame_count);
            telegramp4_camera_release_frame(&frame);
            break;
        }
        memcpy(buf + written, frame.data, frame.len);
        written += frame.len;
        frame_count++;
        telegramp4_camera_release_frame(&frame);
    }

    telegramp4_camera_stop_video_mode();

    if (frame_count == 0) {
        free(buf);
        ESP_LOGE(TAG, "No frames captured");
        return ESP_FAIL;
    }

    out_result->data = buf;
    out_result->len = written;
    out_result->duration_s = (uint32_t) ((esp_timer_get_time() - start_us) / 1000000);
    ESP_LOGI(TAG, "Recorded %" PRIu32 " frames, %u bytes of H.264, %" PRIu32 "s",
              frame_count, (unsigned) written, out_result->duration_s);
    return ESP_OK;
}

void telegramp4_video_release(telegramp4_video_result_t *result)
{
    if (result && result->data) {
        free(result->data);
        result->data = NULL;
        result->len = 0;
    }
}
