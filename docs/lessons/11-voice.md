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

## Phase 13 update — voice commands

Speech-to-text is added on top, as its own swappable component,
`telegramp4_stt`, with one function: `telegramp4_stt_transcribe()`. Per the
master spec ("make STT provider modular"), neither `telegramp4_telegram` nor
`app_main.cpp`'s voice handler know which backend answers this call. The only
provider implemented is OpenAI's audio transcription API (`whisper-1`) — a
real, testable HTTPS multipart integration (unlike the camera/audio/video
hardware stubs), disabled by default (`TELEGRAMP4_STT_ENABLED=n` in Kconfig)
since it needs your own API key and has cost/latency implications. A local
on-device model would slot in behind the same interface later.

After Phase 12's save-and-acknowledge runs, `on_voice_received()` transcribes
the voice bytes already in memory. On success, `parse_voice_command()` does
simple keyword matching ("photo"/"picture" → `/photo`, "video" → `/video`,
etc.) and — on a match — replies:
```
Speech recognized:
"take a photo"

Command:
photo

Executing...
```
then calls the new `telegramp4_telegram_dispatch(chat_id, "/photo")`, a public
wrapper around the same internal `dispatch_command()` every typed command and
button already goes through — never a separate execution path. No match just
reports the recognized text. If STT is disabled or the request fails, nothing
else happens; Phase 12's behavior is unaffected.

Test: enable STT and set an API key in `idf.py menuconfig`, then send a voice
message saying "take a photo".

## Next lesson

[Lesson 12 — AI object detection](12-ai.md)
