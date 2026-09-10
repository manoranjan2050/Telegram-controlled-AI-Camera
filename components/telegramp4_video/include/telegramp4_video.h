/**
 * telegramp4_video — video recording, using the ESP32-P4's hardware H.264
 * encoder (Phase 9).
 *
 * ✅ Confirmed 2026-09-10: `espressif/esp_video` exposes the P4's hardware
 * H.264 encoder as a V4L2 M2M device (`/dev/video11`, same driver family as
 * the JPEG path telegramp4_camera already uses for photos), and the ISP's
 * own CSI capture format table lists packed YUV420 as a real supported
 * output color format - exactly what that H.264 device expects as input,
 * confirmed by reading its source rather than assumed. See
 * telegramp4_camera.h's video-mode functions and docs/lessons/09-video.md
 * for the full story, including why photo and video capture can't run at
 * the same time (one MIPI-CSI capture engine, one active pixel format).
 *
 * Output is a raw H.264 Annex-B elementary stream (start-coded NAL units,
 * SPS/PPS auto-prepended on the first frame) - not wrapped in an MP4
 * container. Delivered to Telegram as a document (`video.h264`), which
 * plays directly in VLC/ffplay; proper MP4 muxing is a possible follow-up.
 * Returned in a heap buffer (mirroring telegramp4_camera_frame_t /
 * telegramp4_audio_result_t) rather than requiring the SD card, since SD
 * isn't reliable enough on this board to depend on - see docs/hardware.md.
 */
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *data; /* H.264 Annex-B bytes, heap-allocated - caller frees via telegramp4_video_release() */
    size_t   len;
    uint32_t duration_s;
} telegramp4_video_result_t;

/**
 * Records for `duration_s` seconds (clamped by the caller to the configured
 * min/max - see Kconfig) and returns a raw H.264 elementary stream. Blocks
 * for the duration of the recording; call from a dedicated task, never from
 * the Telegram poll task, so command handling isn't blocked (see
 * docs/architecture.md task model).
 */
esp_err_t telegramp4_video_record(uint32_t duration_s, telegramp4_video_result_t *out_result);

void telegramp4_video_release(telegramp4_video_result_t *result);

#ifdef __cplusplus
}
#endif
