# Lesson 09 — Receive Photo from Telegram

## Goal

Let a user send a photo *to* the bot: download it via `getFile`, save it to
`/sdcard/received/`, and reply with the file name and size.

## What you learn

- Telegram's two-step file download: `getFile` (returns a `file_path` and
  declared size) then a separate `https://api.telegram.org/file/bot<token>/...`
  request for the actual bytes
- Enforcing a max download size *before* downloading (checking `getFile`'s
  declared `file_size`) and *during* download (the HTTP buffer itself refuses
  to grow past the cap) — two layers, since a malicious/misbehaving server
  could lie about the declared size
- Decoupling "a photo arrived" from what to do about it: `telegramp4_telegram`
  knows nothing about storage; it just calls a registered callback

## Code

- [`components/telegramp4_telegram/`](../../components/telegramp4_telegram/):
  - `http_get_ex()` — generalized `http_get` with a caller-specified size cap,
    used for both small JSON responses and larger file downloads
  - `telegramp4_telegram_download_file()` — `getFile` + size check + download
  - `telegramp4_telegram_set_photo_received_handler()` /
    `..._set_voice_received_handler()` — registration for Phase 10/12
  - `process_update()` now also recognizes `message.photo` (an array of
    `PhotoSize`, smallest to largest — we take the last/largest) and
    `message.voice`
- [`main/app_main.cpp`](../../main/app_main.cpp):
  - `on_photo_received()` — the registered callback: checks size, downloads,
    saves to `/sdcard/received/`, replies
  - `/photo_info`, `/photo_files`

## How it works

Messages that aren't slash-commands used to be ignored entirely. Now
`process_update()` checks for a `photo` or `voice` key on non-command messages
and, if a handler is registered, calls it with the chat ID, the largest
file_id, and Telegram's declared file size. `on_photo_received()` first checks
that size against `TELEGRAMP4_STORAGE_MAX_DOWNLOAD_SIZE_KB` — if Telegram
declared a size we won't accept, we never even call `getFile`. If it downloads,
the max-size cap is enforced a second time by `http_get_ex()`'s buffer growth
check, so a size lie in `getFile`'s response can't cause an unbounded download.

Note that unauthorized chats are now filtered centrally in `process_update()`
(not just in `dispatch_command()`), so a stranger sending a photo doesn't
trigger a download at all, let alone a reply.

## Test

```bash
idf.py build
idf.py -p <PORT> flash monitor
```
Send a photo to your bot. Expected:
```
📥 Image received.

File:
received_1725660300.jpg

Size:
1.4 MB
```
Then `/photo_info` and `/photo_files` to confirm it's tracked.

## Next lesson

[Lesson 10 — Audio recording](10-audio.md)
