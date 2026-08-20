#include "app.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "home_assistant.h"
#include "pico/stdlib.h"
#include "wifi_session.h"

enum {
    STARTUP_RETRY_SECONDS = 30
};

#define APP_ITEM(entity_value, label_value, unit_value, row_value, decimals_value, red_value) entity_value,
static const char *const entities[] = {
    APP_VIEW_ITEMS
};
#undef APP_ITEM

#define APP_ITEM(entity_value, label_value, unit_value, row_value, decimals_value, red_value) \
    {.label = label_value, \
     .unit = unit_value, \
     .row = row_value, \
     .decimals = decimals_value, \
     .red_above = red_value},
static const DashboardItem items[] = {
    APP_VIEW_ITEMS
};
#undef APP_ITEM

enum {
    ITEM_COUNT = sizeof(items) / sizeof(items[0])
};

_Static_assert((int)ITEM_COUNT <= (int)DASHBOARD_MAX_ITEMS, "Too many dashboard items");
_Static_assert((int)DASHBOARD_MAX_ITEMS == (int)HOME_ASSISTANT_MAX_ENTITIES, "Entity capacity mismatch");

static const DashboardConfig dashboard_config = {
    .title = APP_UI_TITLE,
    .updated = APP_UI_UPDATED,
    .items = items,
    .count = ITEM_COUNT
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

static bool configured(void) {
    if (APP_HA_HOST[0] == '\0' || APP_HA_TOKEN[0] == '\0' ||
        !dashboard_config_valid(&dashboard_config)) {
        return false;
    }
    for (size_t index = 0; index < ITEM_COUNT; ++index) {
        if (entities[index] == NULL || entities[index][0] == '\0') {
            return false;
        }
    }
    return true;
}

static void probe_wifi(void) {
    WifiSession wifi;
    WifiSessionStatus status = wifi_session_open(&wifi,
                                                  APP_WIFI_SSID,
                                                  APP_WIFI_PASSWORD,
                                                  15000);
    printf("[%lu ms] WIFI_PROBE status=%d\n", now_ms(), status);
    if (status == WIFI_SESSION_OK) {
        wifi_session_close(&wifi);
    }
}

static bool fetch_data(DashboardData *data) {
    if (data == NULL || !configured()) {
        return false;
    }

    WifiSession wifi;
    WifiSessionStatus wifi_status = wifi_session_open(&wifi,
                                                       APP_WIFI_SSID,
                                                       APP_WIFI_PASSWORD,
                                                       15000);
    if (wifi_status != WIFI_SESSION_OK) {
        printf("[%lu ms] WIFI_ERROR status=%d\n", now_ms(), wifi_status);
        return false;
    }

    HomeAssistantConfig config = {
        .host = APP_HA_HOST,
        .port = APP_HA_PORT,
        .token = APP_HA_TOKEN
    };
    HomeAssistantReading reading;
    HomeAssistantStatus status = home_assistant_read(&config, entities, ITEM_COUNT, &reading);
    wifi_session_close(&wifi);

    if (status != HOME_ASSISTANT_OK) {
        printf("[%lu ms] HOME_ASSISTANT_ERROR status=%d\n", now_ms(), status);
        return false;
    }

    memcpy(data->values_milli, reading.values_milli, sizeof(int) * ITEM_COUNT);
    data->count = reading.count;
    data->hour = reading.hour;
    data->minute = reading.minute;
    printf("[%lu ms] HOME_ASSISTANT_OK time=%02d:%02d items=%u\n",
           now_ms(),
           data->hour,
           data->minute,
           (unsigned int)data->count);
    for (size_t index = 0; index < data->count; ++index) {
        printf("[%lu ms] ITEM index=%u label=%s value_milli=%d\n",
               now_ms(),
               (unsigned int)index,
               items[index].label != NULL ? items[index].label : "",
               data->values_milli[index]);
    }
    return true;
}

static void present(App *app) {
    if (!dashboard_draw(&app->dashboard, &app->data, &dashboard_config)) {
        halt(EPD_ERROR_ARGUMENT);
    }

    EpaperResult result = epaper_present(&app->epaper,
                                         app->dashboard.black,
                                         app->dashboard.red);
    printf("[%lu ms] PRESENT mode=%d status=%s bytes=%lu total=%lu busy=%lu sleep=%d\n",
           now_ms(),
           result.mode,
           epd_status_name(result.driver.status),
           (unsigned long)result.bytes,
           (unsigned long)result.driver.elapsed_ms,
           (unsigned long)result.driver.busy_ms,
           app->epaper.driver.sleeping);
    if (result.driver.status != EPD_OK) {
        halt(result.driver.status);
    }
}

void app_run(App *app, const EpdConfig *config) {
    if (app == NULL || config == NULL) {
        halt(EPD_ERROR_ARGUMENT);
    }

    printf("[%lu ms] PRODUCTION_START v27 items=%u epaper=ondemand wifi=session ha=rest\n",
           now_ms(),
           (unsigned int)ITEM_COUNT);
    EpdStatus status = epaper_open(&app->epaper, config);
    if (status != EPD_OK) {
        halt(status);
    }

    if (!configured()) {
        probe_wifi();
    }

    for (;;) {
        uint32_t cycle_start = now_ms();
        if (fetch_data(&app->data)) {
            app->has_reading = true;
        }
        if (app->has_reading) {
            present(app);
        } else {
            printf("[%lu ms] WAITING_FOR_DATA display_unchanged\n", now_ms());
        }
        uint32_t interval_seconds = app->has_reading ? APP_REFRESH_SECONDS : STARTUP_RETRY_SECONDS;
        uint32_t interval_ms = interval_seconds * 1000U;
        uint32_t elapsed_ms = now_ms() - cycle_start;
        if (elapsed_ms < interval_ms) {
            sleep_ms(interval_ms - elapsed_ms);
        }
    }
}
