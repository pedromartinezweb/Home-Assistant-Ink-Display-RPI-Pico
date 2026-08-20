#ifndef CONFIG_LOCAL_H
#define CONFIG_LOCAL_H

#define APP_WIFI_SSID ""
#define APP_WIFI_PASSWORD ""
#define APP_HA_HOST ""
#define APP_HA_PORT 8123
#define APP_HA_TOKEN ""
#define APP_REFRESH_SECONDS 300

#define APP_UI_TITLE "HOUSE"
#define APP_UI_UPDATED "ACT"

#define APP_VIEW_ITEMS \
    APP_ITEM("sensor.co2", "CO2", "PPM", 1, 0, 1000) \
    APP_ITEM("sensor.external_temperature", "EXT TEMP", "C", 1, 1, DASHBOARD_NO_RED) \
    APP_ITEM("sensor.temperature", "TEMP", "C", 2, 1, DASHBOARD_NO_RED) \
    APP_ITEM("sensor.humidity", "HUM", "%", 2, 0, DASHBOARD_NO_RED) \
    APP_ITEM("sensor.pm25", "PM2.5", "UG/M3", 2, 0, DASHBOARD_NO_RED)

#endif
