/**
 * telegramp4_board — board-specific identity and (future) pin definitions.
 *
 * Target board: DFRobot FireBeetle 2 ESP32-P4 AI Vision Board
 * https://www.dfrobot.com/product-2915.html
 *
 * Exact GPIO mappings for camera/SD/mic/display/PIR/GPIO-whitelist are added here
 * phase by phase (05, 07, 11, 17, 19, 20) only after being verified against current
 * DFRobot/Espressif documentation — see docs/hardware.md for what is confirmed so
 * far. Do not add pin numbers here from memory/guesswork.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define TELEGRAMP4_BOARD_NAME       "FireBeetle 2 ESP32-P4"
#define TELEGRAMP4_FIRMWARE_VERSION "0.1.0"

/**
 * Logs board identity and firmware version. Call once at startup.
 */
void telegramp4_board_print_banner(void);

#ifdef __cplusplus
}
#endif
