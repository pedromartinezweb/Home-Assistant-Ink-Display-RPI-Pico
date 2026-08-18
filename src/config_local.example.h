#ifndef CONFIG_LOCAL_H
#define CONFIG_LOCAL_H

#define APP_WIFI_SSID ""
#define APP_WIFI_PASSWORD ""
#define APP_HA_HOST ""
#define APP_HA_PORT 8123
#define APP_HA_TOKEN ""
#define APP_HA_TEMPERATURE "sensor.temperature"
#define APP_HA_HUMIDITY "sensor.humidity"
#define APP_HA_CO2 "sensor.co2"
#define APP_HA_PM25 "sensor.pm25"
#define APP_REFRESH_SECONDS 300

#define APP_UI_TITLE "INDOOR AIR"
#define APP_UI_UPDATED "ACT"
#define APP_UI_STATUS "STATUS"
#define APP_UI_GOOD "GOOD"
#define APP_UI_FAIR "FAIR"
#define APP_UI_HIGH "HIGH"
#define APP_UI_CO2 "CO2"
#define APP_UI_CO2_UNIT "PPM"
#define APP_UI_TEMPERATURE "TEMP"
#define APP_UI_TEMPERATURE_UNIT "C"
#define APP_UI_HUMIDITY "HUM"
#define APP_UI_HUMIDITY_UNIT "%"
#define APP_UI_PM25 "PM2.5"
#define APP_UI_PM25_UNIT "UG/M3"
#define APP_CO2_GOOD_MAX 800
#define APP_CO2_FAIR_MAX 1200

#endif
