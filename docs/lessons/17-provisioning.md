# Lesson 17 — Web Setup Portal (First-Time Provisioning)

## Goal

Let someone flash the firmware once with no WiFi/Telegram credentials baked
in, then configure the device entirely from a phone or laptop browser —
no `idf.py menuconfig`, no rebuild, no serial connection required.

## What was built

`telegramp4_provisioning` — a new component that:

- On boot, checks NVS (namespace `tp4cfg`) for a saved WiFi SSID and
  Telegram bot token. If both are present, the device boots normally.
- If nothing is saved, it starts a SoftAP (`TelegramP4-Setup`, open) and a
  web server at `http://192.168.4.1/` serving a simple form: WiFi SSID,
  WiFi password, Telegram bot token, and chat ID(s).
- Submitting the form saves the values to NVS and reboots the device
  (`esp_restart()`) — it then boots normally, connecting to the WiFi
  network and Telegram bot the user just configured.
- Holding the **BOOT button** (GPIO35, confirmed in DFRobot's own spec
  table) while powering on forces re-entry into the portal even if a
  configuration is already saved — the only way to reconfigure without a
  full reflash, since there's no live "reconfigure" command (if you're
  changing the bot token, you can't rely on the old one still working to
  receive that command).

## Backward compatibility with the Kconfig workflow

Developers who prefer the existing `idf.py menuconfig` workflow (baking
real credentials into `sdkconfig` for bench/CI testing, as this project's
own test device does) are unaffected: `telegramp4_provisioning_load()`
checks NVS first, then falls back to Kconfig's
`CONFIG_TELEGRAMP4_WIFI_SSID`/`CONFIG_TELEGRAMP4_TELEGRAM_BOT_TOKEN` if
they're set to real (non-placeholder) values. The portal only appears on a
genuinely unconfigured device — neither NVS nor Kconfig has anything.

## Retrofitting existing components

`telegramp4_wifi_init()`, `telegramp4_telegram_start()`, and
`telegramp4_security_configure()` all changed from reading Kconfig macros
directly to taking the SSID/password/bot-token/chat-IDs as parameters,
sourced once in `main/app_main.cpp` from
`telegramp4_provisioning_load()`. This is a small, mechanical change per
component — none of their internal logic changed, just where the
configuration values come from.

## What you learn

- A minimal captive-portal-style setup flow using `esp_wifi` (AP mode) +
  `esp_http_server`, without the complexity of DNS hijacking for a true
  captive portal (the user just opens `192.168.4.1` manually - a clear
  on-screen instruction is simpler and more reliable than depending on
  every OS's captive-portal detection behaving consistently)
- Why POST body parsing needs its own URL-decoding: `httpd_query_key_value()`
  finds `key=value` pairs in a string but does **not** decode
  percent-escapes or `+`-as-space - see `url_decode()` in
  `telegramp4_provisioning.c`
- NVS as the natural place for end-user-editable runtime configuration,
  with Kconfig reserved for compile-time developer defaults - the two
  aren't in conflict, one is just a fallback for the other

## Code

- [`components/telegramp4_provisioning/`](../../components/telegramp4_provisioning/) —
  `telegramp4_provisioning_load/clear/run_portal`
- [`main/app_main.cpp`](../../main/app_main.cpp) — BOOT-button check, calls
  `telegramp4_provisioning_load()` before anything else, passes the result
  into `telegramp4_security_configure()`, `telegramp4_wifi_init()`, and
  `telegramp4_telegram_start()`

## Test

```bash
idf.py build
idf.py -p <PORT> flash monitor
```

On a device with no saved configuration (or after holding BOOT at
power-on), connect a phone/laptop to the "TelegramP4-Setup" WiFi network,
open `http://192.168.4.1/` in a browser, fill in the form, and submit.
Expect a "Saved! Restarting..." page, then the device reboots and connects
to the configured WiFi network and Telegram bot.

**Verified 2026-09-10**: portal starts correctly on an unconfigured device
(confirmed in the boot log: `No WiFi/Telegram configuration found -
starting setup portal`, `ESP Event: softap started`), the AP is joinable
and the form renders correctly in a browser, and a POST to `/save` (tested
directly against the device) correctly parses the fields, saves to NVS,
returns the confirmation page, and reboots.

This lesson was added after [Lesson 16 — Final project review](16-final-project.md),
once real hardware testing (see `docs/hardware.md` and `CHANGELOG.md`)
showed end users need a way to configure the device without a rebuild.
