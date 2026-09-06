#include "telegramp4_audio.h"
#include "esp_log.h"

static const char *TAG = "TAG_AUDIO";

esp_err_t telegramp4_audio_record(uint32_t duration_s, telegramp4_audio_result_t *out_result)
{
    (void) duration_s;
    (void) out_result;
    /*
     * TODO (hardware verification required, see header comment and
     * docs/hardware.md): bring up the actual microphone (I2S/PDM, whichever
     * this board uses) and write PCM samples to a WAV file here. Confirm the
     * mic interface against DFRobot's documentation before implementing.
     */
    ESP_LOGE(TAG, "Microphone hardware not yet verified for this board - see docs/hardware.md");
    return ESP_ERR_NOT_SUPPORTED;
}
