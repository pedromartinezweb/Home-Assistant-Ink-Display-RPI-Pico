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

static DashboardItem items[] = {
    {.label = "CO2", .unit = "PPM", .row = 1, .decimals = 0, .red_above = 1000},
    {.label = "EXT TEMP", .unit = "C", .row = 1, .decimals = 1, .red_above = DASHBOARD_NO_RED},
    {.label = "TEMP", .unit = "C", .row = 2, .decimals = 1, .red_above = DASHBOARD_NO_RED},
    {.label = "HUM", .unit = "%", .row = 2, .decimals = 0, .red_above = DASHBOARD_NO_RED},
    {.label = "PM2.5", .unit = "UG/M3", .row = 2, .decimals = 0, .red_above = DASHBOARD_NO_RED}
};

static DashboardConfig config = {
    .title = "HOUSE",
    .updated = "ACT",
    .items = items,
    .count = 5
};

static DashboardData data = {
    .values_milli = {612000, 18700, 23400, 48000, 14000},
    .valid = {true, true, true, true, true},
    .count = 5,
    .hour = 10,
    .minute = 24
};

int main(void) {
    Dashboard dashboard;
    assert(!dashboard_draw(NULL, &data, &config));
    assert(!dashboard_draw(&dashboard, NULL, &config));
    assert(!dashboard_draw(&dashboard, &data, NULL));
    assert(dashboard_config_valid(&config));
    assert(dashboard_draw(&dashboard, &data, &config));
    assert(pixel(dashboard.black, 10, 29));
    assert(!pixel(dashboard.red, 10, 29));
    assert(pixel(dashboard.black, 8, 38));
    assert(!pixel(dashboard.red, 8, 38));

    Dashboard next;
    DashboardData changed = data;
    changed.values_milli[2] = 23500;
    assert(dashboard_draw(&next, &changed, &config));

    FrameRegion region;
    assert(frame_diff_region(dashboard.black, dashboard.red, next.black, next.red, &region));
    assert(region.width > 0);
    assert(region.height > 0);
    assert(region.x % 8 == 0);
    assert(region.width % 8 == 0);
    assert(!frame_diff_region(next.black, next.red, next.black, next.red, &region));

    DashboardData next_minute = data;
    next_minute.minute++;
    assert(dashboard_draw(&next, &next_minute, &config));
    assert(frame_diff_region(dashboard.black, dashboard.red, next.black, next.red, &region));
    assert(region.x < 24);

    DashboardData threshold = data;
    threshold.values_milli[0] = 1000000;
    assert(dashboard_draw(&next, &threshold, &config));
    assert(pixel(next.black, 8, 38));
    assert(!pixel(next.red, 8, 38));

    threshold.values_milli[0] = 1001000;
    assert(dashboard_draw(&next, &threshold, &config));
    assert(!pixel(next.black, 8, 38));
    assert(pixel(next.red, 8, 38));

    DashboardItem two_items[] = {
        {.label = "ONE", .unit = "", .row = 1, .decimals = 2, .red_above = DASHBOARD_NO_RED},
        {.label = "TWO", .unit = "%", .row = 2, .decimals = 0, .red_above = DASHBOARD_NO_RED}
    };
    DashboardConfig two_config = {.title = "HOUSE", .updated = "ACT", .items = two_items, .count = 2};
    DashboardData two_data = {
        .values_milli = {-1250, 50000},
        .valid = {true, true},
        .count = 2,
        .hour = 10,
        .minute = 24
    };
    assert(dashboard_config_valid(&two_config));
    assert(dashboard_draw(&next, &two_data, &two_config));

    DashboardItem eight_items[] = {
        {.label = "A", .unit = "C", .row = 1, .decimals = 1, .red_above = DASHBOARD_NO_RED},
        {.label = "B", .unit = "%", .row = 1, .decimals = 0, .red_above = DASHBOARD_NO_RED},
        {.label = "C", .unit = "PPM", .row = 1, .decimals = 0, .red_above = 1000},
        {.label = "D", .unit = "", .row = 1, .decimals = 2, .red_above = DASHBOARD_NO_RED},
        {.label = "E", .unit = "C", .row = 2, .decimals = 1, .red_above = DASHBOARD_NO_RED},
        {.label = "F", .unit = "%", .row = 2, .decimals = 0, .red_above = DASHBOARD_NO_RED},
        {.label = "G", .unit = "PPM", .row = 2, .decimals = 0, .red_above = DASHBOARD_NO_RED},
        {.label = "H", .unit = "", .row = 2, .decimals = 2, .red_above = DASHBOARD_NO_RED}
    };
    DashboardConfig eight_config = {.title = "HOUSE", .updated = "ACT", .items = eight_items, .count = 8};
    DashboardData eight_data = {
        .values_milli = {21000, 50000, 900000, 1230, 22000, 51000, 901000, 4560},
        .valid = {true, true, true, true, true, true, true, true},
        .count = 8,
        .hour = 10,
        .minute = 24
    };
    assert(dashboard_config_valid(&eight_config));
    assert(dashboard_draw(&next, &eight_data, &eight_config));

    eight_items[4].row = 1;
    assert(!dashboard_config_valid(&eight_config));
    assert(!dashboard_draw(&next, &eight_data, &eight_config));

    eight_items[4].row = 2;
    eight_items[0].label = "TOO LONGXX";
    assert(!dashboard_config_valid(&eight_config));
    return 0;
}
