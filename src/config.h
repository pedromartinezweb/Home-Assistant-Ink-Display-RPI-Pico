#ifndef CONFIG_H
#define CONFIG_H

#include "config_local.h"

#ifndef APP_HA_HOST
#define APP_HA_HOST ""
#endif

#ifndef APP_HA_PORT
#define APP_HA_PORT 8123
#endif

#ifndef APP_HA_TOKEN
#define APP_HA_TOKEN ""
#endif

#ifndef APP_REFRESH_SECONDS
#define APP_REFRESH_SECONDS 300
#endif

#ifndef APP_UI_TITLE
#define APP_UI_TITLE "HOUSE"
#endif

#ifndef APP_UI_UPDATED
#define APP_UI_UPDATED "ACT"
#endif

#ifndef APP_VIEW_ITEMS
#define APP_VIEW_ITEMS \
    APP_ITEM("sensor.co2", "CO2", "PPM", 1, 0, 1000) \
    APP_ITEM("sensor.external_temperature", "EXT TEMP", "C", 1, 1, DASHBOARD_NO_RED) \
    APP_ITEM("sensor.temperature", "TEMP", "C", 2, 1, DASHBOARD_NO_RED) \
    APP_ITEM("sensor.humidity", "HUM", "%", 2, 0, DASHBOARD_NO_RED) \
    APP_ITEM("sensor.pm25", "PM2.5", "UG/M3", 2, 0, DASHBOARD_NO_RED)
#endif

#endif
