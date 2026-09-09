#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "telegramp4_audio.h"
#include "driver/i2s_pdm.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "TAG_AUDIO";

#define WAV_HEADER_SIZE 44
#define BITS_PER_SAMPLE 16
#define NUM_CHANNELS    1

/* Little-endian standard WAV/RIFF header for 16-bit PCM mono. */
static void write_wav_header(uint8_t *hdr, uint32_t sample_rate, uint32_t data_bytes)
{
    uint32_t byte_rate = sample_rate * NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    uint16_t block_align = NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    uint32_t riff_size = 36 + data_bytes;

    memcpy(hdr + 0, "RIFF", 4);
    memcpy(hdr + 4, &riff_size, 4);
    memcpy(hdr + 8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    uint32_t fmt_size = 16;
    memcpy(hdr + 16, &fmt_size, 4);
    uint16_t audio_format = 1; /* PCM */
    memcpy(hdr + 20, &audio_format, 2);
    uint16_t num_channels = NUM_CHANNELS;
    memcpy(hdr + 22, &num_channels, 2);
    memcpy(hdr + 24, &sample_rate, 4);
    memcpy(hdr + 28, &byte_rate, 4);
    memcpy(hdr + 32, &block_align, 2);
    uint16_t bits_per_sample = BITS_PER_SAMPLE;
    memcpy(hdr + 34, &bits_per_sample, 2);
    memcpy(hdr + 36, "data", 4);
    memcpy(hdr + 40, &data_bytes, 4);
}

esp_err_t telegramp4_audio_record(uint32_t duration_s, telegramp4_audio_result_t *out_result)
{
    if (!out_result) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out_result, 0, sizeof(*out_result));

    const uint32_t sample_rate = CONFIG_TELEGRAMP4_AUDIO_SAMPLE_RATE;
    const size_t data_bytes = (size_t) sample_rate * (BITS_PER_SAMPLE / 8) * duration_s;

    uint8_t *wav = (uint8_t *) malloc(WAV_HEADER_SIZE + data_bytes);
    if (!wav) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes for WAV buffer", (unsigned) (WAV_HEADER_SIZE + data_bytes));
        return ESP_ERR_NO_MEM;
    }
    write_wav_header(wav, sample_rate, (uint32_t) data_bytes);

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    i2s_chan_handle_t rx_handle;
    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &rx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        free(wav);
        return err;
    }

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = (gpio_num_t) CONFIG_TELEGRAMP4_AUDIO_PDM_CLK_PIN,
            .din = (gpio_num_t) CONFIG_TELEGRAMP4_AUDIO_PDM_DATA_PIN,
            .invert_flags = { .clk_inv = false },
        },
    };
    err = i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_pdm_rx_mode failed: %s (check PDM CLK/DATA pins in docs/hardware.md)",
                  esp_err_to_name(err));
        i2s_del_channel(rx_handle);
        free(wav);
        return err;
    }

    err = i2s_channel_enable(rx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(err));
        i2s_del_channel(rx_handle);
        free(wav);
        return err;
    }

    uint8_t *pcm = wav + WAV_HEADER_SIZE;
    size_t total_read = 0;
    while (total_read < data_bytes) {
        size_t bytes_read = 0;
        err = i2s_channel_read(rx_handle, pcm + total_read, data_bytes - total_read, &bytes_read, 2000);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2s_channel_read failed: %s", esp_err_to_name(err));
            break;
        }
        total_read += bytes_read;
    }

    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);

    if (total_read == 0) {
        free(wav);
        return ESP_FAIL;
    }
    if (total_read < data_bytes) {
        /* Partial capture (e.g. a read timeout) - still return what we got,
         * with the WAV header corrected to match. */
        write_wav_header(wav, sample_rate, (uint32_t) total_read);
    }

    out_result->data = wav;
    out_result->len = WAV_HEADER_SIZE + total_read;
    out_result->duration_s = duration_s;
    ESP_LOGI(TAG, "Recorded %u bytes of PCM (%" PRIu32 "s @ %" PRIu32 "Hz)",
              (unsigned) total_read, duration_s, sample_rate);
    return ESP_OK;
}

void telegramp4_audio_release(telegramp4_audio_result_t *result)
{
    if (result && result->data) {
        free(result->data);
        result->data = NULL;
        result->len = 0;
    }
}
