#include "app.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "frame.h"
#include "factory_reset.h"
#include "hardware/watchdog.h"
#include "ink_client.h"
#include "log.h"
#include "pair_server.h"
#include "pico/rand.h"
#include "pico/stdio.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "wifi_session.h"

enum {
    WIFI_TIMEOUT_MS = 20000,
    RETRY_SECONDS = 60,
    DISPLAY_RETRY_SECONDS = 15,
    INITIAL_FRAME_RETRY_SECONDS = 5,
    DEFAULT_INTERVAL_SECONDS = 300,
    MAX_DISPLAY_FAILURES = 3,
    MAX_CONNECTIVITY_FAILURES = 6
};

static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

static void start_usb_maintenance(void) {
#if EPAPER_USB_MAINTENANCE || EPAPER_USB_LOGS
    stdio_init_all();
#endif
#if EPAPER_USB_LOGS
    absolute_time_t deadline = make_timeout_time_ms(5000);
    while (!stdio_usb_connected() && !time_reached(deadline)) {
        sleep_ms(10);
    }
#endif
}

static bool open_wifi(WifiSession *wifi, WifiSessionMode mode) {
    const char *ssids[] = {APP_WIFI_SSID, APP_WIFI_SSID_FALLBACK};
    for (size_t index = 0; index < sizeof(ssids) / sizeof(ssids[0]); ++index) {
        if (ssids[index][0] == '\0' ||
            (index > 0 && strcmp(ssids[index], ssids[0]) == 0)) {
            continue;
        }
        WifiSessionStatus status = wifi_session_open(wifi,
                                                      ssids[index],
                                                      APP_WIFI_PASSWORD,
                                                      WIFI_TIMEOUT_MS,
                                                      mode);
        if (status == WIFI_SESSION_OK) {
            return true;
        }
        APP_LOG("[%lu ms] WIFI_SSID_FAILED index=%u status=%d\n",
                now_ms(),
                (unsigned int)index,
                status);
    }
    return false;
}

static void restart_system(const char *reason, int status) {
    APP_LOG("[%lu ms] SYSTEM_RESTART reason=%s status=%d\n", now_ms(), reason, status);
    watchdog_reboot(0, 0, 100);
    for (;;) {
        tight_loop_contents();
    }
}

static EpdStatus present(App *app, const uint8_t *black, const uint8_t *red) {
    EpaperResult result = epaper_present(&app->epaper, black, red);
    APP_LOG("[%lu ms] PRESENT mode=%d status=%s bytes=%lu busy=%lu\n",
           now_ms(),
           result.mode,
           epd_status_name(result.driver.status),
           (unsigned long)result.bytes,
           (unsigned long)result.driver.busy_ms);
    return result.driver.status;
}

static EpdStatus present_pairing(App *app, const uint8_t *black, const uint8_t *red) {
    EpaperResult result = epaper_present(&app->epaper, black, red);
    APP_LOG("[%lu ms] PAIRING_PRESENT mode=%d status=%s bytes=%lu busy=%lu\n",
            now_ms(),
            result.mode,
            epd_status_name(result.driver.status),
            (unsigned long)result.bytes,
            (unsigned long)result.driver.busy_ms);
    return result.driver.status;
}

static void display_text(char *destination, size_t length, const char *source) {
    if (destination == NULL || length == 0) {
        return;
    }
    size_t index = 0;
    while (source != NULL && source[index] != '\0' && index + 1 < length) {
        destination[index] = (char)toupper((unsigned char)source[index]);
        ++index;
    }
    destination[index] = '\0';
}

static EpdStatus pairing_screen(App *app, uint32_t code, const WifiSession *wifi) {
    char text[7];
    char ssid[WIFI_SESSION_SSID_MAX_LENGTH + 1];
    char ip_address[WIFI_SESSION_IP_ADDRESS_LENGTH];
    bool connected = wifi != NULL && wifi->active;
    snprintf(text, sizeof(text), "%06lu", (unsigned long)code);
    display_text(ssid, sizeof(ssid), connected ? wifi->ssid : "WAITING");
    display_text(ip_address,
                 sizeof(ip_address),
                 connected ? wifi->ip_address : "WAITING");
    frame_clear(app->dashboard.black, false);
    frame_clear(app->dashboard.red, false);
    frame_fill_rect_landscape(app->dashboard.black, 1, 1, 248, 24, true);
    frame_text_landscape_color(app->dashboard.black, 8, 10, "HOME ASSISTANT", 1, false);
    if (connected) {
        frame_text_landscape(app->dashboard.black, 8, 33, "PAIR INTEGRATION", 2);
        frame_text_landscape(app->dashboard.black, 8, 58, "CODE", 1);
        frame_text_landscape(app->dashboard.red, 45, 52, text, 4);
        frame_text_landscape(app->dashboard.black, 8, 88, "SSID:", 1);
        frame_text_landscape(app->dashboard.black, 43, 88, ssid, 1);
        frame_text_landscape(app->dashboard.black, 8, 104, "IP:", 1);
        frame_text_landscape(app->dashboard.black, 29, 104, ip_address, 1);
    }
    return present_pairing(app, app->dashboard.black, app->dashboard.red);
}

static void pair_device(App *app,
                        const char *device_id,
                        uint32_t provisioning_id,
                        DeviceSettings *settings) {
    uint32_t code = (uint32_t)(get_rand_64() % 900000U) + 100000U;
    bool displayed = false;
    uint8_t display_failures = 0;
    while (!settings->paired) {
        WifiSession wifi;
        if (!open_wifi(&wifi, WIFI_SESSION_PAIRING)) {
            APP_LOG("[%lu ms] WIFI_ERROR status=%d\n", now_ms(), WIFI_SESSION_ERROR_CONNECT);
            if (factory_reset_sleep(RETRY_SECONDS * 1000U)) {
                code = (uint32_t)(get_rand_64() % 900000U) + 100000U;
                displayed = false;
            }
            continue;
        }
        if (!displayed) {
            epaper_force_full(&app->epaper);
            EpdStatus display_status = pairing_screen(app, code, &wifi);
            if (display_status != EPD_OK) {
                wifi_session_close(&wifi);
                display_failures++;
                APP_LOG("[%lu ms] PAIRING_DISPLAY_ERROR status=%s failures=%u\n",
                        now_ms(),
                        epd_status_name(display_status),
                        (unsigned int)display_failures);
                if (display_failures >= MAX_DISPLAY_FAILURES) {
                    restart_system("pairing_display", display_status);
                }
                if (factory_reset_sleep(DISPLAY_RETRY_SECONDS * 1000U)) {
                    code = (uint32_t)(get_rand_64() % 900000U) + 100000U;
                    displayed = false;
                }
                continue;
            }
            display_failures = 0;
            APP_LOG("[%lu ms] PAIRING_READY device=%s code=%06lu ssid=%s ip=%s\n",
                    now_ms(),
                    device_id,
                    (unsigned long)code,
                    wifi.ssid,
                    wifi.ip_address);
            displayed = true;
        }
        PairServerConfig config = {
            .device_id = device_id,
            .code = code,
            .provisioning_id = provisioning_id,
            .settings = settings
        };
        bool paired = pair_server_run(&config);
        wifi_session_close(&wifi);
        if (paired) {
            APP_LOG("[%lu ms] PAIRING_OK device=%s\n", now_ms(), device_id);
            return;
        }
        APP_LOG("[%lu ms] PAIRING_WAIT device=%s\n", now_ms(), device_id);
    }
}

static EpdStatus update_display(App *app, const InkFrame *frame) {
    if (!dashboard_draw(&app->dashboard, &frame->data, &frame->config)) {
        return EPD_ERROR_ARGUMENT;
    }
    return present(app, app->dashboard.black, app->dashboard.red);
}

static void run_paired(App *app, DeviceSettings *settings) {
    uint64_t revision = 0;
    uint32_t interval_seconds = INITIAL_FRAME_RETRY_SECONDS;
    uint8_t display_failures = 0;
    uint8_t connectivity_failures = 0;
    for (;;) {
        WifiSession wifi;
        if (!open_wifi(&wifi, WIFI_SESSION_POLLING)) {
            connectivity_failures++;
            APP_LOG("[%lu ms] WIFI_ERROR status=%d\n", now_ms(), WIFI_SESSION_ERROR_CONNECT);
            if (connectivity_failures >= MAX_CONNECTIVITY_FAILURES) {
                restart_system("wifi", WIFI_SESSION_ERROR_CONNECT);
            }
            if (factory_reset_sleep(RETRY_SECONDS * 1000U)) {
                return;
            }
            continue;
        }

        InkFrame next;
        InkClientStatus client_status = ink_client_poll(settings, revision, &next);
        wifi_session_close(&wifi);
        if (client_status == INK_CLIENT_UPDATED) {
            connectivity_failures = 0;
            EpdStatus display_status = update_display(app, &next);
            if (display_status != EPD_OK) {
                display_failures++;
                epaper_force_full(&app->epaper);
                interval_seconds = DISPLAY_RETRY_SECONDS;
                APP_LOG("[%lu ms] FRAME_REJECTED revision=%llu status=%s failures=%u\n",
                        now_ms(),
                        (unsigned long long)next.revision,
                        epd_status_name(display_status),
                        (unsigned int)display_failures);
                if (display_failures >= MAX_DISPLAY_FAILURES) {
                    restart_system("display", display_status);
                }
            } else {
                display_failures = 0;
                revision = next.revision;
                interval_seconds = next.interval_seconds;
                APP_LOG("[%lu ms] FRAME_OK revision=%llu items=%u interval=%lu\n",
                        now_ms(),
                        (unsigned long long)revision,
                        (unsigned int)next.data.count,
                        (unsigned long)interval_seconds);
            }
        } else if (client_status == INK_CLIENT_UNCHANGED) {
            connectivity_failures = 0;
            APP_LOG("[%lu ms] FRAME_UNCHANGED revision=%llu\n",
                    now_ms(),
                    (unsigned long long)revision);
            if (revision == 0) {
                interval_seconds = INITIAL_FRAME_RETRY_SECONDS;
            }
        } else {
            connectivity_failures++;
            APP_LOG("[%lu ms] POLL_ERROR status=%d\n", now_ms(), client_status);
            interval_seconds = RETRY_SECONDS;
            if (connectivity_failures >= MAX_CONNECTIVITY_FAILURES) {
                restart_system("poll", client_status);
            }
        }
        if (factory_reset_sleep(interval_seconds * 1000U)) {
            return;
        }
    }
}

void app_run(App *app, const EpdConfig *display) {
    if (app == NULL || display == NULL || APP_WIFI_SSID[0] == '\0') {
        restart_system("configuration", EPD_ERROR_ARGUMENT);
    }
    APP_LOG("[%lu ms] START version=1 transport=ha-poll signed=1\n", now_ms());
    EpdStatus status = epaper_open(&app->epaper, display);
    if (status != EPD_OK) {
        restart_system("display_open", status);
    }
    start_usb_maintenance();
    factory_reset_boot();

    char device_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    pico_get_unique_board_id_string(device_id, sizeof(device_id));
    for (;;) {
        DeviceSettings settings;
        device_store_load(APP_PROVISIONING_ID, &settings);
        if (!settings.paired) {
            pair_device(app, device_id, APP_PROVISIONING_ID, &settings);
        }
        run_paired(app, &settings);
    }
}
