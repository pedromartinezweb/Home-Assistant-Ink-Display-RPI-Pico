#include "epaper.h"

#include <string.h>

static EpaperResult result(EpdStatus status) {
    EpaperResult value = {
        .driver = {
            .status = status,
            .elapsed_ms = 0,
            .busy_ms = 0,
            .attempts = 0
        },
        .mode = EPAPER_PRESENT_NONE,
        .region = {0},
        .bytes = 0
    };
    return value;
}

EpdStatus epaper_open(Epaper *epaper, const EpdConfig *config) {
    if (epaper == NULL || config == NULL) {
        return EPD_ERROR_ARGUMENT;
    }

    epaper->has_frame = false;
    memset(epaper->black, 0xFF, sizeof(epaper->black));
    memset(epaper->red, 0xFF, sizeof(epaper->red));
    return epd_open(&epaper->driver, config);
}

void epaper_force_full(Epaper *epaper) {
    if (epaper != NULL) {
        epaper->has_frame = false;
    }
}

EpaperResult epaper_present(Epaper *epaper, const uint8_t *black, const uint8_t *red) {
    if (epaper == NULL || black == NULL || red == NULL || !epaper->driver.open) {
        return result(EPD_ERROR_ARGUMENT);
    }

    EpaperResult value = result(EPD_OK);
    if (!epaper->has_frame) {
        value.mode = EPAPER_PRESENT_FULL;
        value.region = (FrameRegion){.x = 0, .y = 0, .width = EPD_WIDTH, .height = EPD_HEIGHT};
        value.bytes = 2U * EPD_BUFFER_SIZE;
        value.driver = epd_draw(&epaper->driver, black, red);
    } else if (frame_diff_region(epaper->black, epaper->red, black, red, &value.region)) {
        value.mode = EPAPER_PRESENT_PARTIAL;
        value.bytes = 2U * (value.region.width / 8U) * value.region.height;
        value.driver = epd_draw_partial(&epaper->driver,
                                        black,
                                        red,
                                        value.region.x,
                                        value.region.y,
                                        value.region.width,
                                        value.region.height);
        if (value.driver.status != EPD_OK) {
            epaper->has_frame = false;
            value.mode = EPAPER_PRESENT_FULL;
            value.region = (FrameRegion){.x = 0, .y = 0, .width = EPD_WIDTH, .height = EPD_HEIGHT};
            value.bytes = 2U * EPD_BUFFER_SIZE;
            value.driver = epd_draw(&epaper->driver, black, red);
        }
    }

    if (value.driver.status == EPD_OK && value.mode != EPAPER_PRESENT_NONE) {
        memcpy(epaper->black, black, EPD_BUFFER_SIZE);
        memcpy(epaper->red, red, EPD_BUFFER_SIZE);
        epaper->has_frame = true;
    }
    return value;
}
