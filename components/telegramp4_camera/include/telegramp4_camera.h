/**
 * telegramp4_camera — camera abstraction (Phase 5).
 *
 * Sensor: OV5647 (identified 2026-09-07 as the sensor on a physically
 * installed Raspberry Pi Camera Module v1.3), driven via Espressif's
 * esp_video (V4L2-style API) + esp_cam_sensor components over MIPI-CSI.
 * Officially supported on ESP32-P4 per esp_cam_sensor's own documentation.
 *
 * ⚠️ Still unverified for this specific board: the SCCB (I2C) pins, sensor
 * reset/power-down pins, and XCLK pin/source this DFRobot board's CSI
 * connector actually wires to the sensor. These are exposed as Kconfig
 * options (see the `Camera` submenu) defaulting to the same placeholder
 * values Espressif's own "customized development board" example uses
 * (SCL=8, SDA=7, reset/pwdn/xclk unset) — not blind guesses, but not
 * confirmed against this board's schematic either. If `telegramp4_camera_init()`
 * fails, this pin mapping is the first thing to check (a failed SCCB I2C
 * transaction to the sensor is the most likely cause) — see docs/hardware.md.
 */
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *data; /* JPEG bytes, heap-allocated - caller frees via telegramp4_camera_release_frame() */
    size_t   len;
} telegramp4_camera_frame_t;

typedef struct {
    bool initialized;
    int  frame_width;
    int  frame_height;
} telegramp4_camera_status_t;

esp_err_t telegramp4_camera_init(void);
esp_err_t telegramp4_camera_deinit(void);

/** Captures one JPEG frame. Caller must call telegramp4_camera_release_frame(). */
esp_err_t telegramp4_camera_capture(telegramp4_camera_frame_t *out_frame);
void telegramp4_camera_release_frame(telegramp4_camera_frame_t *frame);

telegramp4_camera_status_t telegramp4_camera_get_status(void);

/**
 * Video mode (Phase 9). The MIPI-CSI capture engine can only run one pixel
 * format/pipeline at a time, so video and photo modes cannot be active
 * simultaneously: telegramp4_camera_start_video_mode() tears down the
 * photo/JPEG pipeline (if running) and configures the capture device for
 * raw YUV420 output feeding the ESP32-P4's hardware H.264 encoder
 * (`espressif/esp_video`'s `/dev/video11` M2M device - the same driver
 * family as the JPEG path, just a different codec device and a different
 * ISP output color format). telegramp4_camera_stop_video_mode() tears the
 * video pipeline back down and restores photo mode automatically, so
 * `/photo` works again immediately after a `/video` recording finishes.
 */
esp_err_t telegramp4_camera_start_video_mode(uint32_t bitrate_bps);

/**
 * Captures one encoded H.264 access unit (Annex-B NAL unit(s) with start
 * codes - typically one video frame's worth of data). Caller must call
 * telegramp4_camera_release_frame() on the result, same as
 * telegramp4_camera_capture(). Only valid between
 * telegramp4_camera_start_video_mode() and telegramp4_camera_stop_video_mode().
 */
esp_err_t telegramp4_camera_read_video_frame(telegramp4_camera_frame_t *out_frame);

/** Stops video mode and restores the photo/JPEG pipeline. */
esp_err_t telegramp4_camera_stop_video_mode(void);

#ifdef __cplusplus
}
#endif
