#ifndef DEVICE_STORE_H
#define DEVICE_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "ink_protocol.h"

typedef struct {
    uint32_t provisioning_id;
    bool paired;
    char host[INK_HOST_MAX + 1];
    char path[INK_PATH_MAX + 1];
    uint16_t port;
    uint8_t secret[INK_SECRET_SIZE];
} DeviceSettings;

void device_store_load(uint32_t provisioning_id, DeviceSettings *settings);
bool device_store_pair(uint32_t provisioning_id,
                       const InkPairRequest *request,
                       DeviceSettings *settings);

#endif
