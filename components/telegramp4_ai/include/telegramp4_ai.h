/**
 * telegramp4_ai — on-device AI object detection abstraction (Phase 14).
 *
 * ⚠️ Hardware/model verification status: this needs a real decision about
 * which inference framework/model to use (most likely Espressif's ESP-DL with
 * a small quantized detection model), which in turn needs the Phase 5 camera
 * pipeline working so images can actually be fed in, and real measurement of
 * inference time/PSRAM footprint on hardware. None of that has been done in
 * this session - do not claim a model works until it's tested on hardware.
 * telegramp4_ai_process_image() is an honest stub returning
 * ESP_ERR_NOT_SUPPORTED.
 *
 * Kept out of the critical path: AI is compile/config-time optional
 * (TELEGRAMP4_AI_ENABLED) so the rest of the firmware works without it.
 */
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TELEGRAMP4_AI_MAX_DETECTIONS 8

typedef struct {
    char    label[24];
    uint8_t confidence_pct; /* 0-100 */
} telegramp4_ai_detection_t;

typedef struct {
    telegramp4_ai_detection_t detections[TELEGRAMP4_AI_MAX_DETECTIONS];
    int      count;
    uint32_t inference_time_ms;
} telegramp4_ai_result_t;

esp_err_t telegramp4_ai_init(void);
esp_err_t telegramp4_ai_deinit(void);

/** Runs detection on a decoded image (RGB/JPEG - format TBD once a model is chosen). */
esp_err_t telegramp4_ai_process_image(const uint8_t *image_data, size_t len, telegramp4_ai_result_t *out_result);

bool telegramp4_ai_is_enabled(void);

#ifdef __cplusplus
}
#endif
