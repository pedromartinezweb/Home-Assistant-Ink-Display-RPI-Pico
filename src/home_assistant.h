#ifndef HOME_ASSISTANT_H
#define HOME_ASSISTANT_H

#include <stdint.h>

typedef struct {
    const char *host;
    uint16_t port;
    const char *token;
    const char *temperature;
    const char *humidity;
    const char *co2;
    const char *pm25;
} HomeAssistantConfig;

typedef struct {
    int temperature_tenths;
    int humidity;
    int co2;
    int pm25;
    int hour;
    int minute;
    int second;
} HomeAssistantReading;

typedef enum {
    HOME_ASSISTANT_OK,
    HOME_ASSISTANT_NOT_CONFIGURED,
    HOME_ASSISTANT_NETWORK_ERROR,
    HOME_ASSISTANT_RESPONSE_ERROR
} HomeAssistantStatus;

HomeAssistantStatus home_assistant_read(const HomeAssistantConfig *config,
                                        HomeAssistantReading *reading);

#endif
