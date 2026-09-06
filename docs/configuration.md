# Configuration

All configuration is done via `idf.py menuconfig` (Kconfig), under
`TelegramP4 Configuration`. Sensitive values live only in your local `sdkconfig`,
which is gitignored.

> This list grows as each phase adds its options. Update it in the same PR/commit
> that adds a new Kconfig entry.

## Wi-Fi (Phase 1)

| Option | Description | Default |
|---|---|---|
| `TELEGRAMP4_WIFI_SSID` | Network SSID | *(empty — required)* |
| `TELEGRAMP4_WIFI_PASSWORD` | Network password | *(empty — required)* |
| `TELEGRAMP4_WIFI_CONNECT_TIMEOUT_MS` | Initial connect timeout before boot continues without WiFi (reconnect keeps retrying regardless) | 15000 |

## Telegram (Phase 2–4)

| Option | Description | Default |
|---|---|---|
| `TELEGRAMP4_TELEGRAM_BOT_TOKEN` | Bot API token from BotFather | placeholder `123456789:REPLACE_WITH_YOUR_BOT_TOKEN` |
| `TELEGRAMP4_TELEGRAM_ALLOWED_CHAT_IDS` | Comma-separated whitelist of chat IDs (Phase 3) | *(empty — required)* |

## Camera (Phase 5)

| Option | Description | Default |
|---|---|---|
| `CAMERA_JPEG_QUALITY` | JPEG quality (0–63, lower = higher quality) | TBD |
| `CAMERA_FRAME_SIZE` | Capture resolution | TBD |

## Video (Phase 9)

| Option | Description | Default |
|---|---|---|
| `TELEGRAMP4_VIDEO_MIN_DURATION_S` | Minimum recording length | 1 |
| `TELEGRAMP4_VIDEO_DEFAULT_DURATION_S` | Default recording length | 10 |
| `TELEGRAMP4_VIDEO_MAX_DURATION_S` | Maximum recording length | 60 |

## Audio (Phase 11)

| Option | Description | Default |
|---|---|---|
| `TELEGRAMP4_AUDIO_MIN_DURATION_S` | Minimum recording length | 1 |
| `TELEGRAMP4_AUDIO_DEFAULT_DURATION_S` | Default recording length | 10 |
| `TELEGRAMP4_AUDIO_MAX_DURATION_S` | Maximum recording length | 60 |

## Storage (Phase 7)

| Option | Description | Default |
|---|---|---|
| `TELEGRAMP4_STORAGE_MAX_DOWNLOAD_SIZE_KB` | Max size accepted for Telegram-received files | 5120 |

## Speech-to-Text (Phase 13)

| Option | Description | Default |
|---|---|---|
| `TELEGRAMP4_STT_ENABLED` | Enable cloud speech-to-text for voice commands | n |
| `TELEGRAMP4_STT_API_KEY` | API key for the STT provider (currently OpenAI) | placeholder `sk-REPLACE_WITH_YOUR_API_KEY` |

## AI (Phase 14)

| Option | Description | Default |
|---|---|---|
| `TELEGRAMP4_AI_ENABLED` | Compile/enable AI features | n |

## Motion (Phase 17–18)

| Option | Description | Default |
|---|---|---|
| `TELEGRAMP4_MOTION_ENABLED` | Enable PIR motion detection | n |
| `TELEGRAMP4_MOTION_PIR_GPIO` | GPIO connected to PIR sensor | -1 (unset) |
| `TELEGRAMP4_MOTION_ALERT_COOLDOWN_S` | Minimum seconds between Telegram alerts | 30 |

## GPIO (Phase 19)

| Option | Description | Default |
|---|---|---|
| `TELEGRAMP4_GPIO_WHITELIST` | Comma-separated list of pins exposed to `/gpio` | *(empty)* |

## Debug

| Option | Description | Default |
|---|---|---|
| `LOG_DEFAULT_LEVEL` | ESP-IDF log verbosity | Info |

## Never configured via Kconfig source defaults

Bot token and Wi-Fi password must never be committed as real values in
`sdkconfig.defaults` — that file may only contain the placeholder/example shapes.
Real values belong in your local `sdkconfig`.
