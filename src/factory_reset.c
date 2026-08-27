#include "factory_reset.h"

#include <stdbool.h>
#include "device_store.h"
#include "hardware/gpio.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "log.h"
#include "pico/low_power.h"
#include "pico/platform.h"
#include "pico/stdlib.h"

enum {
    DEBUG_SAMPLE_MS = 50,
    LOW_POWER_SLICE_MS = 1000,
    RESET_HOLD_MS = 5000,
    BOOT_SAMPLE_MS = 100,
    CS_PIN_INDEX = 1
};

static bool __no_inline_not_in_flash_func(bootsel_pressed)(void) {
    uint32_t flags = save_and_disable_interrupts();
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    for (volatile int delay = 0; delay < 1000; ++delay) {
    }
#if PICO_RP2040
    const uint32_t cs_bit = 1U << 1;
#else
    const uint32_t cs_bit = SIO_GPIO_HI_IN_QSPI_CSN_BITS;
#endif
    bool pressed = (sio_hw->gpio_hi_in & cs_bit) == 0;
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    restore_interrupts(flags);
    return pressed;
}

static bool reset_device(void) {
    APP_LOG("FACTORY_RESET state=erasing\n");
    if (!device_store_reset()) {
        APP_LOG("FACTORY_RESET state=failed\n");
        return false;
    }
    APP_LOG("FACTORY_RESET state=complete\n");
    return true;
}

bool factory_reset_boot(void) {
    if (!bootsel_pressed()) {
        return false;
    }
    for (uint32_t held = 0; held < RESET_HOLD_MS; held += BOOT_SAMPLE_MS) {
        sleep_ms(BOOT_SAMPLE_MS);
        if (!bootsel_pressed()) {
            return false;
        }
    }
    return reset_device();
}

static uint32_t sample_ms(void) {
#if EPAPER_USB_LOGS || EPAPER_USB_MAINTENANCE
    return DEBUG_SAMPLE_MS;
#else
    return LOW_POWER_SLICE_MS;
#endif
}

static void idle(uint32_t duration_ms) {
#if EPAPER_USB_LOGS || EPAPER_USB_MAINTENANCE
    sleep_ms(duration_ms);
#else
    if (low_power_dormant_for_ms(duration_ms, DORMANT_CLOCK_SOURCE_DEFAULT, NULL) != 0) {
        sleep_ms(duration_ms);
    }
#endif
}

bool factory_reset_sleep(uint32_t duration_ms) {
    uint32_t elapsed = 0;
    uint32_t held = 0;
    while (elapsed < duration_ms) {
        uint32_t remaining = duration_ms - elapsed;
        uint32_t sample = sample_ms();
        uint32_t wait = remaining < sample ? remaining : sample;
        idle(wait);
        elapsed += wait;
        if (bootsel_pressed()) {
            held += wait;
            if (held >= RESET_HOLD_MS) {
                if (reset_device()) {
                    return true;
                }
                held = 0;
            }
        } else {
            held = 0;
        }
    }
    return false;
}
