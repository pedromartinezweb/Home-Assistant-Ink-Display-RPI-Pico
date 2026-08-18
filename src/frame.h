#ifndef FRAME_H
#define FRAME_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} FrameRegion;

void frame_clear(uint8_t *buffer, bool black);
void frame_pixel(uint8_t *buffer, int x, int y, bool black);
void frame_line(uint8_t *buffer, int x0, int y0, int x1, int y1, bool black);
void frame_rect(uint8_t *buffer, int x, int y, int width, int height, bool black);
void frame_fill_rect(uint8_t *buffer, int x, int y, int width, int height, bool black);
void frame_text_landscape(uint8_t *buffer, int x, int y, const char *text, int scale);
void frame_text_landscape_color(uint8_t *buffer, int x, int y, const char *text, int scale, bool black);
void frame_line_landscape(uint8_t *buffer, int x0, int y0, int x1, int y1, bool black);
void frame_rect_landscape(uint8_t *buffer, int x, int y, int width, int height, bool black);
void frame_fill_rect_landscape(uint8_t *buffer, int x, int y, int width, int height, bool black);
bool frame_diff_region(const uint8_t *old_black,
                       const uint8_t *old_red,
                       const uint8_t *new_black,
                       const uint8_t *new_red,
                       FrameRegion *region);

#endif
