#ifndef INK_CLIENT_H
#define INK_CLIENT_H

#include <stdint.h>

#include "device_store.h"
#include "ink_protocol.h"

typedef enum {
    INK_CLIENT_UPDATED,
    INK_CLIENT_UNCHANGED,
    INK_CLIENT_NETWORK_ERROR,
    INK_CLIENT_RESPONSE_ERROR,
    INK_CLIENT_AUTH_ERROR
} InkClientStatus;

InkClientStatus ink_client_poll(const DeviceSettings *settings,
                                uint64_t revision,
                                InkFrame *frame);

#endif
