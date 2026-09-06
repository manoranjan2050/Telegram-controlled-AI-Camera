/**
 * telegramp4_stt — speech-to-text provider abstraction (Phase 13).
 *
 * Deliberately NOT hardcoded into telegramp4_telegram (per spec: "make STT
 * provider modular"). The only concrete provider implemented so far is a
 * cloud one (OpenAI's audio transcription API) - this needs an API key you
 * supply yourself and has real cost/latency implications; it is disabled by
 * default (TELEGRAMP4_STT_ENABLED=n in Kconfig). A local on-device model
 * (e.g. via esp-sr) is future work and would slot in behind this same
 * interface without changing any caller.
 *
 * This is a genuine, testable HTTP integration (unlike the camera/audio/video
 * stubs) — it will actually work once a valid API key is configured. It has
 * simply not been tested against a real key/voice sample in this session.
 */
#pragma once

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Transcribes `audio_data`/`len` (OGG/Opus, as saved by Phase 12) to text.
 * Returns ESP_ERR_NOT_SUPPORTED if STT is disabled in Kconfig,
 * ESP_ERR_INVALID_STATE if enabled but no API key is configured.
 */
esp_err_t telegramp4_stt_transcribe(const uint8_t *audio_data, size_t len, char *out_text, size_t out_text_len);

#ifdef __cplusplus
}
#endif
