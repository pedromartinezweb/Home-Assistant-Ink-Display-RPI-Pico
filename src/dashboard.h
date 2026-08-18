#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "epd.h"

typedef struct {
    int temperature_tenths;
    int humidity;
    int co2;
    int pm25;
    int hour;
    int minute;
} Reading;

typedef struct {
    uint8_t black[EPD_BUFFER_SIZE];
    uint8_t red[EPD_BUFFER_SIZE];
} Dashboard;

typedef struct {
    const char *title;
    const char *updated;
    const char *status;
    const char *good;
    const char *fair;
    const char *high;
    const char *co2;
    const char *co2_unit;
    const char *temperature;
    const char *temperature_unit;
    const char *humidity;
    const char *humidity_unit;
    const char *pm25;
    const char *pm25_unit;
    int co2_good_max;
    int co2_fair_max;
} DashboardConfig;

bool dashboard_draw(Dashboard *dashboard, const Reading *reading, const DashboardConfig *config);

#endif
