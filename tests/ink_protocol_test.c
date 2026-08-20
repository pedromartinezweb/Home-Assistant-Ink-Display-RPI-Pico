#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "ink_protocol.h"

static void frame_test(void) {
    char payload[INK_PAYLOAD_MAX] =
        "INK1\n"
        "42\n"
        "300\n"
        "18\n"
        "07\n"
        "HOUSE\n"
        "ACT\n"
        "2\n"
        "1|0|1000|1201000|1|CO2|PPM\n"
        "2|1|2147483647|23400|1|TEMP|C\n";
    InkFrame frame;
    assert(ink_protocol_frame_parse(payload, strlen(payload), &frame));
    assert(frame.revision == 42);
    assert(frame.interval_seconds == 300);
    assert(frame.data.count == 2);
    assert(frame.data.values_milli[0] == 1201000);
    assert(frame.data.valid[0]);
    assert(strcmp(frame.config.items[1].label, "TEMP") == 0);
    assert(frame.config.items[1].row == 2);
}

static void invalid_frame_test(void) {
    char short_interval[INK_PAYLOAD_MAX] =
        "INK1\n1\n59\n10\n20\nHOUSE\nACT\n2\n"
        "1|0|1000|1|1|A|C\n2|0|1000|1|1|B|C\n";
    InkFrame frame;
    assert(!ink_protocol_frame_parse(short_interval, strlen(short_interval), &frame));

    char bad_row[INK_PAYLOAD_MAX] =
        "INK1\n1\n60\n10\n20\nHOUSE\nACT\n2\n"
        "3|0|1000|1|1|A|C\n2|0|1000|1|1|B|C\n";
    assert(!ink_protocol_frame_parse(bad_row, strlen(bad_row), &frame));
}

static void pair_test(void) {
    char payload[INK_PAYLOAD_MAX] =
        "PAIR1\n"
        "123456\n"
        "192.168.1.20\n"
        "8123\n"
        "/api/ha_ink_display/poll/device\n"
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f\n";
    InkPairRequest request;
    assert(ink_protocol_pair_parse(payload, strlen(payload), &request));
    assert(request.code == 123456);
    assert(request.port == 8123);
    assert(strcmp(request.host, "192.168.1.20") == 0);
    assert(request.secret[0] == 0x00);
    assert(request.secret[31] == 0x1f);
}

int main(void) {
    frame_test();
    invalid_frame_test();
    pair_test();
    return 0;
}
