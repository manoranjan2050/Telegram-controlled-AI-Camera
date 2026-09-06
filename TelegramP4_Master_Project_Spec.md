# TelegramP4 — ESP32-P4 Telegram Camera & IoT Learning Project

## Master Build Specification for Claude Code

You are an expert embedded systems engineer specializing in ESP32-P4, ESP-IDF, camera/MIPI CSI, audio, Telegram Bot API, networking, and production-quality C/C++ firmware.

Build this project **step by step**, starting from a minimal working firmware and progressively adding features. Do NOT attempt to create the entire project in one giant implementation. After each phase, keep the project buildable and explain what was added.

The primary target hardware is:

- DFRobot FireBeetle 2 ESP32-P4 AI Vision Board
- ESP32-P4 main processor
- ESP32-C6 connectivity subsystem
- MIPI CSI camera
- MicroSD card
- On-board microphone/audio hardware where supported
- Optional MIPI DSI display
- Wi-Fi connectivity

Reference product:
https://www.dfrobot.com/product-2915.html

Use current official Espressif and DFRobot documentation when implementation details are uncertain.

---

# 1. PROJECT VISION

Create an open-source project called:

# TelegramP4

Tagline:

> Control an ESP32-P4 AI camera and IoT device from Telegram.

The goal is not merely to create a camera demo. The project should be a **community-friendly learning platform** where users can progress from a simple Telegram bot to:

- Wi-Fi control
- Telegram commands
- Telegram buttons
- Camera photos
- Video recording
- MicroSD storage
- Receiving photos from Telegram
- Audio/voice messages
- Microphone recording
- AI image detection
- Motion-triggered photos
- Remote IoT control
- Optional display
- OTA updates
- Secure user authorization

The code must be modular so individual lessons can be understood and reused.

---

# 2. CORE PRINCIPLES

Follow these principles throughout the project.

## 2.1 Build incrementally

Never start with a giant monolithic firmware.

Every phase must compile and flash before moving to the next phase.

## 2.2 Beginner friendly

The repository should be understandable by:

- school students
- electronics hobbyists
- university students
- embedded developers
- AI/IoT developers

Use comments where they help learning.

## 2.3 Production quality

Even though this is educational, avoid deliberately bad architecture.

Use:

- FreeRTOS tasks where appropriate
- queues
- event groups
- mutexes
- clean module boundaries
- error handling
- logging
- configuration management
- watchdog awareness
- memory checks
- secure credential handling

## 2.4 Don't reinvent existing Espressif components

Prefer official ESP-IDF and Espressif components when available.

For camera/video/AI functionality, investigate and use the appropriate official Espressif components/frameworks rather than creating low-level replacements unnecessarily.

## 2.5 Never hardcode secrets

Telegram bot token and authorized Telegram chat IDs must NOT be committed to Git.

Use configuration through:

- menuconfig/Kconfig
- local configuration
- generated secrets/configuration
- `.gitignore`

Provide a safe example configuration file.

---

# 3. TARGET HARDWARE

Primary board:

DFRobot FireBeetle 2 ESP32-P4 AI Vision Board.

The firmware should have a hardware abstraction layer so future ESP32-P4 boards can be supported.

Expected peripherals:

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

Do not assume exact GPIO mappings from memory. Verify them against the current DFRobot documentation/schematic before implementing hardware-specific code.

Create:

```text
components/telegramp4_board/
```

for board-specific definitions.

---

# 4. HIGH-LEVEL ARCHITECTURE

Use this architecture:

```text
                         TELEGRAM CLOUD
                              │
                         HTTPS / Bot API
                              │
                              ▼
                     ┌──────────────────┐
                     │ Telegram Manager │
                     └────────┬─────────┘
                              │
                    Command / Event Bus
                              │
          ┌───────────────────┼───────────────────┐
          │                   │                   │
          ▼                   ▼                   ▼
      Camera Manager      Audio Manager       AI Manager
          │                   │                   │
          ▼                   ▼                   ▼
       JPEG/Video          Audio files       AI inference
          │                   │                   │
          └───────────────────┼───────────────────┘
                              ▼
                         Storage Manager
                              │
                              ▼
                           MicroSD

Additional modules:

WiFi Manager
Config Manager
Security Manager
GPIO Manager
Motion Manager
Display Manager
OTA Manager
System Manager
```

---

# 5. REPOSITORY STRUCTURE

Create this structure:

```text
TelegramP4/
│
├── README.md
├── LICENSE
├── CHANGELOG.md
├── CONTRIBUTING.md
├── SECURITY.md
├── CODE_OF_CONDUCT.md
│
├── docs/
│   ├── getting-started.md
│   ├── hardware.md
│   ├── telegram-bot-setup.md
│   ├── configuration.md
│   ├── troubleshooting.md
│   ├── architecture.md
│   ├── memory-and-performance.md
│   ├── security.md
│   └── lessons/
│       ├── 01-hello-world.md
│       ├── 02-wifi.md
│       ├── 03-telegram-text.md
│       ├── 04-telegram-buttons.md
│       ├── 05-camera.md
│       ├── 06-send-photo.md
│       ├── 07-microsd.md
│       ├── 08-video.md
│       ├── 09-receive-photo.md
│       ├── 10-audio.md
│       ├── 11-voice.md
│       ├── 12-ai.md
│       ├── 13-motion-ai.md
│       ├── 14-display.md
│       ├── 15-ota.md
│       └── 16-final-project.md
│
├── examples/
│   ├── 01_hello_world/
│   ├── 02_wifi/
│   ├── 03_telegram_hello/
│   ├── 04_telegram_buttons/
│   ├── 05_camera/
│   ├── 06_photo/
│   ├── 07_sdcard/
│   ├── 08_video/
│   ├── 09_receive_photo/
│   ├── 10_audio/
│   ├── 11_voice/
│   ├── 12_ai/
│   └── 13_motion_ai/
│
├── main/
│   ├── app_main.cpp
│   └── CMakeLists.txt
│
├── components/
│   ├── telegramp4_telegram/
│   ├── telegramp4_wifi/
│   ├── telegramp4_camera/
│   ├── telegramp4_video/
│   ├── telegramp4_audio/
│   ├── telegramp4_storage/
│   ├── telegramp4_ai/
│   ├── telegramp4_motion/
│   ├── telegramp4_gpio/
│   ├── telegramp4_display/
│   ├── telegramp4_config/
│   ├── telegramp4_security/
│   ├── telegramp4_ota/
│   ├── telegramp4_system/
│   └── telegramp4_board/
│
├── partitions/
│   └── partitions.csv
│
├── sdkconfig.defaults
├── Kconfig.projbuild
├── CMakeLists.txt
├── idf_component.yml
├── .gitignore
└── tools/
    └── README.md
```

---

# 6. DEVELOPMENT PHASES

Implement these phases in order.

Do not skip ahead until the previous phase works.

---

# PHASE 0 — Project Bootstrap

Create a clean ESP-IDF project.

Requirements:

- ESP-IDF
- C/C++
- CMake
- FreeRTOS
- logging
- Kconfig
- clean component architecture

Verify:

```bash
idf.py set-target esp32p4
idf.py build
idf.py flash
idf.py monitor
```

Use the correct target and configuration for the actual board after checking the hardware documentation.

Expected first result:

```text
TelegramP4 starting...
Board: FireBeetle 2 ESP32-P4
Firmware version: 0.1.0
```

---

# PHASE 1 — Wi-Fi

Implement:

```text
telegramp4_wifi
```

Features:

- STA mode
- SSID/password through Kconfig
- reconnect
- event handling
- IP logging
- connection timeout
- Wi-Fi state events

Expected log:

```text
WiFi connecting...
WiFi connected
IP: 192.168.x.x
```

Do not print passwords.

---

# PHASE 2 — Telegram Basic

Implement Telegram Bot API HTTPS client.

First commands:

```text
/start
/help
/status
```

Example:

```text
User:
/status

Bot:
TelegramP4

Status:
WiFi: Connected
IP: 192.168.1.50
Uptime: 123 sec
Free heap: 8.2 MB
PSRAM: available
```

Use Telegram Bot API methods appropriately.

At minimum implement:

- `getMe`
- `getUpdates`

Use long polling initially because it is much simpler for ESP32.

Do NOT expose the bot token in logs.

---

# PHASE 3 — Telegram Command Framework

Create a reusable command system.

Example:

```cpp
register_command("/start", handler_start);
register_command("/help", handler_help);
register_command("/status", handler_status);
register_command("/photo", handler_photo);
```

Support:

```text
/command
/command argument
/command arg1 arg2
```

Unknown command:

```text
Unknown command.

Use /help to see available commands.
```

Implement an authorization layer.

Only configured Telegram chat IDs may control the device.

Unauthorized user:

```text
Access denied.
```

Do not leak device information to unauthorized users.

---

# PHASE 4 — Telegram Inline Buttons

Create the main Telegram control menu:

```text
┌────────────────────────────┐
│       TelegramP4           │
├────────────────────────────┤
│ 📸 Take Photo              │
│ 🎥 Record Video            │
│ 🖼 Last Photo              │
│ 🎙 Record Audio            │
│ 🤖 AI Detect               │
│ 📊 Status                  │
│ 💾 SD Card                 │
│ ⚙ Settings                 │
└────────────────────────────┘
```

Use callback queries.

Commands and buttons must call the same internal functions.

Do not duplicate business logic.

---

# PHASE 5 — Camera

Implement camera abstraction:

```text
telegramp4_camera
```

Functions should be conceptually similar to:

```cpp
camera_init();
camera_deinit();
camera_capture();
camera_release_frame();
camera_get_status();
```

Use the appropriate official camera/video component for ESP32-P4.

Verify the exact FireBeetle camera interface and supported camera module before selecting configuration.

Initial goal:

```text
Camera initialized
Frame captured
JPEG created
```

Add test command:

```text
/photo_test
```

Save the captured JPEG locally.

---

# PHASE 6 — Send Photo to Telegram

Implement Telegram multipart upload for photos.

Command:

```text
/photo
```

Flow:

```text
Telegram
   │
 /photo
   ↓
ESP32-P4
   ↓
Camera capture
   ↓
JPEG
   ↓
Telegram sendPhoto
   ↓
User receives image
```

Handle:

- HTTPS
- multipart/form-data
- Content-Length
- timeouts
- retries
- memory cleanup

Do not load unnecessarily large files entirely into RAM.

Where possible, stream files from storage.

---

# PHASE 7 — MicroSD Storage

Implement:

```text
telegramp4_storage
```

Directory structure:

```text
/sdcard/
├── photos/
├── videos/
├── audio/
├── received/
├── ai/
└── logs/
```

Commands:

```text
/files
/storage
/delete filename
```

Return:

```text
SD Card

Used: 1.2 GB
Free: 14.8 GB
Total: 16 GB
```

Implement safe filenames.

Never allow Telegram input to create arbitrary filesystem traversal such as:

```text
../../something
```

---

# PHASE 8 — Photo Gallery

Implement:

```text
/photos
```

Show recent photos.

For each photo provide Telegram buttons:

```text
[View]
[Download]
[Delete]
```

Limit gallery size in one message.

Add pagination if necessary.

---

# PHASE 9 — Video

Implement video recording.

Command:

```text
/video 10
```

Behavior:

```text
🎥 Recording for 10 seconds...
```

Then:

```text
Recording complete.

Size: 4.8 MB
Duration: 10 sec

Uploading...
```

Then send the video to Telegram.

Important:

- Check actual hardware/video encoder capabilities.
- Do not assume H.264 is available unless verified.
- If the hardware/software stack produces another supported format, use the practical format.
- Keep video size within Telegram Bot API limits.
- Implement maximum duration.
- Implement free-space checks.

Initial limits:

```text
minimum: 1 sec
default: 10 sec
maximum: 60 sec
```

Make these configurable.

---

# PHASE 10 — Receive Photo From Telegram

User sends a photo to the bot.

Flow:

```text
User
 ↓
Telegram
 ↓
getUpdates
 ↓
file_id
 ↓
getFile
 ↓
download
 ↓
/sdcard/received/
```

Bot replies:

```text
📥 Image received.

File:
received_20260906_224500.jpg

Size:
1.4 MB
```

Implement:

```text
/photo_info
/photo_files
```

Do not download files larger than configured safety limits.

---

# PHASE 11 — Audio Recording

Implement microphone capture.

Command:

```text
/record 10
```

Flow:

```text
Microphone
 ↓
PCM/audio processing
 ↓
file
 ↓
MicroSD
 ↓
Telegram
```

Before selecting a Telegram audio format, determine which codec/format is practical on this hardware.

Do not pretend the device supports arbitrary audio codecs.

Use the simplest robust format first.

Example response:

```text
🎙 Recording 10 seconds...
```

Then send the audio file.

---

# PHASE 12 — Receive Telegram Voice

Receive Telegram voice messages.

Flow:

```text
Telegram Voice
 ↓
file_id
 ↓
getFile
 ↓
download
 ↓
MicroSD
 ↓
optional decoder
 ↓
optional speech recognition
```

Initially just save and acknowledge:

```text
🎙 Voice message received.

Duration: 7 sec
Saved:
received/voice_001.xxx
```

Do not implement speech recognition until basic voice file handling works.

---

# PHASE 13 — Voice Commands

After receiving voice successfully, add speech recognition.

Architecture should allow either:

## Option A — Cloud/server STT

```text
Telegram voice
 ↓
ESP32
 ↓
external STT service
 ↓
text
 ↓
command parser
 ↓
camera/action
```

## Option B — Local STT

```text
Telegram voice
 ↓
ESP32
 ↓
local speech model
 ↓
text
 ↓
command parser
```

Make STT provider modular.

Do not hardcode one external provider into the core Telegram module.

Example:

User sends:

> Take a photo.

System:

```text
Speech recognized:
"take a photo"

Command:
PHOTO

Executing...
```

Then send the photo.

---

# PHASE 14 — AI Vision

Implement AI abstraction:

```text
telegramp4_ai
```

Conceptual interface:

```cpp
ai_init();
ai_process_image();
ai_get_results();
ai_deinit();
```

Use appropriate ESP32-P4-compatible AI framework/model.

Start with object detection.

Example:

```text
🤖 AI Result

Person     92%
Chair      81%
Laptop     78%
```

Important:

- Keep AI model separate from application code.
- Store model configuration separately.
- Check PSRAM requirements.
- Measure inference time.
- Measure memory usage.
- Do not claim a model works until tested on actual hardware.

---

# PHASE 15 — Telegram AI Workflow

Command:

```text
/ai
```

Flow:

```text
Camera
 ↓
Capture
 ↓
Preprocess
 ↓
AI inference
 ↓
Detection results
 ↓
Telegram
```

Send both:

1. photo
2. text result

Example:

```text
🤖 AI Detection

Detected:
👤 Person — 94%
💻 Laptop — 88%

Inference:
142 ms
```

---

# PHASE 16 — AI on Telegram Photos

User sends a photo.

Bot asks:

```text
Image received.

What would you like to do?

[🤖 Detect Objects]
[💾 Save]
[🗑 Delete]
```

If AI is selected:

```text
Received image
 ↓
Decode
 ↓
Resize/preprocess
 ↓
AI
 ↓
Results
```

Return result and optionally annotated image.

---

# PHASE 17 — Motion Detection

Support an external PIR sensor.

Architecture:

```text
PIR
 ↓
GPIO interrupt/event
 ↓
Motion Manager
 ↓
Camera
 ↓
AI
 ↓
Person?
 ├── No → ignore
 └── Yes
      ↓
    save photo
      ↓
    Telegram alert
```

Commands:

```text
/arm
/disarm
/motion
```

Status:

```text
Motion detection: ARMED
AI filtering: ON
```

Do not continuously upload images unless configured.

---

# PHASE 18 — Motion AI Alert

When armed:

```text
Motion detected
       ↓
Take photo
       ↓
AI detection
       ↓
Person detected?
       ↓
Yes
       ↓
Telegram alert
```

Example:

```text
🚨 Person detected

Confidence: 94%
Time: 22:45:31

[Photo]
```

Allow configurable cooldown:

```text
motion_alert_cooldown = 30 seconds
```

This prevents Telegram spam.

---

# PHASE 19 — GPIO / IoT Control

Add generic GPIO controls.

Example commands:

```text
/gpio
/gpio 4 on
/gpio 4 off
```

Prefer a safe whitelist of configurable GPIOs.

Telegram UI:

```text
🔌 GPIO Control

GPIO 1 [ON]
GPIO 2 [OFF]
GPIO 3 [ON]
```

Also support external:

- LED
- buzzer
- relay
- sensor
- button

Never expose dangerous/unconfigured GPIOs automatically.

---

# PHASE 20 — Display

If a supported MIPI DSI display is attached, show:

```text
TelegramP4

WiFi ✓
Telegram ✓
Camera ✓
SD ✓
AI ✓

IP:
192.168.1.50
```

Camera preview:

```text
┌───────────────────────┐
│                       │
│      CAMERA           │
│       VIEW            │
│                       │
├───────────────────────┤
│ WiFi ✓   AI ✓         │
└───────────────────────┘
```

Make display optional.

The firmware must still work without the display.

---

# PHASE 21 — OTA

Implement OTA updates.

Requirements:

- firmware version
- safe update process
- rollback where supported
- sufficient free flash
- progress reporting
- verification

Telegram command:

```text
/version
```

Response:

```text
TelegramP4

Firmware:
1.0.0

ESP-IDF:
x.x.x

Board:
FireBeetle 2 ESP32-P4
```

For the first implementation, local/manual OTA is acceptable. Remote OTA can be added afterward.

---

# PHASE 22 — System Status

Implement:

```text
/status
```

Example:

```text
📊 TelegramP4 Status

Firmware: 1.0.0

WiFi:
✓ Connected
IP: 192.168.1.50
RSSI: -52 dBm

Telegram:
✓ Connected

Camera:
✓ Ready

SD Card:
✓ Ready
Free: 14.2 GB

AI:
✓ Ready
Last inference: 138 ms

Memory:
Heap: 8.1 MB
PSRAM: 20.4 MB free

Uptime:
3h 24m
```

Do not expose sensitive credentials.

---

# 7. TELEGRAM COMMAND LIST

Final target command list:

```text
/start
/help
/status
/photo
/video [seconds]
/record [seconds]
/photos
/files
/storage
/delete [file]
/ai
/arm
/disarm
/motion
/gpio
/version
/reboot
```

Use buttons for common actions.

Dangerous actions such as reboot/delete must have confirmation.

Example:

```text
Are you sure?

[Yes, reboot] [Cancel]
```

---

# 8. TELEGRAM SECURITY

This is mandatory.

## 8.1 Chat ID whitelist

Only configured IDs can control the device.

Support multiple IDs:

```text
TELEGRAM_ALLOWED_CHAT_IDS
```

## 8.2 Bot token

Never commit it.

Never print it.

Never include it in README examples.

Use:

```text
123456789:REPLACE_WITH_YOUR_BOT_TOKEN
```

only as a placeholder.

## 8.3 HTTPS

Telegram communication must use TLS.

Validate certificates appropriately for ESP-IDF.

Do not disable certificate verification merely to make development easier.

If a development-only insecure mode is absolutely necessary, clearly isolate and label it.

## 8.4 File safety

Sanitize filenames.

Limit:

- maximum download size
- maximum video duration
- maximum number of stored files
- storage usage

## 8.5 Rate limiting

Avoid command flooding.

Implement basic per-chat cooldown/rate limits where appropriate.

---

# 9. MEMORY MANAGEMENT

ESP32-P4 has substantial RAM/PSRAM, but camera frames, JPEGs, video and AI models can still consume a lot of memory.

Every major module should consider:

```text
heap_caps_malloc()
PSRAM
DMA-capable memory
frame buffers
temporary buffers
```

Avoid unnecessary copies.

After large operations log memory:

```text
free_heap
free_psram
largest_free_block
```

Do not leave memory allocated after photo/video/AI operations.

---

# 10. TASK ARCHITECTURE

Use FreeRTOS tasks where appropriate.

Suggested model:

```text
telegram_task
wifi_event_task / event loop
camera_task
video_task
audio_task
ai_task
storage_task
motion_task
display_task
system_monitor_task
```

Do not create a task for every trivial function.

Use queues/events between modules.

Example:

```text
Telegram Task
     ↓
Command Queue
     ↓
Application Controller
     ↓
Camera/AI/Storage
```

Avoid blocking Telegram communication while AI inference or video recording runs.

---

# 11. EVENT SYSTEM

Create internal events such as:

```cpp
TELEGRAM_CONNECTED
TELEGRAM_MESSAGE_RECEIVED
PHOTO_REQUESTED
PHOTO_READY
VIDEO_STARTED
VIDEO_READY
AUDIO_READY
AI_STARTED
AI_COMPLETE
MOTION_DETECTED
STORAGE_ERROR
CAMERA_ERROR
```

This makes the architecture scalable.

---

# 12. ERROR HANDLING

Every subsystem must handle failure.

Examples:

Camera unavailable:

```text
❌ Camera unavailable.
Check camera connection.
```

SD full:

```text
❌ Storage full.
Free some space before recording.
```

Telegram upload failure:

```text
❌ Telegram upload failed.
Retrying...
```

Wi-Fi lost:

```text
WiFi disconnected.
Reconnecting...
```

AI memory failure:

```text
❌ AI inference failed: insufficient memory
```

Never silently ignore serious errors.

---

# 13. LOGGING

Use ESP-IDF logging:

```cpp
ESP_LOGI()
ESP_LOGW()
ESP_LOGE()
ESP_LOGD()
```

Tags:

```text
TAG_WIFI
TAG_TELEGRAM
TAG_CAMERA
TAG_VIDEO
TAG_AUDIO
TAG_STORAGE
TAG_AI
TAG_MOTION
TAG_DISPLAY
TAG_SYSTEM
```

Example:

```text
I (1234) TELEGRAM: Received command /photo
I (1240) CAMERA: Capturing frame
I (1320) CAMERA: JPEG size 182 KB
I (1325) TELEGRAM: Uploading photo
I (1530) TELEGRAM: Photo sent successfully
```

Never log:

- bot token
- Wi-Fi password
- private credentials

---

# 14. CONFIGURATION

Use Kconfig.

Configuration should include:

```text
WiFi SSID
WiFi password

Telegram bot token
Allowed chat IDs

Camera settings
Video max duration
Photo JPEG quality

SD storage limits

AI enabled
AI model

Motion detection enabled
Motion cooldown

Debug logging level
```

Sensitive configuration must be local and ignored by Git.

---

# 15. README REQUIREMENTS

Create a beautiful beginner-friendly README.

Include:

```text
TelegramP4
Control an ESP32-P4 AI Camera from Telegram
```

Sections:

1. What is TelegramP4?
2. Features
3. Hardware
4. Wiring
5. Software requirements
6. Install ESP-IDF
7. Create Telegram bot
8. Configure Wi-Fi
9. Configure Telegram token
10. Build
11. Flash
12. First command
13. Camera setup
14. AI setup
15. Troubleshooting
16. Architecture
17. Learning roadmap
18. Contributing
19. License

Include diagrams using Mermaid where useful.

---

# 16. TELEGRAM BOT SETUP GUIDE

Create:

```text
docs/telegram-bot-setup.md
```

Explain how to:

1. Open Telegram
2. Find BotFather
3. Create bot
4. Obtain token
5. Configure token securely
6. Obtain chat ID
7. Add chat ID to configuration
8. Test `/start`

Never put a real token in documentation.

---

# 17. LESSON SYSTEM

Every lesson must contain:

```text
# Lesson X — Title

## Goal

## What you learn

## Hardware

## Wiring

## Software

## Code

## How it works

## Test

## Expected output

## Common errors

## Challenge

## Next lesson
```

The lessons should gradually increase complexity.

---

# 18. LESSON ROADMAP

Create these lessons:

### 01
Hello ESP32-P4

### 02
Wi-Fi connection

### 03
Telegram `/start`

### 04
Telegram `/status`

### 05
Telegram buttons

### 06
Camera capture

### 07
Send photo to Telegram

### 08
MicroSD storage

### 09
Telegram photo gallery

### 10
Video recording

### 11
Receive Telegram photo

### 12
Microphone/audio

### 13
Receive Telegram voice

### 14
Voice command processing

### 15
AI object detection

### 16
AI + Telegram

### 17
PIR motion detection

### 18
Motion + AI + Telegram

### 19
GPIO/relay control

### 20
Display

### 21
OTA

### 22
Final integrated project

---

# 19. FINAL INTEGRATED EXPERIENCE

The final Telegram interface should look like:

```text
TelegramP4
ESP32-P4 AI Camera

WiFi: ✓
Camera: ✓
SD: ✓
AI: ✓

[📸 Photo]
[🎥 Video]
[🤖 AI]
[🎙 Audio]
[🖼 Gallery]
[🚨 Motion]
[🔌 GPIO]
[📊 Status]
[⚙ Settings]
```

Example workflow:

## Photo

```text
User → 📸 Photo
       ↓
ESP32 camera
       ↓
JPEG
       ↓
Telegram
       ↓
Photo
```

## AI

```text
User → 🤖 AI
       ↓
Camera
       ↓
AI
       ↓
Result + image
```

## Motion

```text
PIR
 ↓
Motion
 ↓
Camera
 ↓
AI
 ↓
Person detected
 ↓
Telegram alert
```

## Voice

```text
Telegram voice
 ↓
Download
 ↓
Speech-to-text
 ↓
Command
 ↓
Camera/action
```

---

# 20. TESTING REQUIREMENTS

Create a test checklist.

## Wi-Fi

- connect
- disconnect
- reconnect
- wrong password

## Telegram

- valid command
- invalid command
- unauthorized chat
- Telegram unavailable
- network unavailable

## Camera

- initialization
- capture
- repeated captures
- camera failure

## SD

- mount
- write
- read
- delete
- full storage

## Photo

- capture
- upload
- Telegram failure
- retry

## Video

- short recording
- maximum duration
- insufficient storage
- upload failure

## Audio

- recording
- file creation
- Telegram upload

## AI

- model load
- inference
- low memory
- repeated inference

## Motion

- trigger
- cooldown
- false triggers

---

# 21. PERFORMANCE TESTING

Create a `/diagnostics` command for authorized users.

Report:

```text
CPU
Heap
PSRAM
Storage
Camera FPS
JPEG size
AI inference time
WiFi RSSI
Telegram latency
```

Example:

```text
Diagnostics

Free Heap: 7.8 MB
Free PSRAM: 19.4 MB

Camera:
FPS: 15
JPEG: 185 KB

AI:
Inference: 137 ms

WiFi:
RSSI: -51 dBm

Telegram:
Latency: 240 ms
```

---

# 22. IMPORTANT IMPLEMENTATION RULES

1. Do not invent unsupported hardware capabilities.
2. Verify exact FireBeetle GPIO/camera/audio/display details from current documentation.
3. Do not assume a particular camera sensor works without configuration/testing.
4. Do not assume hardware video encoding support without verification.
5. Do not assume arbitrary Telegram media formats are supported by the ESP32.
6. Use official Espressif APIs/components where possible.
7. Keep external dependencies documented.
8. Pin dependency versions when practical.
9. Keep secrets out of source control.
10. Never disable TLS certificate validation in production.
11. Keep every phase buildable.
12. Do not generate fake test results.
13. Clearly label features that are experimental.
14. Write real error handling.
15. Do not block the main application indefinitely on network operations.
16. Keep the device functional if display or AI is unavailable.
17. Make AI optional at compile/configuration level.
18. Make camera/video/audio modules independently testable.

---

# 23. DEVELOPMENT WORKFLOW FOR CLAUDE

When starting this project:

## Step 1

Inspect the environment.

Check:

```bash
idf.py --version
idf.py --list-targets
```

Check whether ESP-IDF is installed correctly.

## Step 2

Inspect the board documentation and determine:

- exact ESP-IDF target
- camera connector/pins
- supported camera modules
- MicroSD pins/mode
- microphone interface
- display interface
- power requirements
- available PSRAM/flash
- any DFRobot-specific configuration

## Step 3

Create Phase 0.

## Step 4

Build and verify.

## Step 5

Only then implement Phase 1.

Continue one phase at a time.

---

# 24. DO NOT DO THIS

Do NOT:

- create one giant `main.cpp`
- put Telegram, camera, AI and SD code together
- hardcode credentials
- use blocking delays everywhere
- allocate huge buffers on internal RAM unnecessarily
- ignore memory failures
- ignore TLS
- expose the camera to every Telegram user
- claim unsupported video/audio formats
- copy large third-party projects without respecting licenses
- remove copyright/license information from reused code

---

# 25. OPEN-SOURCE LICENSING

Use a permissive open-source license for the TelegramP4 application code unless a different choice is explicitly requested.

Before adding third-party code:

- inspect its license
- retain required copyright notices
- document dependencies
- don't copy code whose license is incompatible

Espressif components may have their own licenses. Preserve their notices.

---

# 26. FUTURE EXTENSIONS

Design the architecture so these can be added later:

```text
Telegram
WhatsApp/other messaging integrations
Web dashboard
Android app
MQTT
Home Assistant
REST API
Bluetooth
USB camera
Multiple cameras
Object tracking
Face recognition
QR/barcode
Timelapse
Night vision
Temperature sensors
Relay control
Smart home automation
Local web UI
Cloud storage
AI image descriptions
```

Do not implement all of these now.

Keep the architecture extensible.

---

# 27. FINAL SUCCESS CRITERIA

The project is considered successful when a new user can:

1. Buy the supported ESP32-P4 board.
2. Connect a supported camera.
3. Install ESP-IDF.
4. Clone the repository.
5. Configure Wi-Fi.
6. Configure a Telegram bot.
7. Flash firmware.
8. Send `/start`.
9. Receive `/status`.
10. Press `📸 Photo`.
11. Receive a real camera image.
12. Save photos to MicroSD.
13. Record and receive a short video if supported by the selected hardware/software configuration.
14. Send an image to the bot.
15. Run AI detection.
16. Receive AI results.
17. Enable motion detection.
18. Receive a Telegram alert.
19. Learn each subsystem from the lessons.

---

# 28. FIRST IMPLEMENTATION TASK

Do NOT attempt all phases immediately.

Start by implementing ONLY:

```text
PHASE 0
PHASE 1
PHASE 2
```

The first milestone is:

```text
ESP32-P4
   ↓
Wi-Fi
   ↓
Telegram
   ↓
/start
/status
/help
```

Expected Telegram:

```text
/start

Welcome to TelegramP4!

ESP32-P4 Telegram Camera & IoT Platform

Use /help to see commands.
```

Then:

```text
/status

TelegramP4 Status

WiFi: Connected
Telegram: Connected
Board: FireBeetle 2 ESP32-P4
Uptime: ...
Memory: ...
```

Once this works on actual hardware, proceed to Phase 3.

---

# 29. IMPORTANT: WORKING STYLE

You are not being asked to merely write an explanation.

You are being asked to **build the actual project**.

For every phase:

1. inspect existing project
2. make changes
3. compile
4. fix errors
5. explain what changed
6. provide exact flash/test commands
7. document the lesson
8. stop at the phase boundary

Do not claim hardware success without actual hardware testing.

If a hardware dependency cannot be tested in the current environment, state:

> "Build verified; hardware test required."

Never fabricate a successful camera/AI/video test.

---

# 30. PROJECT PHILOSOPHY

The final project should feel like:

> **Arduino simplicity + ESP-IDF power + Telegram convenience + ESP32-P4 Edge AI.**

The most important goal is not just making the device work.

The goal is making the project easy enough that another person can clone it, understand it, modify it, and contribute to it.

Start with the smallest working system.

Then grow it into the complete TelegramP4 platform.
