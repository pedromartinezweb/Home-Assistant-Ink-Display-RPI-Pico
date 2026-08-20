#include "dashboard.h"

#include <stdio.h>
#include <string.h>

#include "frame.h"

enum {
    CONTENT_LEFT = 8,
    CONTENT_RIGHT = 242,
    CONTENT_WIDTH = CONTENT_RIGHT - CONTENT_LEFT,
    UNIT_GAP = 4
};

static bool text_valid(const char *text, size_t max, bool empty) {
    return text != NULL && (empty || text[0] != '\0') && strlen(text) <= max;
}

static size_t row_count(const DashboardConfig *config, uint8_t row) {
    size_t count = 0;
    for (size_t index = 0; index < config->count; ++index) {
        if (config->items[index].row == row) {
            count++;
        }
    }
    return count;
}

bool dashboard_config_valid(const DashboardConfig *config) {
    if (config == NULL || config->items == NULL || config->count < 2 ||
        config->count > DASHBOARD_MAX_ITEMS ||
        !text_valid(config->title, 24, false) ||
        !text_valid(config->updated, 8, false)) {
        return false;
    }

    for (size_t index = 0; index < config->count; ++index) {
        const DashboardItem *item = &config->items[index];
        if (!text_valid(item->label, 12, false) ||
            !text_valid(item->unit, 5, true) ||
            (item->row != 1 && item->row != 2) ||
            item->decimals > 2) {
            return false;
        }
    }

    size_t first = row_count(config, 1);
    size_t second = row_count(config, 2);
    if (first < 1 || first > 4 || second < 1 || second > 4 ||
        (strlen(config->title) + strlen(config->updated) + 6) * 6 > 228) {
        return false;
    }

    for (uint8_t row = 1; row <= 2; ++row) {
        size_t count = row == 1 ? first : second;
        size_t slot = 0;
        for (size_t index = 0; index < config->count; ++index) {
            if (config->items[index].row != row) {
                continue;
            }
            int left = CONTENT_LEFT + (int)(slot * CONTENT_WIDTH / count);
            int right = CONTENT_LEFT + (int)((slot + 1) * CONTENT_WIDTH / count);
            int width = right - left - (slot > 0 ? 10 : 4);
            if ((int)strlen(config->items[index].label) * 6 > width) {
                return false;
            }
            slot++;
        }
    }
    return true;
}

static bool data_valid(const DashboardData *data, size_t count) {
    return data != NULL && data->count == count &&
           data->hour >= 0 && data->hour <= 23 &&
           data->minute >= 0 && data->minute <= 59;
}

static int text_width(const char *text, int scale) {
    return (int)strlen(text) * 6 * scale;
}

static int rounded(int value, int divisor) {
    if (value >= 0) {
        return (value + divisor / 2) / divisor;
    }
    return (value - divisor / 2) / divisor;
}

static void format_value(char *text, size_t size, int milli, uint8_t decimals) {
    static const int divisors[] = {1000, 100, 10};
    int value = rounded(milli, divisors[decimals]);
    if (decimals == 0) {
        snprintf(text, size, "%d", value);
        return;
    }

    int base = decimals == 1 ? 10 : 100;
    int magnitude = value < 0 ? -value : value;
    snprintf(text,
             size,
             decimals == 1 ? "%s%d.%01d" : "%s%d.%02d",
             value < 0 ? "-" : "",
             magnitude / base,
             magnitude % base);
}

static int value_scale(const char *value, const char *unit, int width, int max_scale) {
    for (int scale = max_scale; scale > 1; --scale) {
        if (text_width(value, scale) + UNIT_GAP + text_width(unit, 1) <= width) {
            return scale;
        }
    }
    return 1;
}

static bool use_red(const DashboardItem *item, int milli) {
    return item->red_above != DASHBOARD_NO_RED &&
           (int64_t)milli > (int64_t)item->red_above * 1000;
}

static void draw_header(Dashboard *dashboard, const DashboardData *data, const DashboardConfig *config) {
    char time[32];
    snprintf(time, sizeof(time), "%s %02d:%02d", config->updated, data->hour, data->minute);
    int time_x = CONTENT_RIGHT - text_width(time, 1);
    frame_fill_rect_landscape(dashboard->black, 1, 1, 248, 30, true);
    frame_text_landscape_color(dashboard->black, CONTENT_LEFT, 10, config->title, 1, false);
    frame_text_landscape_color(dashboard->black, time_x, 10, time, 1, false);
}

static void draw_item(Dashboard *dashboard,
                      const DashboardItem *item,
                      int milli,
                      bool valid,
                      int x,
                      int width,
                      int top,
                      int max_scale) {
    char value[24];
    if (valid) {
        format_value(value, sizeof(value), milli, item->decimals);
    } else {
        memcpy(value, "N/A", 4);
    }
    const char *unit = item->unit;
    if (text_width(value, 1) + UNIT_GAP + text_width(unit, 1) > width) {
        unit = "";
    }
    if (text_width(value, 1) > width) {
        snprintf(value, sizeof(value), "OVR");
    }
    int scale = value_scale(value, unit, width, max_scale);
    int value_y = top + 14;
    int unit_y = value_y + scale * 7 - 7;
    uint8_t *plane = valid && use_red(item, milli) ? dashboard->red : dashboard->black;

    frame_text_landscape(plane, x, top + 2, item->label, 1);
    frame_text_landscape(plane, x, value_y, value, scale);
    int unit_x = x + text_width(value, scale) + UNIT_GAP;
    frame_text_landscape(plane, unit_x, unit_y, unit, 1);
}

static void draw_row(Dashboard *dashboard,
                     const DashboardData *data,
                     const DashboardConfig *config,
                     uint8_t row,
                     int top,
                     int max_scale) {
    size_t count = row_count(config, row);
    size_t slot = 0;
    for (size_t index = 0; index < config->count; ++index) {
        if (config->items[index].row != row) {
            continue;
        }

        int left = CONTENT_LEFT + (int)(slot * CONTENT_WIDTH / count);
        int right = CONTENT_LEFT + (int)((slot + 1) * CONTENT_WIDTH / count);
        if (slot > 0) {
            frame_line_landscape(dashboard->black, left, top + 2, left, top + 36, true);
        }
        int x = left + (slot > 0 ? 6 : 0);
        draw_item(dashboard,
                  &config->items[index],
                  data->values_milli[index],
                  data->valid[index],
                  x,
                  right - x - 4,
                  top,
                  max_scale);
        slot++;
    }
}

bool dashboard_draw(Dashboard *dashboard, const DashboardData *data, const DashboardConfig *config) {
    if (dashboard == NULL || !dashboard_config_valid(config) || !data_valid(data, config->count)) {
        return false;
    }

    frame_clear(dashboard->black, false);
    frame_clear(dashboard->red, false);
    draw_header(dashboard, data, config);
    frame_line_landscape(dashboard->black, CONTENT_LEFT, 79, CONTENT_RIGHT, 79, true);
    draw_row(dashboard, data, config, 1, 35, 4);
    draw_row(dashboard, data, config, 2, 82, 3);
    return true;
}
