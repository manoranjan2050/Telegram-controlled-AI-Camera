# Lesson 02 — Wi-Fi Connection

## Goal

Get the board connected to your Wi-Fi network in station mode, with automatic
reconnect and no credentials ever committed to Git.

## What you learn

- Configuring secrets through Kconfig instead of hardcoding them
- The ESP-IDF Wi-Fi event model (`WIFI_EVENT`, `IP_EVENT`) via the default event loop
- Using a FreeRTOS event group to let `app_main` block until the first connection
  succeeds (or time out and continue booting anyway)
- Implementing reconnect-on-disconnect without a dedicated polling task

## Hardware

FireBeetle 2 ESP32-P4 board, USB-connected. A 2.4 GHz Wi-Fi network to connect to
(confirm your board's supported bands against DFRobot's documentation — see
[hardware.md](../hardware.md)).

## Wiring

None — the ESP32-C6 connectivity subsystem handles Wi-Fi onboard.

## Software

Same ESP-IDF setup as Lesson 01.

## Code

- [`components/telegramp4_wifi/`](../../components/telegramp4_wifi/) — the new
  component: `telegramp4_wifi_init()`, `telegramp4_wifi_wait_connected()`,
  `telegramp4_wifi_get_state()`, `telegramp4_wifi_get_ip_str()`,
  `telegramp4_wifi_get_rssi()`
- [`Kconfig.projbuild`](../../Kconfig.projbuild) — new `WiFi` submenu:
  `TELEGRAMP4_WIFI_SSID`, `TELEGRAMP4_WIFI_PASSWORD`,
  `TELEGRAMP4_WIFI_CONNECT_TIMEOUT_MS`
- [`main/app_main.cpp`](../../main/app_main.cpp) — calls `telegramp4_wifi_init()`
  after the boot banner, then waits up to the configured timeout for the first
  connection before continuing

## How it works

`telegramp4_wifi_init()` sets up `esp_netif`, the default event loop, and the
Wi-Fi driver in STA mode, then registers a single event handler for both
`WIFI_EVENT` and `IP_EVENT`. On `WIFI_EVENT_STA_START` it calls
`esp_wifi_connect()`. On `WIFI_EVENT_STA_DISCONNECTED` it logs a warning, waits a
fixed delay, and reconnects — this runs for the life of the device, so temporary
Wi-Fi outages recover on their own. On `IP_EVENT_STA_GOT_IP` it logs the IP and
sets a bit in a FreeRTOS event group.

`app_main` calls `telegramp4_wifi_wait_connected(timeout_ms)`, which blocks on that
event group bit. If the network is unreachable, boot continues anyway — Wi-Fi will
keep retrying in the background rather than the whole device being stuck.

The SSID and password are read from `CONFIG_TELEGRAMP4_WIFI_SSID` /
`CONFIG_TELEGRAMP4_WIFI_PASSWORD` (generated from Kconfig into your local,
gitignored `sdkconfig`) and are never logged — not even at debug level.

## Test

```bash
idf.py menuconfig   # TelegramP4 Configuration -> WiFi -> set SSID/password
idf.py build
idf.py -p <PORT> flash monitor
```

## Expected output

```
I (xxx) TAG_WIFI: WiFi connecting...
I (xxx) TAG_WIFI: WiFi connected
I (xxx) TAG_WIFI: IP: 192.168.1.50
I (xxx) TAG_SYSTEM: Boot WiFi connect succeeded, IP: 192.168.1.50
I (xxx) TAG_SYSTEM: Phase 1 bootstrap complete.
```

If you power off your router briefly, expect:
```
W (xxx) TAG_WIFI: WiFi disconnected. Reconnecting...
```
followed by a fresh `WiFi connected` once it's back.

## Common errors

- Wrong SSID/password → connection never completes; double-check in
  `idf.py menuconfig` (values are case-sensitive).
- 5 GHz-only network → ESP32 Wi-Fi is 2.4 GHz only; use/enable a 2.4 GHz SSID.
- See [docs/troubleshooting.md](../troubleshooting.md).

## Challenge

Add an `esp_wifi_set_ps(WIFI_PS_NONE)` call and compare reconnect latency with and
without Wi-Fi power-save mode — useful groundwork for keeping Telegram long-polling
responsive in Phase 2.

## Next lesson

[Lesson 03 — Telegram `/start`](03-telegram-text.md)
