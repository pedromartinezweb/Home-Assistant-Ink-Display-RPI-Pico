#include "ink_protocol.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static char *next_line(char **cursor) {
    if (cursor == NULL || *cursor == NULL) {
        return NULL;
    }
    char *line = *cursor;
    char *end = strchr(line, '\n');
    if (end == NULL) {
        *cursor = NULL;
    } else {
        *end = '\0';
        *cursor = end + 1;
    }
    size_t length = strlen(line);
    if (length > 0 && line[length - 1] == '\r') {
        line[length - 1] = '\0';
    }
    return line;
}

static bool line_is(char **cursor, const char *expected) {
    char *line = next_line(cursor);
    return line != NULL && strcmp(line, expected) == 0;
}

static bool unsigned_value(const char *text, uint64_t max, uint64_t *value) {
    if (text == NULL || text[0] == '\0' || text[0] == '-') {
        return false;
    }
    errno = 0;
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > max) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool signed_value(const char *text, int32_t *value) {
    if (text == NULL || text[0] == '\0') {
        return false;
    }
    errno = 0;
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX) {
        return false;
    }
    *value = (int32_t)parsed;
    return true;
}

static bool copy_text(char *target, size_t size, const char *source, bool empty) {
    if (source == NULL || (!empty && source[0] == '\0') || strlen(source) >= size) {
        return false;
    }
    memcpy(target, source, strlen(source) + 1);
    return true;
}

static bool item_parse(char *line, InkFrame *frame, size_t index) {
    char *fields[7];
    char *cursor = line;
    for (size_t field = 0; field < 7; ++field) {
        fields[field] = cursor;
        char *separator = field < 6 ? strchr(cursor, '|') : NULL;
        if (field < 6 && separator == NULL) {
            return false;
        }
        if (separator != NULL) {
            *separator = '\0';
            cursor = separator + 1;
        }
    }
    if (strchr(fields[6], '|') != NULL) {
        return false;
    }

    uint64_t row;
    uint64_t decimals;
    uint64_t valid;
    int32_t red_above;
    int32_t value_milli;
    if (!unsigned_value(fields[0], 2, &row) || row < 1 ||
        !unsigned_value(fields[1], 2, &decimals) ||
        !signed_value(fields[2], &red_above) ||
        !signed_value(fields[3], &value_milli) ||
        !unsigned_value(fields[4], 1, &valid) ||
        !copy_text(frame->labels[index], sizeof(frame->labels[index]), fields[5], false) ||
        !copy_text(frame->units[index], sizeof(frame->units[index]), fields[6], true)) {
        return false;
    }

    frame->items[index].label = frame->labels[index];
    frame->items[index].unit = frame->units[index];
    frame->items[index].row = (uint8_t)row;
    frame->items[index].decimals = (uint8_t)decimals;
    frame->items[index].red_above = red_above;
    frame->data.values_milli[index] = value_milli;
    frame->data.valid[index] = valid != 0;
    return true;
}

bool ink_protocol_frame_parse(char *payload, size_t length, InkFrame *frame) {
    if (payload == NULL || frame == NULL || length == 0 || length >= INK_PAYLOAD_MAX) {
        return false;
    }
    payload[length] = '\0';
    memset(frame, 0, sizeof(*frame));
    char *cursor = payload;
    if (!line_is(&cursor, "INK1")) {
        return false;
    }

    uint64_t revision;
    uint64_t interval;
    uint64_t hour;
    uint64_t minute;
    uint64_t count;
    if (!unsigned_value(next_line(&cursor), UINT64_MAX, &revision) || revision == 0 ||
        !unsigned_value(next_line(&cursor), 86400, &interval) || interval < 60 ||
        !unsigned_value(next_line(&cursor), 23, &hour) ||
        !unsigned_value(next_line(&cursor), 59, &minute) ||
        !copy_text(frame->title, sizeof(frame->title), next_line(&cursor), false) ||
        !copy_text(frame->updated, sizeof(frame->updated), next_line(&cursor), false) ||
        !unsigned_value(next_line(&cursor), DASHBOARD_MAX_ITEMS, &count) || count < 2) {
        return false;
    }

    frame->revision = revision;
    frame->interval_seconds = (uint32_t)interval;
    frame->data.hour = (int)hour;
    frame->data.minute = (int)minute;
    frame->data.count = (size_t)count;
    for (size_t index = 0; index < count; ++index) {
        char *line = next_line(&cursor);
        if (line == NULL || !item_parse(line, frame, index)) {
            return false;
        }
    }
    if (cursor != NULL && cursor[0] != '\0') {
        return false;
    }

    frame->config.title = frame->title;
    frame->config.updated = frame->updated;
    frame->config.items = frame->items;
    frame->config.count = (size_t)count;
    return dashboard_config_valid(&frame->config);
}

static int hex_value(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

bool ink_protocol_hex_decode(const char *text, uint8_t *output, size_t output_size) {
    if (text == NULL || output == NULL || strlen(text) != output_size * 2) {
        return false;
    }
    for (size_t index = 0; index < output_size; ++index) {
        int high = hex_value(text[index * 2]);
        int low = hex_value(text[index * 2 + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        output[index] = (uint8_t)((high << 4) | low);
    }
    return true;
}

void ink_protocol_hex_encode(const uint8_t *input, size_t input_size, char *output) {
    static const char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < input_size; ++index) {
        output[index * 2] = digits[input[index] >> 4];
        output[index * 2 + 1] = digits[input[index] & 0x0f];
    }
    output[input_size * 2] = '\0';
}

bool ink_protocol_pair_parse(char *payload, size_t length, InkPairRequest *request) {
    if (payload == NULL || request == NULL || length == 0 || length >= INK_PAYLOAD_MAX) {
        return false;
    }
    payload[length] = '\0';
    memset(request, 0, sizeof(*request));
    char *cursor = payload;
    uint64_t code;
    uint64_t port;
    if (!line_is(&cursor, "PAIR1") ||
        !unsigned_value(next_line(&cursor), 999999, &code) || code < 100000 ||
        !copy_text(request->host, sizeof(request->host), next_line(&cursor), false) ||
        !unsigned_value(next_line(&cursor), UINT16_MAX, &port) || port == 0 ||
        !copy_text(request->path, sizeof(request->path), next_line(&cursor), false) ||
        request->path[0] != '/' ||
        !ink_protocol_hex_decode(next_line(&cursor), request->secret, sizeof(request->secret)) ||
        (cursor != NULL && cursor[0] != '\0')) {
        return false;
    }
    request->code = (uint32_t)code;
    request->port = (uint16_t)port;
    return true;
}
