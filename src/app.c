#include "app.h"

#include <stdio.h>

#include "config.h"
#include "frame.h"
#include "ink_client.h"
#include "pair_server.h"
#include "pico/rand.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "wifi_session.h"

enum {
    WIFI_TIMEOUT_MS = 20000,
    PAIR_WINDOW_MS = 300000,
    RETRY_SECONDS = 60,
    DEFAULT_INTERVAL_SECONDS = 300
};

static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

static void halt(EpdStatus status) {
    for (;;) {
        printf("[%lu ms] SYSTEM_HALTED status=%s\n", now_ms(), epd_status_name(status));
        sleep_ms(1000);
    }
}

static void present(App *app, const uint8_t *black, const uint8_t *red) {
    EpaperResult result = epaper_present(&app->epaper, black, red);
    printf("[%lu ms] PRESENT mode=%d status=%s bytes=%lu busy=%lu\n",
           now_ms(),
           result.mode,
           epd_status_name(result.driver.status),
           (unsigned long)result.bytes,
           (unsigned long)result.driver.busy_ms);
    if (result.driver.status != EPD_OK) {
        halt(result.driver.status);
    }
}

static void pairing_screen(App *app, uint32_t code) {
    char text[7];
    snprintf(text, sizeof(text), "%06lu", (unsigned long)code);
    frame_clear(app->dashboard.black, false);
    frame_clear(app->dashboard.red, false);
    frame_fill_rect_landscape(app->dashboard.black, 1, 1, 248, 30, true);
    frame_text_landscape_color(app->dashboard.black, 8, 10, "HOME ASSISTANT", 1, false);
    frame_text_landscape(app->dashboard.black, 8, 42, "PAIR INTEGRATION", 2);
    frame_text_landscape(app->dashboard.black, 8, 69, "CODE", 1);
    frame_text_landscape(app->dashboard.red, 52, 65, text, 4);
    frame_text_landscape(app->dashboard.black, 8, 104, "WAITING FOR HOME ASSISTANT", 1);
    present(app, app->dashboard.black, app->dashboard.red);
}

static bool pair_device(App *app,
                        const char *device_id,
                        uint32_t provisioning_id,
                        DeviceSettings *settings) {
    uint32_t code = (uint32_t)(get_rand_64() % 900000U) + 100000U;
    bool ready = false;
    while (!settings->paired) {
        WifiSession wifi;
        WifiSessionStatus wifi_status = wifi_session_open(&wifi,
                                                           APP_WIFI_SSID,
                                                           APP_WIFI_PASSWORD,
                                                           WIFI_TIMEOUT_MS);
        if (wifi_status != WIFI_SESSION_OK) {
            printf("[%lu ms] WIFI_ERROR status=%d\n", now_ms(), wifi_status);
            sleep_ms(RETRY_SECONDS * 1000U);
            continue;
        }
        if (!ready) {
            pairing_screen(app, code);
            printf("[%lu ms] PAIRING_READY device=%s code=%06lu\n",
                   now_ms(),
                   device_id,
                   (unsigned long)code);
            ready = true;
        }
        PairServerConfig config = {
            .device_id = device_id,
            .code = code,
            .provisioning_id = provisioning_id,
            .settings = settings
        };
        bool paired = pair_server_run(&config, PAIR_WINDOW_MS);
        wifi_session_close(&wifi);
        if (paired) {
            printf("[%lu ms] PAIRING_OK device=%s\n", now_ms(), device_id);
            return true;
        }
        printf("[%lu ms] PAIRING_WAIT device=%s\n", now_ms(), device_id);
    }
    return true;
}

static bool update_display(App *app, const InkFrame *frame) {
    if (!dashboard_draw(&app->dashboard, &frame->data, &frame->config)) {
        return false;
    }
    present(app, app->dashboard.black, app->dashboard.red);
    return true;
}

void app_run(App *app, const EpdConfig *display) {
    if (app == NULL || display == NULL || APP_WIFI_SSID[0] == '\0') {
        halt(EPD_ERROR_ARGUMENT);
    }
    printf("[%lu ms] START version=1 transport=ha-poll signed=1\n", now_ms());
    EpdStatus status = epaper_open(&app->epaper, display);
    if (status != EPD_OK) {
        halt(status);
    }

    char device_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    pico_get_unique_board_id_string(device_id, sizeof(device_id));
    DeviceSettings settings;
    device_store_load(APP_PROVISIONING_ID, &settings);
    if (!settings.paired && !pair_device(app, device_id, APP_PROVISIONING_ID, &settings)) {
        halt(EPD_ERROR_ARGUMENT);
    }

    uint64_t revision = 0;
    uint32_t interval_seconds = DEFAULT_INTERVAL_SECONDS;
    for (;;) {
        WifiSession wifi;
        WifiSessionStatus wifi_status = wifi_session_open(&wifi,
                                                           APP_WIFI_SSID,
                                                           APP_WIFI_PASSWORD,
                                                           WIFI_TIMEOUT_MS);
        if (wifi_status != WIFI_SESSION_OK) {
            printf("[%lu ms] WIFI_ERROR status=%d\n", now_ms(), wifi_status);
            sleep_ms(RETRY_SECONDS * 1000U);
            continue;
        }

        InkFrame next;
        InkClientStatus client_status = ink_client_poll(&settings, revision, &next);
        wifi_session_close(&wifi);
        if (client_status == INK_CLIENT_UPDATED) {
            if (!update_display(app, &next)) {
                printf("[%lu ms] FRAME_REJECTED revision=%llu\n",
                       now_ms(),
                       (unsigned long long)next.revision);
            } else {
                revision = next.revision;
                interval_seconds = next.interval_seconds;
                printf("[%lu ms] FRAME_OK revision=%llu items=%u interval=%lu\n",
                       now_ms(),
                       (unsigned long long)revision,
                       (unsigned int)next.data.count,
                       (unsigned long)interval_seconds);
            }
        } else if (client_status == INK_CLIENT_UNCHANGED) {
            printf("[%lu ms] FRAME_UNCHANGED revision=%llu\n",
                   now_ms(),
                   (unsigned long long)revision);
        } else {
            printf("[%lu ms] POLL_ERROR status=%d\n", now_ms(), client_status);
            interval_seconds = RETRY_SECONDS;
        }
        sleep_ms(interval_seconds * 1000U);
    }
}
