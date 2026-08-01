#ifndef OTA_TRUSTED_KEY_H
#define OTA_TRUSTED_KEY_H

#include <stdint.h>

#define OTA_TRUSTED_PUBLIC_KEY_SIZE 64u

extern const uint32_t g_ota_trusted_key_id;
extern const uint8_t
    g_ota_trusted_public_key[OTA_TRUSTED_PUBLIC_KEY_SIZE];

#endif
