/**
 * telegramp4_ota — firmware updates via esp_https_ota (Phase 21).
 *
 * Unlike camera/audio/video/AI/motion/display, this is standard ESP-IDF
 * functionality that doesn't depend on unverified board specifics - the
 * two-OTA-slot partition table was reserved back in Phase 0 specifically so
 * this would work without repartitioning. Manual/local OTA (you provide the
 * .bin URL) is in scope for this phase; fully automatic remote OTA (e.g.
 * periodic update checks against a release server) is future work.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Downloads and flashes the firmware image at `url` (HTTPS, cert-bundle
 * validated - never disabled) to the inactive OTA partition, verifies it,
 * marks it bootable, and reboots into it on success. Does not return on
 * success. TELEGRAMP4_OTA_ENABLED must be set in Kconfig or this returns
 * ESP_ERR_NOT_SUPPORTED without attempting anything - flashing arbitrary
 * firmware from a URL is powerful enough to be opt-in.
 */
esp_err_t telegramp4_ota_update_from_url(const char *url);

#ifdef __cplusplus
}
#endif
