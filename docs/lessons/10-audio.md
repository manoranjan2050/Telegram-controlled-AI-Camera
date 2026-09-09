# Lesson 10 — Microphone / Audio Recording

## Goal

Add `/record [seconds]`, recording real microphone audio and uploading it to
Telegram as a WAV file.

## ✅ Resolved 2026-09-09: real PDM mic capture confirmed working live

The DFRobot DFR1172 schematic (page 6, `U4`) confirmed the onboard mic is an
**MSM261DGT003 PDM microphone**, with its I2S_CLK/I2S_DATA nets traced to
**GPIO12 (CLK) / GPIO9 (DATA)**. `telegramp4_audio_record()` now drives these
via ESP-IDF's I2S driver in PDM RX mode (`driver/i2s_pdm.h`, 16-bit mono),
building a standard WAV file (44-byte RIFF header + raw PCM) entirely in a
heap buffer.

**Confirmed live** via a temporary diagnostic build: a `/record` call
produced a real `Recorded 320000 bytes of PCM (10s @ 16000Hz)` and returned
`ESP_OK` — genuine microphone data, not a stub.

### Why the result comes back in memory, not from an SD path

The original design (see the header comment history) had
`telegramp4_audio_record()` write straight to `/sdcard/audio/` and hand back
just a path, mirroring what seemed like the simplest approach. But the SD
card doesn't reliably mount on this board yet (see docs/hardware.md) — a
voice feature that only works when SD happens to be up isn't a working
feature. `telegramp4_audio_result_t` now carries the WAV bytes directly
(`data`/`len`, exactly like `telegramp4_camera_frame_t`), so `/record`
works regardless of SD state; a copy is still saved to `/sdcard/audio/`
when storage is mounted (best-effort, the same pattern already used for
`/photo`).

## What you learn

- ESP-IDF's I2S PDM RX API (`i2s_new_channel` → `i2s_channel_init_pdm_rx_mode`
  → `i2s_channel_enable` → `i2s_channel_read`) — a different driver family
  from I2S standard mode, fixed to 16-bit samples
- Building a minimal WAV/RIFF header by hand (44 bytes, no library needed)
- Returning captured media in a heap buffer rather than coupling a component
  to storage being available — the same lesson `telegramp4_camera` already
  applied to photos, now applied consistently to audio too

## Code

- [`components/telegramp4_audio/`](../../components/telegramp4_audio/) —
  `telegramp4_audio_record()` / `telegramp4_audio_release()`, real I2S PDM
  implementation
- [`main/Kconfig.projbuild`](../../main/Kconfig.projbuild) — `Audio` submenu:
  min/default/max duration (1/10/60s) plus the confirmed PDM CLK/DATA pins
  and sample rate
- [`main/app_main.cpp`](../../main/app_main.cpp) — `handler_record()` /
  `audio_record_task()`: sends the recording-started message, records,
  best-effort saves to SD, then uploads the WAV as a document

## Test

```bash
idf.py build
idf.py -p <PORT> flash monitor
```
`/record 5` in Telegram. Expect `🎙 Recording 5 seconds...` followed a few
seconds later by a real `audio.wav` document in the chat.

## Next lesson

[Lesson 11 — Receive Telegram voice](11-voice.md)
