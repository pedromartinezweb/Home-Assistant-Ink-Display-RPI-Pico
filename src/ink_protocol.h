#ifndef INK_PROTOCOL_H
#define INK_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dashboard.h"

enum {
    INK_PAYLOAD_MAX = 768,
    INK_HOST_MAX = 63,
    INK_PATH_MAX = 95,
    INK_SECRET_SIZE = 32,
    INK_SECRET_HEX_SIZE = 64
};

typedef struct {
    uint64_t revision;
    uint32_t interval_seconds;
    char title[25];
    char updated[9];
    char labels[DASHBOARD_MAX_ITEMS][13];
    char units[DASHBOARD_MAX_ITEMS][6];
    DashboardItem items[DASHBOARD_MAX_ITEMS];
    DashboardData data;
    DashboardConfig config;
} InkFrame;

typedef struct {
    uint32_t code;
    char host[INK_HOST_MAX + 1];
    char path[INK_PATH_MAX + 1];
    uint16_t port;
    uint8_t secret[INK_SECRET_SIZE];
} InkPairRequest;

bool ink_protocol_frame_parse(char *payload, size_t length, InkFrame *frame);
bool ink_protocol_pair_parse(char *payload, size_t length, InkPairRequest *request);
bool ink_protocol_hex_decode(const char *text, uint8_t *output, size_t output_size);
void ink_protocol_hex_encode(const uint8_t *input, size_t input_size, char *output);

#endif
