#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "dashboard.h"
#include "frame.h"

static bool pixel(const uint8_t *buffer, int x, int y) {
    int panel_x = y;
    int panel_y = EPD_HEIGHT - 1 - x;
    int index = panel_y * EPD_ROW_BYTES + panel_x / 8;
    uint8_t mask = (uint8_t)(0x80U >> (panel_x % 8));
    return (buffer[index] & mask) == 0;
}

int main(void) {
    Dashboard dashboard;
    DashboardConfig config = {
        .title = "INDOOR AIR",
        .updated = "ACT",
        .external_temperature = "EXT TEMP",
        .external_temperature_unit = "C",
        .co2 = "CO2",
        .co2_unit = "PPM",
        .temperature = "TEMP",
        .temperature_unit = "C",
        .humidity = "HUM",
        .humidity_unit = "%",
        .pm25 = "PM2.5",
        .pm25_unit = "UG/M3",
        .co2_red_above = 1000
    };
    Reading valid = {
        .temperature_tenths = 234,
        .humidity = 48,
        .co2 = 612,
        .pm25 = 14,
        .external_temperature_tenths = 187,
        .hour = 10,
        .minute = 24
    };
    Reading invalid = valid;
    invalid.humidity = 101;

    assert(!dashboard_draw(NULL, &valid, &config));
    assert(!dashboard_draw(&dashboard, NULL, &config));
    assert(!dashboard_draw(&dashboard, &valid, NULL));
    assert(!dashboard_draw(&dashboard, &invalid, &config));
    assert(dashboard_draw(&dashboard, &valid, &config));
    assert(pixel(dashboard.black, 20, 24));
    assert(!pixel(dashboard.red, 20, 24));
    assert(!pixel(dashboard.black, 10, 29));
    assert(pixel(dashboard.red, 10, 29));
    assert(pixel(dashboard.black, 132, 40));
    assert(!pixel(dashboard.red, 132, 40));
    assert(pixel(dashboard.black, 8, 40));
    assert(!pixel(dashboard.red, 8, 40));

    Dashboard next;
    Reading changed = valid;
    changed.temperature_tenths = 235;
    assert(dashboard_draw(&next, &changed, &config));

    FrameRegion region;
    assert(frame_diff_region(dashboard.black, dashboard.red, next.black, next.red, &region));
    assert(region.width > 0);
    assert(region.height > 0);
    assert(region.x % 8 == 0);
    assert(region.width % 8 == 0);
    assert(!frame_diff_region(next.black, next.red, next.black, next.red, &region));

    Reading next_minute = valid;
    next_minute.minute++;
    assert(dashboard_draw(&next, &next_minute, &config));
    assert(frame_diff_region(dashboard.black, dashboard.red, next.black, next.red, &region));
    assert(region.x < 24);

    invalid = valid;
    invalid.hour = 24;
    assert(!dashboard_draw(&dashboard, &invalid, &config));

    Dashboard customized;
    config.title = "OFFICE AIR";
    config.temperature = "ROOM";
    assert(dashboard_draw(&customized, &valid, &config));
    assert(frame_diff_region(dashboard.black,
                             dashboard.red,
                             customized.black,
                             customized.red,
                             &region));

    Reading threshold_co2 = valid;
    threshold_co2.co2 = 1000;
    assert(dashboard_draw(&customized, &threshold_co2, &config));
    assert(pixel(customized.black, 8, 40));
    assert(!pixel(customized.red, 8, 40));

    Reading high_co2 = valid;
    high_co2.co2 = 1001;
    assert(dashboard_draw(&customized, &high_co2, &config));
    assert(!pixel(customized.black, 8, 40));
    assert(pixel(customized.red, 8, 40));

    config.co2_red_above = -1;
    assert(!dashboard_draw(&dashboard, &valid, &config));
    return 0;
}
