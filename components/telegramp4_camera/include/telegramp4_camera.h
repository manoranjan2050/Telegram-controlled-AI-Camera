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

#ifdef __cplusplus
}
#endif
