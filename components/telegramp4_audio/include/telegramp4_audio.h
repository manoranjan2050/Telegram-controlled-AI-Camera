/**
 * telegramp4_audio — microphone capture abstraction (Phase 11).
 *
 * ✅ Confirmed 2026-09-09 from the DFRobot DFR1172 schematic (page 6, `U4`):
 * onboard mic is an MSM261DGT003 PDM (Pulse Density Modulation) microphone,
 * with I2S_CLK/I2S_DATA traced to GPIO12/GPIO9 respectively - see
 * docs/hardware.md. Implemented using ESP-IDF's I2S driver in PDM RX mode
 * (`driver/i2s_pdm.h`), 16-bit mono PCM, wrapped in a standard WAV header.
 *
 * The result is always returned in a heap buffer (like
 * telegramp4_camera_frame_t) rather than requiring the SD card, since SD is
 * not reliably working on this board yet (see docs/hardware.md) and a voice
 * feature that only works when SD happens to mount isn't a working feature.
 * A copy is still saved to /sdcard/audio/ when storage is mounted, same
 * best-effort pattern as photo capture in main/app_main.cpp.
 */
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *data; /* WAV bytes (header + PCM), heap-allocated - caller frees via telegramp4_audio_release() */
    size_t   len;
    uint32_t duration_s;
} telegramp4_audio_result_t;

/**
 * Records for `duration_s` seconds from the onboard PDM microphone and
 * returns a complete WAV file in `out_result->data`. Blocks for the
 * duration of the recording; call from a dedicated task, never the Telegram
 * poll task.
 */
esp_err_t telegramp4_audio_record(uint32_t duration_s, telegramp4_audio_result_t *out_result);

void telegramp4_audio_release(telegramp4_audio_result_t *result);

#ifdef __cplusplus
}
#endif
