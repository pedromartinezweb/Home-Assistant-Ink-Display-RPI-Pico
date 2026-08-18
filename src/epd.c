#include "epd.h"

#include <stdio.h>

#include "pico/stdlib.h"

enum {
    RESET_MS = 50,
    RESET_TIMEOUT_MS = 5000,
    BUSY_START_TIMEOUT_MS = 1000,
    REFRESH_TIMEOUT_MS = 40000,
    RETRIES = 2,
    RECOVERY_BAUD = 100000
};

typedef struct {
    const uint8_t *black;
    const uint8_t *red;
} EpdImage;

static uint32_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

static EpdReport report(EpdStatus status, uint8_t attempts) {
    EpdReport value = {
        .status = status,
        .elapsed_ms = 0,
        .busy_ms = 0,
        .attempts = attempts
    };
    return value;
}

static void select(const Epd *epd, bool active) {
    gpio_put(epd->config.cs, !active);
}

static bool write_byte(const Epd *epd, bool is_data, uint8_t value) {
    gpio_put(epd->config.dc, is_data);
    select(epd, true);
    int written = spi_write_blocking(epd->config.spi, &value, 1);
    select(epd, false);
    return written == 1;
}

static bool cmd(const Epd *epd, uint8_t value) {
    return write_byte(epd, false, value);
}

static bool dat(const Epd *epd, uint8_t value) {
    return write_byte(epd, true, value);
}

static bool cmd1(const Epd *epd, uint8_t command, uint8_t value) {
    return cmd(epd, command) && dat(epd, value);
}

static EpdStatus wait_level(const Epd *epd, bool level, uint32_t timeout_ms, EpdStatus timeout) {
    uint32_t start = now_ms();
    while ((bool)gpio_get(epd->config.busy) != level) {
        if (now_ms() - start >= timeout_ms) {
            return timeout;
        }
        sleep_ms(1);
    }
    return EPD_OK;
}

static EpdStatus reset(const Epd *epd) {
    select(epd, false);
    gpio_put(epd->config.dc, true);
    gpio_put(epd->config.rst, true);
    sleep_ms(10);
    gpio_put(epd->config.rst, false);
    sleep_ms(RESET_MS);
    gpio_put(epd->config.rst, true);
    sleep_ms(10);
    printf("[%lu ms] HARD_RESET complete BUSY=%d\n", now_ms(), gpio_get(epd->config.busy));
    return EPD_OK;
}

static bool sleep_panel(Epd *epd) {
    if (!cmd1(epd, 0x10, 0x01)) {
        return false;
    }
    sleep_ms(1);
    epd->sleeping = true;
    return true;
}

static bool valid_region(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    return width > 0 && height > 0 && x % 8 == 0 && width % 8 == 0 &&
           x + width <= EPD_WIDTH && y + height <= EPD_HEIGHT;
}

static bool set_area(const Epd *epd, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    uint16_t y_end = y + height - 1;
    return cmd(epd, 0x44) &&
           dat(epd, (uint8_t)(x / 8)) &&
           dat(epd, (uint8_t)((x + width - 1) / 8)) &&
           cmd(epd, 0x45) &&
           dat(epd, (uint8_t)y) &&
           dat(epd, (uint8_t)(y >> 8)) &&
           dat(epd, (uint8_t)y_end) &&
           dat(epd, (uint8_t)(y_end >> 8)) &&
           cmd1(epd, 0x4E, (uint8_t)(x / 8)) &&
           cmd(epd, 0x4F) &&
           dat(epd, (uint8_t)y) &&
           dat(epd, (uint8_t)(y >> 8));
}

static EpdStatus configure(const Epd *epd) {
    sleep_ms(10);
    if (!cmd(epd, 0x12)) {
        return EPD_ERROR_SPI;
    }
    printf("[%lu ms] SWRESET sent BUSY=%d\n", now_ms(), gpio_get(epd->config.busy));
    sleep_ms(10);

    EpdStatus status = wait_level(epd, false, RESET_TIMEOUT_MS, EPD_ERROR_BUSY_IDLE);
    if (status != EPD_OK) {
        printf("[%lu ms] SWRESET BUSY_not_released=%d\n", now_ms(), gpio_get(epd->config.busy));
        return status;
    }

    bool written = cmd(epd, 0x01) &&
                   dat(epd, 0xF9) &&
                   dat(epd, 0x00) &&
                   dat(epd, 0x00) &&
                   cmd1(epd, 0x11, 0x03) &&
                   cmd1(epd, 0x3C, 0x05) &&
                   cmd1(epd, 0x18, 0x80) &&
                   cmd(epd, 0x21) &&
                   dat(epd, 0x00) &&
                   dat(epd, 0x80) &&
                   set_area(epd, 0, 0, EPD_WIDTH, EPD_HEIGHT);
    return written ? EPD_OK : EPD_ERROR_SPI;
}

static bool write_rows(const Epd *epd, uint8_t target, const EpdImage *image) {
    if (!cmd(epd, target)) {
        return false;
    }

    gpio_put(epd->config.dc, true);
    select(epd, true);
    bool valid = true;
    uint8_t row[EPD_ROW_BYTES];
    for (int y = 0; y < EPD_HEIGHT && valid; ++y) {
        for (int x = 0; x < EPD_ROW_BYTES; ++x) {
            int index = y * EPD_ROW_BYTES + x;
            row[x] = target == 0x24 ? image->black[index] : (uint8_t)~image->red[index];
        }
        valid = spi_write_blocking(epd->config.spi, row, EPD_ROW_BYTES) == EPD_ROW_BYTES;
    }
    select(epd, false);
    return valid;
}

static bool write_region(const Epd *epd,
                         uint8_t target,
                         const EpdImage *image,
                         uint16_t x,
                         uint16_t y,
                         uint16_t width,
                         uint16_t height) {
    if (!cmd(epd, target)) {
        return false;
    }

    uint16_t start_byte = x / 8;
    uint16_t bytes = width / 8;
    uint8_t row[EPD_ROW_BYTES];
    gpio_put(epd->config.dc, true);
    select(epd, true);
    bool valid = true;

    for (uint16_t row_index = 0; row_index < height && valid; ++row_index) {
        int offset = (y + row_index) * EPD_ROW_BYTES + start_byte;
        for (uint16_t byte = 0; byte < bytes; ++byte) {
            row[byte] = target == 0x24 ? image->black[offset + byte] : (uint8_t)~image->red[offset + byte];
        }
        valid = spi_write_blocking(epd->config.spi, row, bytes) == bytes;
    }

    select(epd, false);
    return valid;
}

static EpdReport refresh(const Epd *epd, uint8_t attempt) {
    EpdReport value = report(EPD_OK, attempt);
    uint32_t start = now_ms();

    if (!cmd1(epd, 0x22, 0xF7) || !cmd(epd, 0x20)) {
        value.status = EPD_ERROR_SPI;
        return value;
    }

    EpdStatus status = wait_level(epd, true, BUSY_START_TIMEOUT_MS, EPD_ERROR_BUSY_START);
    if (status != EPD_OK) {
        value.status = status;
        value.elapsed_ms = now_ms() - start;
        return value;
    }

    uint32_t busy_start = now_ms();
    status = wait_level(epd, false, REFRESH_TIMEOUT_MS, EPD_ERROR_BUSY_END);
    value.status = status;
    value.busy_ms = now_ms() - busy_start;
    value.elapsed_ms = now_ms() - start;
    return value;
}

static EpdReport update(Epd *epd, const EpdImage *image) {
    if (epd == NULL || image == NULL || !epd->open) {
        return report(EPD_ERROR_ARGUMENT, 0);
    }

    EpdReport value = report(EPD_ERROR_ARGUMENT, 0);
    for (uint8_t attempt = 1; attempt <= RETRIES; ++attempt) {
        uint32_t update_start = now_ms();
        uint32_t requested_baud = attempt == 1 ? epd->config.baud : RECOVERY_BAUD;
        uint32_t actual_baud = spi_set_baudrate(epd->config.spi, requested_baud);
        printf("[%lu ms] UPDATE attempt=%u spi=%lu\n",
               now_ms(),
               attempt,
               (unsigned long)actual_baud);
        EpdStatus status = reset(epd);
        if (status == EPD_OK) {
            epd->sleeping = false;
            status = configure(epd);
        }
        if (status == EPD_OK) {
            uint32_t transfer_start = now_ms();
            if (!write_rows(epd, 0x24, image) || !write_rows(epd, 0x26, image)) {
                status = EPD_ERROR_SPI;
            }
            printf("[%lu ms] RAM_TRANSFER ms=%lu\n",
                   now_ms(),
                   (unsigned long)(now_ms() - transfer_start));
        }
        if (status == EPD_OK) {
            value = refresh(epd, attempt);
        } else {
            value = report(status, attempt);
        }

        printf("[%lu ms] UPDATE %s total=%lu busy=%lu\n",
               now_ms(),
               epd_status_name(value.status),
               (unsigned long)value.elapsed_ms,
               (unsigned long)value.busy_ms);
        if (value.status == EPD_OK) {
            epd->ready = true;
            if (!sleep_panel(epd)) {
                value.status = EPD_ERROR_SPI;
                return value;
            }
            value.elapsed_ms = now_ms() - update_start;
            printf("[%lu ms] FULL_UPDATE ms=%lu refresh=%lu\n",
                   now_ms(),
                   (unsigned long)value.elapsed_ms,
                   (unsigned long)value.busy_ms);
            return value;
        }
        sleep_ms(100);
    }
    return value;
}

EpdStatus epd_open(Epd *epd, const EpdConfig *config) {
    if (epd == NULL || config == NULL || config->spi == NULL || config->baud == 0) {
        return EPD_ERROR_ARGUMENT;
    }

    epd->config = *config;
    epd->open = false;
    epd->ready = false;
    epd->sleeping = false;

    gpio_init(config->cs);
    gpio_init(config->dc);
    gpio_init(config->rst);
    gpio_init(config->busy);
    gpio_set_dir(config->cs, GPIO_OUT);
    gpio_set_dir(config->dc, GPIO_OUT);
    gpio_set_dir(config->rst, GPIO_OUT);
    gpio_set_dir(config->busy, GPIO_IN);
    gpio_disable_pulls(config->busy);
    gpio_put(config->cs, true);
    gpio_put(config->dc, true);
    gpio_put(config->rst, true);

    uint32_t baud = spi_init(config->spi, config->baud);
    spi_set_format(config->spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(config->clk, GPIO_FUNC_SPI);
    gpio_set_function(config->mosi, GPIO_FUNC_SPI);

    if (baud == 0) {
        return EPD_ERROR_SPI;
    }

    epd->open = true;
    printf("[%lu ms] SPI ready requested=%lu actual=%lu\n",
           now_ms(),
           (unsigned long)config->baud,
           (unsigned long)baud);
    return EPD_OK;
}

EpdReport epd_draw(Epd *epd, const uint8_t *black, const uint8_t *red) {
    if (black == NULL || red == NULL) {
        return report(EPD_ERROR_ARGUMENT, 0);
    }

    EpdImage image = {
        .black = black,
        .red = red
    };
    return update(epd, &image);
}

EpdReport epd_draw_partial(Epd *epd,
                           const uint8_t *black,
                           const uint8_t *red,
                           uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height) {
    if (epd == NULL || black == NULL || red == NULL || !epd->open || !epd->ready ||
        !valid_region(x, y, width, height)) {
        return report(EPD_ERROR_ARGUMENT, 0);
    }

    EpdImage image = {.black = black, .red = red};
    uint32_t start = now_ms();
    uint32_t actual_baud = spi_set_baudrate(epd->config.spi, epd->config.baud);
    EpdStatus status = reset(epd);
    if (status == EPD_OK) {
        epd->sleeping = false;
        status = configure(epd);
    }
    uint32_t transfer_start = now_ms();
    if (status == EPD_OK &&
        (!set_area(epd, x, y, width, height) ||
         !write_region(epd, 0x24, &image, x, y, width, height) ||
         !write_region(epd, 0x26, &image, x, y, width, height))) {
        status = EPD_ERROR_SPI;
    }
    uint32_t transfer_ms = now_ms() - transfer_start;

    EpdReport value = report(status, 1);
    if (status == EPD_OK) {
        value = refresh(epd, 1);
        if (value.status == EPD_OK && !sleep_panel(epd)) {
            value.status = EPD_ERROR_SPI;
        }
    }
    value.elapsed_ms = now_ms() - start;

    printf("[%lu ms] PARTIAL x=%u y=%u w=%u h=%u bytes=%lu spi=%lu transfer=%lu total=%lu busy=%lu status=%s\n",
           now_ms(),
           x,
           y,
           width,
           height,
           (unsigned long)(2U * (width / 8U) * height),
           (unsigned long)actual_baud,
           (unsigned long)transfer_ms,
           (unsigned long)value.elapsed_ms,
           (unsigned long)value.busy_ms,
           epd_status_name(value.status));
    return value;
}

const char *epd_status_name(EpdStatus status) {
    switch (status) {
        case EPD_OK:
            return "OK";
        case EPD_ERROR_ARGUMENT:
            return "ERROR_ARGUMENT";
        case EPD_ERROR_SPI:
            return "ERROR_SPI";
        case EPD_ERROR_BUSY_IDLE:
            return "ERROR_BUSY_IDLE";
        case EPD_ERROR_BUSY_START:
            return "ERROR_BUSY_START";
        case EPD_ERROR_BUSY_END:
            return "ERROR_BUSY_END";
    }
    return "ERROR_UNKNOWN";
}
