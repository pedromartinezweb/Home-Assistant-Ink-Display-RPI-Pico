#include "wifi_session.h"

#include "pico/cyw43_arch.h"

WifiSessionStatus wifi_session_open(WifiSession *session,
                                    const char *ssid,
                                    const char *password,
                                    uint32_t timeout_ms) {
    if (session == NULL || ssid == NULL || password == NULL || ssid[0] == '\0' || timeout_ms == 0) {
        return WIFI_SESSION_ERROR_ARGUMENT;
    }

    session->active = false;
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_SPAIN) != 0) {
        return WIFI_SESSION_ERROR_INIT;
    }

    cyw43_arch_enable_sta_mode();
    cyw43_wifi_pm(&cyw43_state, CYW43_AGGRESSIVE_PM);
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, timeout_ms) != 0) {
        cyw43_arch_disable_sta_mode();
        cyw43_arch_deinit();
        return WIFI_SESSION_ERROR_CONNECT;
    }

    session->active = true;
    return WIFI_SESSION_OK;
}

void wifi_session_close(WifiSession *session) {
    if (session == NULL || !session->active) {
        return;
    }

    cyw43_arch_disable_sta_mode();
    cyw43_arch_deinit();
    session->active = false;
}
