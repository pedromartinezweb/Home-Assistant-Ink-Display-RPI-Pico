#ifndef APP_H
#define APP_H

#include "dashboard.h"
#include "epaper.h"

typedef struct {
    Epaper epaper;
    Dashboard dashboard;
    Reading reading;
    bool has_reading;
} App;

void app_run(App *app, const EpdConfig *config);

#endif
