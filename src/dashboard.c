#include "dashboard.h"

#include <stdio.h>
#include <string.h>

#include "frame.h"

enum {
    UNIT_GAP = 4
};

static bool valid_reading(const Reading *reading) {
    return reading != NULL &&
           reading->temperature_tenths >= -500 &&
           reading->temperature_tenths <= 1000 &&
           reading->humidity >= 0 &&
           reading->humidity <= 100 &&
           reading->co2 >= 0 &&
           reading->co2 <= 99999 &&
           reading->pm25 >= 0 &&
           reading->pm25 <= 9999 &&
           reading->hour >= 0 &&
           reading->hour <= 23 &&
           reading->minute >= 0 &&
           reading->minute <= 59;
}

static bool valid_text(const char *text, size_t max) {
    return text != NULL && text[0] != '\0' && strlen(text) <= max;
}

static bool valid_config(const DashboardConfig *config) {
    return config != NULL &&
           valid_text(config->title, 24) &&
           valid_text(config->updated, 8) &&
           valid_text(config->status, 16) &&
           valid_text(config->good, 8) &&
           valid_text(config->fair, 8) &&
           valid_text(config->high, 8) &&
           valid_text(config->co2, 8) &&
           valid_text(config->co2_unit, 5) &&
           valid_text(config->temperature, 12) &&
           valid_text(config->temperature_unit, 5) &&
           valid_text(config->humidity, 12) &&
           valid_text(config->humidity_unit, 5) &&
           valid_text(config->pm25, 12) &&
           valid_text(config->pm25_unit, 5) &&
           config->co2_good_max >= 0 &&
           config->co2_fair_max > config->co2_good_max;
}

static int text_width(const char *text, int scale) {
    return (int)strlen(text) * 6 * scale;
}

static void metric(Dashboard *dashboard, int x, const char *label, const char *value, const char *unit) {
    int scale = 2;
    if (text_width(value, scale) + UNIT_GAP + text_width(unit, 1) > 74) {
        scale = 1;
    }
    frame_text_landscape(dashboard->black, x, 86, label, 1);
    frame_text_landscape(dashboard->black, x, 98, value, scale);
    int unit_x = x + text_width(value, scale) + UNIT_GAP;
    frame_text_landscape(dashboard->black, unit_x, scale == 2 ? 105 : 98, unit, 1);
}

static const char *air_quality(int co2, const DashboardConfig *config) {
    if (co2 <= config->co2_good_max) {
        return config->good;
    }
    if (co2 <= config->co2_fair_max) {
        return config->fair;
    }
    return config->high;
}

static void hero(Dashboard *dashboard,
                 const DashboardConfig *config,
                 const char *value,
                 const char *status) {
    int scale = strlen(value) > 4 ? 3 : 4;
    frame_text_landscape(dashboard->red, 8, 39, config->co2, 1);
    frame_text_landscape(dashboard->red, 8, 50, value, scale);
    int unit_x = 8 + text_width(value, scale) + UNIT_GAP;
    frame_text_landscape(dashboard->red, unit_x, scale == 4 ? 69 : 62, config->co2_unit, 1);
    frame_text_landscape(dashboard->black, 145, 41, config->status, 1);
    frame_text_landscape(dashboard->black, 145, 54, status, 2);
}

static void header(Dashboard *dashboard, const Reading *reading, const DashboardConfig *config) {
    char time[32];
    snprintf(time, sizeof(time), "%s %02d:%02d", config->updated, reading->hour, reading->minute);
    int time_x = 242 - text_width(time, 1);
    frame_fill_rect_landscape(dashboard->black, 1, 1, 248, 27, true);
    frame_text_landscape_color(dashboard->black, 8, 10, config->title, 1, false);
    frame_text_landscape_color(dashboard->black, time_x, 10, time, 1, false);
    frame_fill_rect_landscape(dashboard->red, 1, 28, 248, 3, true);
}

static void layout(Dashboard *dashboard) {
    frame_line_landscape(dashboard->black, 132, 38, 132, 76, true);
    frame_line_landscape(dashboard->black, 8, 80, 241, 80, true);
    frame_line_landscape(dashboard->black, 83, 86, 83, 116, true);
    frame_line_landscape(dashboard->black, 166, 86, 166, 116, true);
}

static void temperature(char *value, size_t size, int tenths) {
    int magnitude = tenths < 0 ? -tenths : tenths;
    snprintf(value, size, "%s%d.%d", tenths < 0 ? "-" : "", magnitude / 10, magnitude % 10);
}

static void draw_metrics(Dashboard *dashboard, const Reading *reading, const DashboardConfig *config) {
    char value[20];
    temperature(value, sizeof(value), reading->temperature_tenths);
    metric(dashboard, 8, config->temperature, value, config->temperature_unit);

    snprintf(value, sizeof(value), "%d", reading->humidity);
    metric(dashboard, 91, config->humidity, value, config->humidity_unit);

    snprintf(value, sizeof(value), "%d", reading->pm25);
    metric(dashboard, 174, config->pm25, value, config->pm25_unit);
}

static void draw_air(Dashboard *dashboard, int co2, const DashboardConfig *config) {
    char value[20];
    snprintf(value, sizeof(value), "%d", co2);
    hero(dashboard, config, value, air_quality(co2, config));
}

static void clear(Dashboard *dashboard) {
    frame_clear(dashboard->black, false);
    frame_clear(dashboard->red, false);
}

bool dashboard_draw(Dashboard *dashboard, const Reading *reading, const DashboardConfig *config) {
    if (dashboard == NULL || !valid_reading(reading) || !valid_config(config)) {
        return false;
    }

    clear(dashboard);
    header(dashboard, reading, config);
    layout(dashboard);
    draw_air(dashboard, reading->co2, config);
    draw_metrics(dashboard, reading, config);
    return true;
}
