#include "frame.h"

#include <stdlib.h>
#include <string.h>

#include "epd.h"

static const uint8_t font[27][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43}
};

static const uint8_t digits[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E}
};

static const uint8_t blank[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
static const uint8_t percent[5] = {0x63, 0x13, 0x08, 0x64, 0x63};
static const uint8_t slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};

static const uint8_t *glyph(char value) {
    if (value >= 'A' && value <= 'Z') {
        return font[value - 'A' + 1];
    }
    if (value >= '0' && value <= '9') {
        return digits[value - '0'];
    }
    if (value == '.') {
        return dot;
    }
    if (value == ':') {
        return colon;
    }
    if (value == '%') {
        return percent;
    }
    if (value == '/') {
        return slash;
    }
    return blank;
}

static void landscape_pixel(uint8_t *buffer, int x, int y, bool black) {
    frame_pixel(buffer, y, EPD_HEIGHT - 1 - x, black);
}

void frame_clear(uint8_t *buffer, bool black) {
    if (buffer == NULL) {
        return;
    }
    memset(buffer, black ? 0x00 : 0xFF, EPD_BUFFER_SIZE);
}

void frame_pixel(uint8_t *buffer, int x, int y, bool black) {
    if (buffer == NULL || x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) {
        return;
    }

    uint8_t *value = &buffer[y * EPD_ROW_BYTES + x / 8];
    uint8_t mask = 0x80U >> (x % 8);

    if (black) {
        *value &= (uint8_t)~mask;
    } else {
        *value |= mask;
    }
}

void frame_line(uint8_t *buffer, int x0, int y0, int x1, int y1, bool black) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        frame_pixel(buffer, x0, y0, black);
        if (x0 == x1 && y0 == y1) {
            break;
        }

        int next = 2 * error;
        if (next >= dy) {
            error += dy;
            x0 += sx;
        }
        if (next <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void frame_rect(uint8_t *buffer, int x, int y, int width, int height, bool black) {
    if (buffer == NULL || width <= 0 || height <= 0) {
        return;
    }
    frame_line(buffer, x, y, x + width - 1, y, black);
    frame_line(buffer, x, y, x, y + height - 1, black);
    frame_line(buffer, x + width - 1, y, x + width - 1, y + height - 1, black);
    frame_line(buffer, x, y + height - 1, x + width - 1, y + height - 1, black);
}

void frame_fill_rect(uint8_t *buffer, int x, int y, int width, int height, bool black) {
    if (buffer == NULL || width <= 0 || height <= 0) {
        return;
    }
    for (int row = y; row < y + height; ++row) {
        frame_line(buffer, x, row, x + width - 1, row, black);
    }
}

void frame_text_landscape_color(uint8_t *buffer,
                                int x,
                                int y,
                                const char *text,
                                int scale,
                                bool black) {
    if (buffer == NULL || text == NULL || scale <= 0) {
        return;
    }
    for (int index = 0; text[index] != '\0'; ++index) {
        const uint8_t *shape = glyph(text[index]);

        for (int column = 0; column < 5; ++column) {
            for (int row = 0; row < 7; ++row) {
                if ((shape[column] & (1U << row)) == 0) {
                    continue;
                }

                for (int dx = 0; dx < scale; ++dx) {
                    for (int dy = 0; dy < scale; ++dy) {
                        landscape_pixel(buffer, x + column * scale + dx, y + row * scale + dy, black);
                    }
                }
            }
        }

        x += 6 * scale;
    }
}

void frame_text_landscape(uint8_t *buffer, int x, int y, const char *text, int scale) {
    frame_text_landscape_color(buffer, x, y, text, scale, true);
}

void frame_line_landscape(uint8_t *buffer, int x0, int y0, int x1, int y1, bool black) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        landscape_pixel(buffer, x0, y0, black);
        if (x0 == x1 && y0 == y1) {
            break;
        }

        int next = 2 * error;
        if (next >= dy) {
            error += dy;
            x0 += sx;
        }
        if (next <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void frame_rect_landscape(uint8_t *buffer, int x, int y, int width, int height, bool black) {
    if (buffer == NULL || width <= 0 || height <= 0) {
        return;
    }
    frame_line_landscape(buffer, x, y, x + width - 1, y, black);
    frame_line_landscape(buffer, x, y, x, y + height - 1, black);
    frame_line_landscape(buffer, x + width - 1, y, x + width - 1, y + height - 1, black);
    frame_line_landscape(buffer, x, y + height - 1, x + width - 1, y + height - 1, black);
}

void frame_fill_rect_landscape(uint8_t *buffer, int x, int y, int width, int height, bool black) {
    if (buffer == NULL || width <= 0 || height <= 0) {
        return;
    }
    for (int row = y; row < y + height; ++row) {
        frame_line_landscape(buffer, x, row, x + width - 1, row, black);
    }
}

bool frame_diff_region(const uint8_t *old_black,
                       const uint8_t *old_red,
                       const uint8_t *new_black,
                       const uint8_t *new_red,
                       FrameRegion *region) {
    if (old_black == NULL || old_red == NULL || new_black == NULL || new_red == NULL || region == NULL) {
        return false;
    }

    int min_byte = EPD_ROW_BYTES;
    int max_byte = -1;
    int min_y = EPD_HEIGHT;
    int max_y = -1;

    for (int y = 0; y < EPD_HEIGHT; ++y) {
        for (int x = 0; x < EPD_ROW_BYTES; ++x) {
            int index = y * EPD_ROW_BYTES + x;
            if (old_black[index] == new_black[index] && old_red[index] == new_red[index]) {
                continue;
            }
            if (x < min_byte) min_byte = x;
            if (x > max_byte) max_byte = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }

    if (max_byte < 0) {
        return false;
    }

    region->x = (uint16_t)(min_byte * 8);
    region->y = (uint16_t)min_y;
    region->width = (uint16_t)((max_byte - min_byte + 1) * 8);
    region->height = (uint16_t)(max_y - min_y + 1);
    return true;
}
