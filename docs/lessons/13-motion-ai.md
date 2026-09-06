# Lesson 13 — PIR Motion Detection + AI Alert (Phases 17/18)

## Goal

Add `/arm`, `/disarm`, `/motion`, and a full motion → photo → AI-filtered →
Telegram alert pipeline with a cooldown to prevent spam.

## ⚠️ Hardware verification status

Which GPIO is safe to use for an external PIR sensor on this board has not
been confirmed — `TELEGRAMP4_MOTION_PIR_GPIO` defaults to `-1` (explicitly
unset) precisely so nobody enables an interrupt on a pin that's actually
wired to something else. The PIR-active-high assumption (rising-edge
interrupt) also needs verification against whatever module you actually
attach — some PIR modules are active-low.

## What you learn

- Keeping ISR work minimal: the GPIO interrupt handler only notifies a
  FreeRTOS task (`vTaskNotifyGiveFromISR`), all real work (camera capture, AI,
  Telegram) happens outside interrupt context
- A cooldown timestamp pattern to prevent alert spam without needing a timer
  or extra task
- Choosing to alert on raw motion when AI can't confirm a person, rather than
  silently going quiet — with the message honestly saying "AI filtering
  unavailable" instead of fabricating a confidence number

## Code

- [`components/telegramp4_motion/`](../../components/telegramp4_motion/) —
  GPIO ISR → task → `telegramp4_motion_arm/disarm/is_armed`, callback-based
- [`main/Kconfig.projbuild`](../../main/Kconfig.projbuild) — new `Motion`
  submenu: `TELEGRAMP4_MOTION_ENABLED`, `TELEGRAMP4_MOTION_PIR_GPIO`,
  `TELEGRAMP4_MOTION_ALERT_COOLDOWN_S` (default 30s)
- [`components/telegramp4_security/`](../../components/telegramp4_security/) —
  new `telegramp4_security_get_allowed_ids()`, used to broadcast alerts to
  every authorized chat
- [`main/app_main.cpp`](../../main/app_main.cpp) — `/arm`, `/disarm`,
  `/motion`, and `on_motion_detected()` (the full Phase 17+18 pipeline)

## How it works

`telegramp4_motion_init()` configures the PIR GPIO as an input with a
rising-edge interrupt and starts `motion_task`, which blocks on a task
notification. The ISR (`pir_isr_handler`) does the absolute minimum required
by IRAM/ISR constraints — just wakes the task. `motion_task` only calls the
registered callback if `telegramp4_motion_is_armed()` is true.

`on_motion_detected()` (registered as that callback) first checks the
cooldown (`TELEGRAMP4_MOTION_ALERT_COOLDOWN_S` since the last alert) and
bails out silently if still within it. Otherwise it captures a photo and, if
AI is enabled *and actually runs* (Phase 14's real model, once verified),
filters for a "person" label before alerting — no person, no alert. If AI is
disabled, or enabled but still an unverified stub, it alerts on raw motion
anyway (a silent security camera defeats the point) while being explicit in
the message that AI filtering wasn't available, rather than inventing a
confidence percentage. Alerts go to every chat ID in the whitelist via the
new `telegramp4_security_get_allowed_ids()`.

Time-of-day in the alert message is deliberately shown as **uptime**
(`HH:MM:SS` since boot), not wall-clock time — no NTP time sync has been
implemented yet, so a real clock time would be fabricated.

## Test

```bash
idf.py menuconfig   # TelegramP4 Configuration -> Motion -> enable + set your confirmed PIR GPIO
idf.py build
idf.py -p <PORT> flash monitor
```
`/arm`, then trigger the PIR. Expected (with camera hardware still unverified
from Phase 5): `❌ Camera unavailable` logged, no alert sent — once Phase 5 is
verified, expect an actual alert with photo.

## Phase 19 update — GPIO / IoT control

`/gpio` (no args) sends an inline keyboard listing every whitelisted pin with
its current state:
```
🔌 GPIO Control

GPIO 4 [OFF]
GPIO 5 [ON]
```
Tapping a button toggles that pin via `/gpio_toggle <pin>` (same dispatch
path as everything else) and re-sends the updated menu. `/gpio <pin> on|off`
does the same thing as a typed command. Only pins listed in
`TELEGRAMP4_GPIO_WHITELIST` (Kconfig, comma-separated, empty by default) can
be controlled — `telegramp4_gpio_is_whitelisted()` is checked before every
single write, so a request for an unlisted pin is rejected with
`❌ GPIO not whitelisted.` rather than silently driving an arbitrary pin.

**Code:** [`components/telegramp4_gpio/`](../../components/telegramp4_gpio/)
(`init/get_whitelist/is_whitelisted/set/get`),
[`main/Kconfig.projbuild`](../../main/Kconfig.projbuild) (`GPIO` submenu),
[`main/app_main.cpp`](../../main/app_main.cpp) (`handler_gpio`,
`handler_gpio_toggle`, `send_gpio_menu`).

**Test:** set `TELEGRAMP4_GPIO_WHITELIST` (e.g. `4,5`) with an LED wired to
one of those pins, then `/gpio` and tap a button.

## Next lesson

[Lesson 14 — Display](../PHASES.md#phase-20--display)
