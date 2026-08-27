#ifndef WIFI_SESSION_H
#define WIFI_SESSION_H

#include <stdbool.h>
#include <stdint.h>

enum {
    WIFI_SESSION_SSID_MAX_LENGTH = 32,
    WIFI_SESSION_IP_ADDRESS_LENGTH = 16
};

typedef enum {
    WIFI_SESSION_OK,
    WIFI_SESSION_ERROR_ARGUMENT,
    WIFI_SESSION_ERROR_INIT,
    WIFI_SESSION_ERROR_CONNECT,
    WIFI_SESSION_ERROR_UNSTABLE
} WifiSessionStatus;

typedef enum {
    WIFI_SESSION_PAIRING,
    WIFI_SESSION_POLLING
} WifiSessionMode;

typedef struct {
    bool active;
    char ssid[WIFI_SESSION_SSID_MAX_LENGTH + 1];
    char ip_address[WIFI_SESSION_IP_ADDRESS_LENGTH];
} WifiSession;

WifiSessionStatus wifi_session_open(WifiSession *session,
                                    const char *ssid,
                                    const char *password,
                                    uint32_t timeout_ms,
                                    WifiSessionMode mode);
void wifi_session_close(WifiSession *session);

#endif
