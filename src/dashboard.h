#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "epd.h"

enum {
    DASHBOARD_MAX_ITEMS = 8,
    DASHBOARD_ALERT_OFF = 0,
    DASHBOARD_ALERT_ABOVE = 1,
    DASHBOARD_ALERT_BELOW = 2
};

typedef struct {
    const char *label;
    const char *unit;
    uint8_t row;
    uint8_t decimals;
    uint8_t alert_mode;
    int alert_threshold_milli;
} DashboardItem;

typedef struct {
    int values_milli[DASHBOARD_MAX_ITEMS];
    bool valid[DASHBOARD_MAX_ITEMS];
    size_t count;
    int hour;
    int minute;
} DashboardData;

typedef struct {
    const char *title;
    const char *updated;
    const DashboardItem *items;
    size_t count;
} DashboardConfig;

typedef struct {
    uint8_t black[EPD_BUFFER_SIZE];
    uint8_t red[EPD_BUFFER_SIZE];
} Dashboard;

bool dashboard_config_valid(const DashboardConfig *config);
bool dashboard_draw(Dashboard *dashboard, const DashboardData *data, const DashboardConfig *config);

#endif
