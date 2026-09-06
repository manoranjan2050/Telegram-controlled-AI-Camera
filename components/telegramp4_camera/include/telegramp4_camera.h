/**
 * telegramp4_camera — camera abstraction (Phase 5).
 *
 * IMPORTANT — hardware verification status: the exact MIPI-CSI camera sensor
 * shipped/recommended with the FireBeetle 2 ESP32-P4 AI Vision Board, and the
 * exact Espressif driver stack that supports it (esp32-camera vs the newer
 * esp-video/esp_cam_sensor V4L2-style stack — ESP32-P4's MIPI-CSI interface is
 * not the classic DVP interface older esp32-camera targets), was NOT verified
 * against live DFRobot/Espressif documentation before this code was written
 * (web access was unavailable in the development session). See
 * docs/hardware.md for what's confirmed vs. still TBD.
 *
 * This component therefore provides the real abstraction/interface and
 * Kconfig scaffolding, but telegramp4_camera_init()/capture() return
 * ESP_ERR_NOT_SUPPORTED with a clear log message until someone with the board
 * verifies the sensor/driver and fills in telegramp4_camera.c accordingly.
 * DO NOT report a successful photo capture until this has actually been done
 * and tested on hardware.
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
    uint8_t *data; /* JPEG bytes */
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
