#ifndef PAIR_SERVER_H
#define PAIR_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#include "device_store.h"

typedef struct {
    const char *device_id;
    uint32_t code;
    uint32_t provisioning_id;
    DeviceSettings *settings;
} PairServerConfig;

bool pair_server_run(const PairServerConfig *config);

#endif
