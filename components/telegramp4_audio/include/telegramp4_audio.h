/**
 * telegramp4_audio — microphone capture abstraction (Phase 11).
 *
 * ⚠️ Hardware verification status: the FireBeetle 2 ESP32-P4's onboard
 * microphone interface (I2S vs PDM, exact pins) was not verified against live
 * documentation while writing this code. telegramp4_audio_record() is an
 * honest stub returning ESP_ERR_NOT_SUPPORTED until verified — see
 * docs/hardware.md. The output format (WAV/PCM vs. something else) must also
 * be picked based on what's actually practical on this hardware and accepted
 * by Telegram, not assumed.
 */
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char     path[160];
    uint32_t duration_s;
    size_t   size_bytes;
} telegramp4_audio_result_t;

/**
 * Records for `duration_s` seconds and saves to /sdcard/audio/ as a WAV file
 * (PCM is the simplest robust format to start with - see header note on
 * verifying this against the actual mic hardware). Blocks for the duration of
 * the recording; call from a dedicated task, never the Telegram poll task.
 */
esp_err_t telegramp4_audio_record(uint32_t duration_s, telegramp4_audio_result_t *out_result);

#ifdef __cplusplus
}
#endif
