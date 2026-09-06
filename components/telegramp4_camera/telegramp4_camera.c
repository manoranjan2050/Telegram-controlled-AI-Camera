#include "telegramp4_camera.h"
#include "esp_log.h"

static const char *TAG = "TAG_CAMERA";
static bool s_initialized = false;

esp_err_t telegramp4_camera_init(void)
{
    /*
     * TODO (hardware verification required, see header comment and
     * docs/hardware.md): bring up the actual MIPI-CSI sensor here using
     * whichever Espressif component matches the confirmed sensor
     * (esp32-camera or esp-video/esp_cam_sensor). Do not fill this in from
     * guesswork - confirm sensor model and driver against DFRobot's
     * documentation/schematic and the sensor's own datasheet first.
     */
    ESP_LOGE(TAG, "Camera hardware driver not yet verified for this board - see docs/hardware.md");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t telegramp4_camera_deinit(void)
{
    s_initialized = false;
    return ESP_OK;
}

esp_err_t telegramp4_camera_capture(telegramp4_camera_frame_t *out_frame)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Camera not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    /* Unreachable until telegramp4_camera_init() actually succeeds. */
    return ESP_ERR_NOT_SUPPORTED;
}

void telegramp4_camera_release_frame(telegramp4_camera_frame_t *frame)
{
    if (frame) {
        frame->data = NULL;
        frame->len = 0;
    }
}

telegramp4_camera_status_t telegramp4_camera_get_status(void)
{
    telegramp4_camera_status_t status = {
        .initialized = s_initialized,
        .frame_width = 0,
        .frame_height = 0,
    };
    return status;
}
