# Getting Started

This is the fast path from a fresh clone to a working `/start`. For deeper detail on
any step, follow the linked doc.

1. **Install ESP-IDF** with `esp32p4` target support. Confirm:
   ```bash
   idf.py --version
   idf.py --list-targets
   ```
2. **Clone this repository** and open a terminal in it with the ESP-IDF environment
   activated.
3. **Create your Telegram bot** — see [telegram-bot-setup.md](telegram-bot-setup.md).
4. **Configure the project**:
   ```bash
   idf.py set-target esp32p4
   idf.py menuconfig
   ```
   Set Wi-Fi SSID/password, bot token, and allowed chat ID(s) under
   `TelegramP4 Configuration`. Full option reference: [configuration.md](configuration.md).
5. **Build**:
   ```bash
   idf.py build
   ```
6. **Flash and monitor**:
   ```bash
   idf.py -p <PORT> flash monitor
   ```
7. **Talk to your bot**: send `/start`, then `/status`, then `/help`.

If something doesn't work, check [troubleshooting.md](troubleshooting.md) before
opening an issue.

## Where to go next

- [architecture.md](architecture.md) — how the firmware is organized
- [PHASES.md](PHASES.md) — the full incremental build plan, phase by phase
- [lessons/](lessons/) — one write-up per phase, written as each phase lands
