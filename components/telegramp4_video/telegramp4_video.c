#include "telegramp4_video.h"
#include "esp_log.h"

static const char *TAG = "TAG_VIDEO";

esp_err_t telegramp4_video_record(uint32_t duration_s, telegramp4_video_result_t *out_result)
{
    (void) duration_s;
    (void) out_result;
    /*
     * TODO (hardware verification required, see header comment and
     * docs/hardware.md): implement using whichever Espressif video-encode
     * path is confirmed to work with the verified camera driver from Phase 5.
     * Do not assume H.264 or any specific format/throughput without testing
     * on real hardware first.
     */
    ESP_LOGE(TAG, "Video recording not yet verified for this board - see docs/hardware.md");
    return ESP_ERR_NOT_SUPPORTED;
}
