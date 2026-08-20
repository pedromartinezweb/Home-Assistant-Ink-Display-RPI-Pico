#ifndef HOME_ASSISTANT_H
#define HOME_ASSISTANT_H

#include <stddef.h>
#include <stdint.h>

enum {
    HOME_ASSISTANT_MAX_ENTITIES = 8
};

typedef struct {
    const char *host;
    uint16_t port;
    const char *token;
} HomeAssistantConfig;

typedef struct {
    int values_milli[HOME_ASSISTANT_MAX_ENTITIES];
    size_t count;
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
                                        const char *const *entities,
                                        size_t count,
                                        HomeAssistantReading *reading);

#endif
