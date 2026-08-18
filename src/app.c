#include "app.h"

#include <stdio.h>

#include "config.h"
#include "home_assistant.h"
#include "pico/stdlib.h"
#include "wifi_session.h"

enum {
    STARTUP_RETRY_SECONDS = 30
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

static bool home_assistant_configured(void) {
    return APP_HA_HOST[0] != '\0' && APP_HA_TOKEN[0] != '\0' &&
           APP_HA_TEMPERATURE[0] != '\0' && APP_HA_HUMIDITY[0] != '\0' &&
           APP_HA_CO2[0] != '\0' && APP_HA_PM25[0] != '\0';
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

static bool fetch_reading(Reading *reading) {
    if (reading == NULL || !home_assistant_configured()) {
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
        .token = APP_HA_TOKEN,
        .temperature = APP_HA_TEMPERATURE,
        .humidity = APP_HA_HUMIDITY,
        .co2 = APP_HA_CO2,
        .pm25 = APP_HA_PM25
    };
    HomeAssistantReading value;
    HomeAssistantStatus status = home_assistant_read(&config, &value);
    wifi_session_close(&wifi);

    if (status != HOME_ASSISTANT_OK) {
        printf("[%lu ms] HOME_ASSISTANT_ERROR status=%d\n", now_ms(), status);
        return false;
    }

    reading->temperature_tenths = value.temperature_tenths;
    reading->humidity = value.humidity;
    reading->co2 = value.co2;
    reading->pm25 = value.pm25;
    reading->hour = value.hour;
    reading->minute = value.minute;
    printf("[%lu ms] HOME_ASSISTANT_OK time=%02d:%02d temperature=%d humidity=%d co2=%d pm25=%d\n",
           now_ms(),
           reading->hour,
           reading->minute,
           reading->temperature_tenths,
           reading->humidity,
           reading->co2,
           reading->pm25);
    return true;
}

static void present(App *app) {
    if (!dashboard_draw(&app->dashboard, &app->reading)) {
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

    printf("[%lu ms] PRODUCTION_START v26 epaper=ondemand wifi=session ha=rest\n", now_ms());
    EpdStatus status = epaper_open(&app->epaper, config);
    if (status != EPD_OK) {
        halt(status);
    }

    if (!home_assistant_configured()) {
        probe_wifi();
    }

    for (;;) {
        uint32_t cycle_start = now_ms();
        if (fetch_reading(&app->reading)) {
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
