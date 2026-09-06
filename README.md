# TelegramP4

**Control an ESP32-P4 AI Camera from Telegram.**

> ⚠️ Status: all 22 build phases are implemented and the firmware
> **build-verifies cleanly** with ESP-IDF v5.4.1 targeting `esp32p4`. It has
> **not been tested on real hardware** in this development session — several
> subsystems (camera, video, microphone, AI model, PIR GPIO) are honest stubs
> pending hardware verification; see [docs/hardware.md](docs/hardware.md) for
> exactly what's confirmed vs. still unverified, and
> [docs/lessons/16-final-project.md](docs/lessons/16-final-project.md) for a
> full status check-in against the original success criteria. See
> [docs/PHASES.md](docs/PHASES.md) for the phase-by-phase build plan and
> [CLAUDE.md](CLAUDE.md) for the live progress checklist.

## 1. What is TelegramP4?

TelegramP4 turns a DFRobot FireBeetle 2 ESP32-P4 AI Vision Board into a
Telegram-controlled smart camera: take photos, record video, run on-device AI object
detection, get motion alerts, and control GPIOs — all from a Telegram chat. It's built
as an incremental, beginner-friendly learning platform: each feature is its own
lesson and its own firmware phase, so you can follow along from "hello world" to a
full AI security camera.

## 2. Features

All implemented and build-verified; ⚠️ marks features whose hardware driver
is still an honest stub pending verification on a real board (see
[docs/hardware.md](docs/hardware.md)):

- Wi-Fi connectivity with auto-reconnect (via `esp_wifi_remote` + the onboard
  ESP32-C6, since ESP32-P4 has no native WiFi radio)
- Telegram bot control: commands + inline buttons, chat-ID whitelist
- ⚠️ Photo capture and delivery to Telegram
- ⚠️ Video recording and delivery
- MicroSD storage with a photo gallery (`[View][Download][Delete]`)
- Receiving photos and voice messages from Telegram
- ⚠️ Audio recording and playback delivery
- Voice-command control (speech-to-text → camera/action), modular STT
  provider (OpenAI Whisper implemented, disabled by default)
- ⚠️ On-device AI object detection (ESP32-P4 edge AI)
- ⚠️ PIR motion detection with AI-filtered Telegram alerts
- Whitelisted GPIO control (LEDs, relays, buzzers, sensors)
- ⚠️ Optional MIPI-DSI display with live status/preview
- OTA firmware updates (`/ota <url>`, opt-in)
- Full `/status` dashboard and `/diagnostics`
- Confirmation prompts (`[Yes]/[Cancel]`) for `/delete` and `/reboot`
- TLS-only Telegram/OTA traffic (never disabled), no secrets in Git

## 3. Hardware

- **DFRobot FireBeetle 2 ESP32-P4 AI Vision Board** — https://www.dfrobot.com/product-2915.html
  - ESP32-P4 main processor
  - ESP32-C6 connectivity co-processor (Wi-Fi/BLE)
  - MIPI-CSI camera
  - MicroSD slot
  - Onboard microphone
  - Optional MIPI-DSI display
- Optional: PIR motion sensor, relay/LED/buzzer for GPIO demos

See [docs/hardware.md](docs/hardware.md) for verified pinouts and specs (filled in
during Phase 0/5/7/11/17/19/20 as each subsystem is verified against real
documentation).

## 4. Wiring

Covered per-lesson in [docs/lessons/](docs/lessons/) as each peripheral is added.
No generic wiring diagram yet — the camera/SD/mic are onboard; PIR/relay/LED wiring
is documented in the Phase 17/19 lessons.

## 5. Software requirements

- [ESP-IDF](https://github.com/espressif/esp-idf) (version targeting `esp32p4`
  support — confirmed during Phase 0)
- Python 3 (installed by the ESP-IDF installer)
- A Telegram account and bot token (see below)
- Git

## 6. Install ESP-IDF

Follow the official Espressif "Get Started" guide for your OS, then confirm:

```bash
idf.py --version
idf.py --list-targets
```

`esp32p4` must appear in the target list.

## 7. Create a Telegram bot

See [docs/telegram-bot-setup.md](docs/telegram-bot-setup.md).

## 8. Configure Wi-Fi

```bash
idf.py menuconfig
```

Set your SSID/password under `TelegramP4 Configuration → WiFi`. This is stored in
your local `sdkconfig`, which is gitignored — never commit it.

## 9. Configure the Telegram token

Set your bot token and allowed chat ID(s) under
`TelegramP4 Configuration → Telegram` in `idf.py menuconfig`. See
[docs/configuration.md](docs/configuration.md) for every option.

## 10. Build

```bash
idf.py set-target esp32p4
idf.py build
```

## 11. Flash

```bash
idf.py -p <PORT> flash
```

## 12. First command

```bash
idf.py -p <PORT> monitor
```

Then in Telegram, send `/start` to your bot.

## 13. Camera setup

See [docs/lessons/05-camera.md](docs/lessons/05-camera.md) (added in Phase 5).

## 14. AI setup

See [docs/lessons/15-ai.md](docs/lessons/15-ai.md) (added in Phase 14).

## 15. Troubleshooting

See [docs/troubleshooting.md](docs/troubleshooting.md).

## 16. Architecture

See [docs/architecture.md](docs/architecture.md) for the full module diagram.

```mermaid
flowchart TD
    TG[Telegram Cloud] -- HTTPS / Bot API --> TM[Telegram Manager]
    TM --> Bus[Command / Event Bus]
    Bus --> CM[Camera Manager]
    Bus --> AM[Audio Manager]
    Bus --> AIM[AI Manager]
    CM --> SM[Storage Manager]
    AM --> SM
    AIM --> SM
    SM --> SD[(MicroSD)]
```

## 17. Learning roadmap

22 lessons, one per firmware phase — see [docs/PHASES.md](docs/PHASES.md) for the
build plan and [docs/lessons/](docs/lessons/) for the write-ups as they're published.

## 18. Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## 19. License

MIT — see [LICENSE](LICENSE).
