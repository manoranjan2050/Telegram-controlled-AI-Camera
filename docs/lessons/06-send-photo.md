# Lesson 06 — Send Photo to Telegram

## Goal

Implement the real `/photo` command: capture → upload via Telegram's `sendPhoto`.

## ⚠️ Still blocked on Phase 5 hardware verification

The upload path implemented here is real, generic HTTP/multipart code that
doesn't depend on any board specifics — but until the camera driver from
[Lesson 05](05-camera.md) is verified on real hardware, `/photo` will report
`❌ Camera unavailable.` rather than a captured image, because
`telegramp4_camera_capture()` still returns an error. That's the correct,
honest behavior for now — the upload machinery is ready and waiting.

## What you learn

- Building a `multipart/form-data` HTTP body by hand with `esp_http_client`
  (`esp_http_client_open` + `esp_http_client_write` in pieces) instead of
  buffering the whole request into one blob
- Streaming the JPEG bytes directly from the camera frame buffer into the HTTP
  body, so memory usage doesn't double
- A simple retry-once pattern for upload failures

## Code

- [`components/telegramp4_telegram/telegramp4_telegram.c`](../../components/telegramp4_telegram/telegramp4_telegram.c):
  `send_photo_once()` / `telegramp4_telegram_send_photo()`
- [`main/app_main.cpp`](../../main/app_main.cpp): `handler_photo()` — the real
  `/photo` command, replacing the Phase 4 stub

## How it works

Telegram's `sendPhoto` expects a `multipart/form-data` POST with a `chat_id`
field and a `photo` file field. `send_photo_once()` builds the boundary text
before and after the image bytes as two small stack buffers (`part1`, `part2`,
`part3`), computes the total `Content-Length` up front, opens the HTTP
connection with that length, and writes the three text pieces and the raw JPEG
bytes as separate `esp_http_client_write()` calls — the JPEG data itself is
never copied into a second buffer. `telegramp4_telegram_send_photo()` wraps this
with one retry if the first attempt fails (network hiccup, Telegram-side
timeout, etc.).

`handler_photo()` in `app_main.cpp` captures a frame, uploads it, and always
releases the frame afterward (even on upload failure) so no camera buffer is
ever leaked.

## Test

```bash
idf.py build
idf.py -p <PORT> flash monitor
```
`/photo` in Telegram. Expected today: `❌ Camera unavailable.` Once Phase 5's
driver is filled in and verified, expected: an actual photo arrives in the chat.

## Next lesson

[Lesson 07 — MicroSD storage](07-microsd.md)
