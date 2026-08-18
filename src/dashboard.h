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

bool dashboard_draw(Dashboard *dashboard, const Reading *reading);

#endif
