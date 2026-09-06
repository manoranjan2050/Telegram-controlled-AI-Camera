# Lesson 10 — Microphone / Audio Recording

## Goal

Add `/record [seconds]`, recording microphone audio to `/sdcard/audio/` and
uploading it to Telegram.

## ⚠️ Hardware verification status

The FireBeetle 2 ESP32-P4's microphone interface (I2S vs PDM, exact pins) was
not verified against live documentation while writing this code.
`telegramp4_audio_record()` is an honest stub returning
`ESP_ERR_NOT_SUPPORTED`. The output format is planned as WAV/PCM — "the
simplest robust format first," per the master spec — but even that choice
should be re-confirmed once the mic hardware is verified and Telegram's
accepted audio formats are checked against what this board can actually
produce.

## What you learn

- The same task-per-recording pattern as Phase 9 (`/video`), reused for audio
- Why a placeholder format decision (WAV/PCM) is stated explicitly rather than
  silently assumed, so it's easy to revisit once real hardware constraints are
  known

## Code

- [`components/telegramp4_audio/`](../../components/telegramp4_audio/) —
  `telegramp4_audio_record()` (stub)
- [`Kconfig.projbuild`](../../Kconfig.projbuild) — new `Audio` submenu: min/
  default/max duration (1/10/60s)
- [`main/app_main.cpp`](../../main/app_main.cpp) — `handler_record()` /
  `audio_record_task()`, mirroring the Phase 9 video pattern exactly

## Test

```bash
idf.py build
idf.py -p <PORT> flash monitor
```
`/record 5` in Telegram. Expected today: `🎙 Recording 5 seconds...` followed by
`❌ Microphone unavailable.\nCheck hardware.`

## Next lesson

[Lesson 11 — Receive Telegram voice](11-voice.md)
