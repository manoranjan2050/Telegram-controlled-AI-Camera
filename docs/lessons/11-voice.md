# Lesson 11 — Receive Telegram Voice

## Goal

Handle incoming Telegram voice messages: download, save, acknowledge with
duration — no decoding or speech recognition yet.

## What you learn

- Reusing Phase 10's generic download infrastructure for a second media type
  by adding a `duration_s` parameter to the shared
  `telegramp4_media_received_cb_t` callback type (0 for photos, real for voice)
- Saving Telegram voice notes as-is: they arrive as OGG/Opus, and this phase
  deliberately does not transcode or inspect the audio — just persist it

## Code

- [`components/telegramp4_telegram/telegramp4_telegram.c`](../../components/telegramp4_telegram/telegramp4_telegram.c):
  `handle_incoming_voice()` now also reads the `duration` field from Telegram's
  `Voice` object and passes it through
- [`main/app_main.cpp`](../../main/app_main.cpp): `on_voice_received()` —
  downloads (same size-capped path as photos), saves to
  `/sdcard/received/voice_<timestamp>.ogg`, replies with duration and filename

## How it works

`on_voice_received()` is registered via
`telegramp4_telegram_set_voice_received_handler()` exactly like the Phase 10
photo handler — same size check, same `telegramp4_storage_sanitize_path()`
call, same `/sdcard/received/` destination. The only real difference is the
reply format (includes duration) and the file extension (`.ogg`, matching
Telegram's actual voice-note container format rather than assuming a generic
extension).

Per the master spec, this phase intentionally stops at "save and acknowledge" —
speech-to-text comes in Phase 12 (build order) / lesson 13, as its own
swappable module, not bolted onto this download path.

## Test

```bash
idf.py build
idf.py -p <PORT> flash monitor
```
Send a voice message to your bot. Expected:
```
🎙 Voice message received.

Duration: 7 sec
Saved:
received/voice_1725660300.ogg
```

## Next lesson

[Lesson 12 — Voice command processing](12-ai.md) (speech-to-text arrives
alongside the AI lessons in the build order — see docs/PHASES.md Phase 13).
