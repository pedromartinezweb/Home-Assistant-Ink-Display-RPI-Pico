#ifndef EPD_H
#define EPD_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/spi.h"

enum {
    EPD_WIDTH = 128,
    EPD_VISIBLE_WIDTH = 122,
    EPD_HEIGHT = 250,
    EPD_ROW_BYTES = EPD_WIDTH / 8,
    EPD_BUFFER_SIZE = EPD_ROW_BYTES * EPD_HEIGHT
};

typedef struct {
    spi_inst_t *spi;
    uint32_t baud;
    uint cs;
    uint clk;
    uint mosi;
    uint dc;
    uint rst;
    uint busy;
} EpdConfig;

typedef struct {
    EpdConfig config;
    bool open;
    bool ready;
    bool sleeping;
} Epd;

typedef enum {
    EPD_OK,
    EPD_ERROR_ARGUMENT,
    EPD_ERROR_SPI,
    EPD_ERROR_BUSY_IDLE,
    EPD_ERROR_BUSY_START,
    EPD_ERROR_BUSY_END
} EpdStatus;

typedef struct {
    EpdStatus status;
    uint32_t elapsed_ms;
    uint32_t busy_ms;
    uint8_t attempts;
} EpdReport;

EpdStatus epd_open(Epd *epd, const EpdConfig *config);
EpdReport epd_draw(Epd *epd, const uint8_t *black, const uint8_t *red);
EpdReport epd_draw_partial(Epd *epd,
                           const uint8_t *black,
                           const uint8_t *red,
                           uint16_t x,
                           uint16_t y,
                           uint16_t width,
                           uint16_t height);
const char *epd_status_name(EpdStatus status);

#endif
