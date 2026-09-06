# Memory & Performance

ESP32-P4 has substantial RAM/PSRAM, but camera frames, JPEGs, video buffers, and AI
models can still exhaust it if handled carelessly. This document tracks the rules
and, over time, the measured numbers.

## Rules

- Prefer `heap_caps_malloc()` with explicit capability flags (`MALLOC_CAP_SPIRAM`,
  `MALLOC_CAP_DMA`, etc.) for large or DMA-bound buffers over plain `malloc`.
- Free every large buffer as soon as it's no longer needed — no "free on next boot."
- Avoid unnecessary copies between capture → encode → upload; stream where the API
  allows it instead of buffering a whole file in RAM.
- After large operations (photo capture, video recording, AI inference), log:
  ```
  free_heap
  free_psram
  largest_free_block
  ```
- Don't leave memory allocated after a photo/video/AI operation completes — verify
  via the logs above that usage returns to baseline.

## Measured performance (filled in as phases are hardware-tested)

| Metric | Value | Phase / Date measured |
|---|---|---|
| Camera FPS | TBD | Phase 5 |
| JPEG size (typical) | TBD | Phase 5 |
| Photo capture → upload latency | TBD | Phase 6 |
| AI inference time | TBD | Phase 14 |
| AI model memory footprint | TBD | Phase 14 |
| Video encode throughput | TBD | Phase 9 |
| Telegram round-trip latency | TBD | Phase 22 (`/diagnostics`) |
| Baseline free heap (idle, Wi-Fi+Telegram connected) | TBD | Phase 2 |
| Baseline free PSRAM (idle) | TBD | Phase 2 |

Never fill this table with fabricated numbers — only real measurements from actual
hardware runs, with the date and phase noted.
