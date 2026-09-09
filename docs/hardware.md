# Hardware

## Primary board

**DFRobot FireBeetle 2 ESP32-P4 (DFR1172, "P4R32")**
Product page: https://www.dfrobot.com/product-2915.html
Wiki: https://wiki.dfrobot.com/dfr1172/
Schematic: https://dfimg.dfrobot.com/wiki/21103/DFR1172_firebeetle-esp32-p4r32-development-board_schematics_V1.0.pdf
Datasheet: https://dfimg.dfrobot.com/wiki/21103/DFR1172_firebeetle-esp32-p4r32-development-board_datasheet_V1.0.pdf

> **History:** this table started as an unverified placeholder (2026-09-06,
> written before web/hardware access). On 2026-09-07 it was fully verified
> against the real device: first live hardware testing (WiFi/Telegram/button
> menu working end-to-end), then the user provided the actual DFRobot wiki
> and schematic links, which were read directly (schematic pages rendered to
> images and inspected pin-by-pin) to confirm SD card and camera wiring, and
> the wiki's own spec table confirmed flash/PSRAM size. Every "Confirmed"
> row below is backed by one of those two sources, not inferred.

| Item | Value | Source |
|---|---|---|
| Main chip | ESP32-P4NRW32 (QFN104), confirmed from schematic page 1 (`U1`) | Schematic |
| Connectivity co-processor | ESP32-C6-MINI-1-N4, confirmed from schematic page 7 (`U3`) | Schematic |
| WiFi transport | ESP32-P4 has no native WiFi radio - reaches WiFi through the C6 via SDIO, using `esp_wifi_remote` (`CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`). SDIO pins: CLK=GPIO18, CMD=GPIO19, D0=GPIO14, D1=GPIO15, D2=GPIO16, D3=GPIO17 (schematic page 7, net names `SDIO_*`) - these also appear directly in the ESP-Hosted boot log. | Schematic + boot log, confirmed working live |
| Flash size | **16MB** | DFRobot wiki spec table ("Flash 16MB") + confirmed by boot log ("Detected size(16384k)") |
| PSRAM size | **32MB, in-package** (part of the NRW32 chip variant) | DFRobot wiki spec table ("PSRAM 32MB") + ESP32-P4 datasheet (NRW32 = 32MB in-package PSRAM) |
| Camera connector | MIPI-CSI (2-lane), schematic page 2 (`J1`) | Schematic |
| Camera sensor | **OV5647** - the installed module is a Raspberry Pi Camera Module v1.3, confirmed by reading the module's own PCB silkscreen | User-confirmed (physical module inspection) |
| Camera SCCB (I2C) pins | SCL=**GPIO8**, SDA=**GPIO7** (schematic page 1 pins 8/7, net `ESP_SCL`/`ESP_SDA`, routed to CSI connector page 2) | Schematic |
| Camera reset/pwdn/xclk | **Not connected** - the CSI connector (page 2) only carries the 2 MIPI diff pairs, I2C, and two unused IO pins (`CSI_IO0`/`CSI_IO1`, the latter marked NC) - no reset, power-down, or XCLK signal is routed from the P4 to the sensor at all | Schematic |
| MicroSD interface | SDMMC, 4-bit, `SDMMC_HOST_SLOT_0` (slot 1 is used by the WiFi/C6 link). CLK=GPIO43, CMD=GPIO44, D0=GPIO39, D1=GPIO40, D2=GPIO41, D3=GPIO42 (schematic page 1, net names `SD1_*`) - these happen to match ESP-IDF's own `SDMMC_SLOT_CONFIG_DEFAULT()` for ESP32-P4 exactly | Schematic |
| MicroSD power gate | The SD socket's VDD is switched by a P-MOSFET (`Q1`, AO3401) gated by **GPIO45** (net `SD1_PWRN`). `telegramp4_storage_init()` drives GPIO45 low with a 100ms settle delay before mounting. **Every plausible firmware-side fix has now been tried on real hardware and none change the outcome**: both power-gate polarities (LOW/HIGH), and (2026-09-10) enabling `SDMMC_SLOT_FLAG_INTERNAL_PULLUP` on CMD/D0-D3 — all produce the exact same `sdmmc_init_ocr: send_op_cond (1) returned 0x107` failure, at the exact same point, every time. That failure is at the very first card-identification command, before bus width/speed/filesystem are even negotiated. An identical, unmoved failure across four independent firmware variables is a strong signal this is now a **physical** issue, not a code bug: no card seated, a bad/dead card, or a broken trace/pad on the socket. Firmware cannot distinguish those from here. Next steps require physical access to the board: (1) confirm a card is actually seated in the slot; (2) try a different, known-good, small (≤32GB) FAT32-formatted MicroSD card; (3) if it still fails, probe the SD socket's VDD pin with a multimeter while booting to confirm GPIO45 is actually switching power at all. | Schematic (page 4) + repeated live hardware tests (root cause narrowed to physical/card, not firmware) |
| Microphone | MSM261DGT003 PDM MIC (schematic page 6, `U4`). Confirmed 2026-09-09: **PDM_DATA=GPIO9, PDM_CLK=GPIO12** (I2S_DATA/I2S_CLK nets traced to these pins). Not yet used in code - `telegramp4_audio` is still a stub. | Schematic |
| Display connector | MIPI-DSI (`J2`, schematic page 2), same SCL/SDA I2C lines as camera, no other GPIO signals routed | Schematic |
| Available GPIO for user peripherals (incl. PIR) | Not yet cross-referenced against this schematic - default unset (`-1`) until confirmed free | Confirm before setting `TELEGRAMP4_MOTION_PIR_GPIO` / `TELEGRAMP4_GPIO_WHITELIST` |
| USB | Two USB-C ports: `USBC` (page 3, `U8`) used for flashing/serial (shows as VID_303A/PID_1001 "USB Serial Device" + "USB JTAG/serial debug unit"), `USB1`/`TYPEC` (page 3, `U5`) wired to the P4's other USB PHY | Schematic + boot log |

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
