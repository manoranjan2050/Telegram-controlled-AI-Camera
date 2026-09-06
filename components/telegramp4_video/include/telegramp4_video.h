/**
 * telegramp4_video — video recording abstraction (Phase 9).
 *
 * ⚠️ Hardware verification status: same caveat as telegramp4_camera. ESP32-P4
 * does have a hardware H.264 encoder block in silicon, but whether it's usable
 * here depends on the same unverified camera/sensor driver stack (Phase 5) and
 * on which Espressif video-encode component/example actually supports it on
 * this chip. Do not assume H.264 (or any format) works until it has been
 * tested end-to-end on real hardware - see spec rule "do not assume hardware
 * video encoding support without verification."
 *
 * telegramp4_video_record() is therefore an honest stub returning
 * ESP_ERR_NOT_SUPPORTED until someone verifies the encoder path and fills it
 * in, exactly like telegramp4_camera.
 */
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char     path[160];  /* where the recording was saved, on success */
    uint32_t duration_s;
    size_t   size_bytes;
} telegramp4_video_result_t;

/**
 * Records for `duration_s` seconds (clamped by the caller to the configured
 * min/max — see Kconfig) and saves to /sdcard/videos/. Blocks for the
 * duration of the recording; call from a dedicated task, never from the
 * Telegram poll task, so command handling isn't blocked (see
 * docs/architecture.md task model).
 */
esp_err_t telegramp4_video_record(uint32_t duration_s, telegramp4_video_result_t *out_result);

#ifdef __cplusplus
}
#endif
