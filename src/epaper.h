#ifndef EPAPER_H
#define EPAPER_H

#include <stdbool.h>
#include <stdint.h>

#include "epd.h"
#include "frame.h"

typedef enum {
    EPAPER_PRESENT_NONE,
    EPAPER_PRESENT_FULL,
    EPAPER_PRESENT_PARTIAL
} EpaperPresentMode;

typedef struct {
    Epd driver;
    uint8_t black[EPD_BUFFER_SIZE];
    uint8_t red[EPD_BUFFER_SIZE];
    bool has_frame;
} Epaper;

typedef struct {
    EpdReport driver;
    EpaperPresentMode mode;
    FrameRegion region;
    uint32_t bytes;
} EpaperResult;

EpdStatus epaper_open(Epaper *epaper, const EpdConfig *config);
EpaperResult epaper_present(Epaper *epaper, const uint8_t *black, const uint8_t *red);

#endif
