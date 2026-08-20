#include "pico/stdlib.h"

#include "app.h"

static App app;

static const EpdConfig display = {
    .spi = spi0,
    .baud = 4000000,
    .cs = 17,
    .clk = 18,
    .mosi = 19,
    .dc = 20,
    .rst = 21,
    .busy = 22
};

int main(void) {
    stdio_init_all();
    sleep_ms(1000);
    app_run(&app, &display);
    return 0;
}
