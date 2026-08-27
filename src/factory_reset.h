#ifndef FACTORY_RESET_H
#define FACTORY_RESET_H

#include <stdbool.h>
#include <stdint.h>

bool factory_reset_sleep(uint32_t duration_ms);
bool factory_reset_boot(void);

#endif
