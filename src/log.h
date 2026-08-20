#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#if EPAPER_USB_LOGS
#define APP_LOG(...) printf(__VA_ARGS__)
#else
#define APP_LOG(...) do { if (0) printf(__VA_ARGS__); } while (0)
#endif

#endif
