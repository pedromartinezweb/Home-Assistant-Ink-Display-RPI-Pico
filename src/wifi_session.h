#ifndef WIFI_SESSION_H
#define WIFI_SESSION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WIFI_SESSION_OK,
    WIFI_SESSION_ERROR_ARGUMENT,
    WIFI_SESSION_ERROR_INIT,
    WIFI_SESSION_ERROR_CONNECT
} WifiSessionStatus;

typedef struct {
    bool active;
} WifiSession;

WifiSessionStatus wifi_session_open(WifiSession *session,
                                    const char *ssid,
                                    const char *password,
                                    uint32_t timeout_ms);
void wifi_session_close(WifiSession *session);

#endif
