#include "wifi_session.h"

#include <stdio.h>
#include <string.h>

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

enum {
    PAIRING_STABLE_MS = 1500,
    LINK_SAMPLE_MS = 50
};

static bool wait_for_stable_link(uint32_t timeout_ms) {
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    uint32_t stable_ms = 0;
    while (!time_reached(deadline)) {
        cyw43_arch_poll();
        if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
            stable_ms += LINK_SAMPLE_MS;
            if (stable_ms >= PAIRING_STABLE_MS) {
                return true;
            }
        } else {
            stable_ms = 0;
        }
        sleep_ms(LINK_SAMPLE_MS);
    }
    return false;
}

WifiSessionStatus wifi_session_open(WifiSession *session,
                                    const char *ssid,
                                    const char *password,
                                    uint32_t timeout_ms,
                                    WifiSessionMode mode) {
    if (session == NULL || ssid == NULL || password == NULL ||
        ssid[0] == '\0' || timeout_ms == 0) {
        return WIFI_SESSION_ERROR_ARGUMENT;
    }

    memset(session, 0, sizeof(*session));
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_SPAIN) != 0) {
        return WIFI_SESSION_ERROR_INIT;
    }

    cyw43_arch_enable_sta_mode();
    cyw43_wifi_pm(&cyw43_state,
                  mode == WIFI_SESSION_PAIRING
                      ? CYW43_NO_POWERSAVE_MODE
                      : CYW43_AGGRESSIVE_PM);
    if (cyw43_arch_wifi_connect_timeout_ms(
            ssid, password, CYW43_AUTH_WPA2_AES_PSK, timeout_ms) != 0) {
        cyw43_arch_disable_sta_mode();
        cyw43_arch_deinit();
        return WIFI_SESSION_ERROR_CONNECT;
    }
    if (mode == WIFI_SESSION_PAIRING && !wait_for_stable_link(timeout_ms)) {
        cyw43_arch_disable_sta_mode();
        cyw43_arch_deinit();
        return WIFI_SESSION_ERROR_UNSTABLE;
    }

    snprintf(session->ssid, sizeof(session->ssid), "%s", ssid);
    if (netif_default == NULL ||
        ip4addr_ntoa_r(netif_ip4_addr(netif_default),
                       session->ip_address,
                       sizeof(session->ip_address)) == NULL) {
        snprintf(session->ip_address, sizeof(session->ip_address), "UNAVAILABLE");
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
