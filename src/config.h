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

#ifndef APP_REFRESH_SECONDS
#define APP_REFRESH_SECONDS 300
#endif

#endif
