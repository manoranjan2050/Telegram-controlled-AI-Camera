# Hardware

## Primary board

**DFRobot FireBeetle 2 ESP32-P4 AI Vision Board**
Product page: https://www.dfrobot.com/product-2915.html

> ⚠️ This document is a placeholder until Phase 0 (and the relevant later phases)
> actually verify each detail against current DFRobot/Espressif documentation and,
> where possible, real hardware. Do not treat unconfirmed rows below as fact — they
> are known-unknowns to check, not assumptions to build against.
>
> **Status as of the initial code-writing pass (2026-09-06):** web search/fetch
> tools were unavailable in that development session, so none of the rows below
> could be checked against live DFRobot/Espressif documentation. Camera, MicroSD,
> microphone, display, and GPIO-whitelist code was written as an abstraction
> layer with Kconfig-exposed, placeholder pin values rather than guessed pin
> numbers — see each component's header comment for what's stubbed vs. real. This
> table must be filled in from the actual product page/wiki/schematic
> (https://www.dfrobot.com/product-2915.html) and confirmed on real hardware
> before trusting any of it.

| Item | Value | Verified? |
|---|---|---|
| Main SoC | ESP32-P4 | Confirm exact revision in Phase 0 |
| Connectivity co-processor | ESP32-C6 (Wi-Fi/BLE) | Confirm exact revision in Phase 0 |
| Flash size | Assumed 8MB in `sdkconfig.defaults` (`CONFIG_ESPTOOLPY_FLASHSIZE_8MB`) so the two-OTA-slot partition table fits | **Unconfirmed** — needed at least >4.1MB for the current partition table; 8MB was picked as a conservative common size, not read off a datasheet. Adjust if the real board differs. |
| PSRAM size | TBD | Verify in Phase 0 |
| Camera connector | MIPI-CSI | Confirm connector pinout/FPC type in Phase 5 |
| Bundled/recommended camera sensor | TBD | Confirm in Phase 5 |
| MicroSD interface | SDMMC, 4-bit, using ESP-IDF's SoC-default pins for ESP32-P4 (CLK=43 CMD=44 D0=39 D1=40 D2=41 D3=42) | **Unconfirmed for this board** — this is Espressif's chip-level reference default (`SDMMC_SLOT_CONFIG_DEFAULT()`), not something read off a DFRobot schematic. Verify before trusting it. |
| Microphone interface | TBD (I2S/PDM) | Confirm in Phase 11 |
| Display connector | MIPI-DSI (optional) | Confirm in Phase 20 |
| Available GPIO for user peripherals | TBD | Confirm whitelist in Phase 19 |
| Power input | TBD | Confirm in Phase 0 |

## Expected peripheral tree (spec §3)

```text
ESP32-P4
├── MIPI CSI camera
├── MIPI DSI display (optional)
├── MicroSD
├── microphone/audio
├── Wi-Fi through ESP32-C6 connectivity subsystem
├── Bluetooth LE where useful
├── GPIO
├── I2C
├── SPI
├── UART
└── USB
```

## Board abstraction

All board-specific pin/peripheral definitions live in
`components/telegramp4_board/`, so a future ESP32-P4 board can be supported by
adding a new board definition rather than editing feature components.

## How this file gets filled in

Each phase that touches a new piece of hardware (0, 5, 7, 11, 17, 19, 20) updates
the table above with confirmed values and a link to the source documentation
consulted, per [CLAUDE.md](../CLAUDE.md) rule 4 ("don't invent hardware
capabilities").
