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

#ifndef APP_HA_TEMPERATURE
#define APP_HA_TEMPERATURE ""
#endif

#ifndef APP_HA_HUMIDITY
#define APP_HA_HUMIDITY ""
#endif

#ifndef APP_HA_CO2
#define APP_HA_CO2 ""
#endif

#ifndef APP_HA_PM25
#define APP_HA_PM25 ""
#endif

#ifndef APP_HA_EXTERNAL_TEMPERATURE
#define APP_HA_EXTERNAL_TEMPERATURE ""
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

#ifndef APP_UI_EXTERNAL_TEMPERATURE
#define APP_UI_EXTERNAL_TEMPERATURE "EXT TEMP"
#endif

#ifndef APP_UI_EXTERNAL_TEMPERATURE_UNIT
#define APP_UI_EXTERNAL_TEMPERATURE_UNIT "C"
#endif

#ifndef APP_UI_CO2
#define APP_UI_CO2 "CO2"
#endif

#ifndef APP_UI_CO2_UNIT
#define APP_UI_CO2_UNIT "PPM"
#endif

#ifndef APP_UI_TEMPERATURE
#define APP_UI_TEMPERATURE "TEMP"
#endif

#ifndef APP_UI_TEMPERATURE_UNIT
#define APP_UI_TEMPERATURE_UNIT "C"
#endif

#ifndef APP_UI_HUMIDITY
#define APP_UI_HUMIDITY "HUM"
#endif

#ifndef APP_UI_HUMIDITY_UNIT
#define APP_UI_HUMIDITY_UNIT "%"
#endif

#ifndef APP_UI_PM25
#define APP_UI_PM25 "PM2.5"
#endif

#ifndef APP_UI_PM25_UNIT
#define APP_UI_PM25_UNIT "UG/M3"
#endif

#ifndef APP_CO2_RED_ABOVE
#define APP_CO2_RED_ABOVE 1000
#endif

#endif
