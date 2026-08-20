#ifndef APP_H
#define APP_H

#include "dashboard.h"
#include "epaper.h"

typedef struct {
    Epaper epaper;
    Dashboard dashboard;
} App;

void app_run(App *app, const EpdConfig *display);

#endif
