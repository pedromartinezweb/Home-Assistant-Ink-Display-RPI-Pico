#include "dashboard.h"

#include <stdio.h>
#include <string.h>

#include "frame.h"

enum {
    UNIT_GAP = 4
};

static bool valid(const Reading *reading) {
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

static const char *air_quality(int co2) {
    if (co2 <= 800) {
        return "BUENO";
    }
    if (co2 <= 1200) {
        return "MEDIO";
    }
    return "ALTO";
}

static void hero(Dashboard *dashboard, const char *value, const char *status) {
    int scale = strlen(value) > 4 ? 3 : 4;
    frame_text_landscape(dashboard->red, 8, 39, "CO2", 1);
    frame_text_landscape(dashboard->red, 8, 50, value, scale);
    int unit_x = 8 + text_width(value, scale) + UNIT_GAP;
    frame_text_landscape(dashboard->red, unit_x, scale == 4 ? 69 : 62, "PPM", 1);
    frame_text_landscape(dashboard->black, 145, 41, "ESTADO", 1);
    frame_text_landscape(dashboard->black, 145, 54, status, 2);
}

static void header(Dashboard *dashboard, const Reading *reading) {
    char time[12];
    snprintf(time, sizeof(time), "ACT %02d:%02d", reading->hour, reading->minute);
    frame_fill_rect_landscape(dashboard->black, 1, 1, 248, 27, true);
    frame_text_landscape_color(dashboard->black, 8, 10, "AIRE INTERIOR", 1, false);
    frame_text_landscape_color(dashboard->black, 188, 10, time, 1, false);
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

static void draw_metrics(Dashboard *dashboard, const Reading *reading) {
    char value[20];
    temperature(value, sizeof(value), reading->temperature_tenths);
    metric(dashboard, 8, "TEMP", value, "C");

    snprintf(value, sizeof(value), "%d", reading->humidity);
    metric(dashboard, 91, "HUM", value, "%");

    snprintf(value, sizeof(value), "%d", reading->pm25);
    metric(dashboard, 174, "PM2.5", value, "UG/M3");
}

static void draw_air(Dashboard *dashboard, int co2) {
    char value[20];
    snprintf(value, sizeof(value), "%d", co2);
    hero(dashboard, value, air_quality(co2));
}

static void clear(Dashboard *dashboard) {
    frame_clear(dashboard->black, false);
    frame_clear(dashboard->red, false);
}

bool dashboard_draw(Dashboard *dashboard, const Reading *reading) {
    if (dashboard == NULL || !valid(reading)) {
        return false;
    }

    clear(dashboard);
    header(dashboard, reading);
    layout(dashboard);
    draw_air(dashboard, reading->co2);
    draw_metrics(dashboard, reading);
    return true;
}
