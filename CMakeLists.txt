cmake_minimum_required(VERSION 3.13)

if (NOT DEFINED PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

if (NOT PICO_SDK_PATH)
    message(FATAL_ERROR "Set PICO_SDK_PATH to the Raspberry Pi Pico SDK directory")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(ha_ink_display C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
option(EPAPER_USB_LOGS "Enable USB logs" ON)

pico_sdk_init()

add_library(epaper STATIC
    src/epd.c
    src/frame.c
    src/epaper.c
)

target_include_directories(epaper PUBLIC src)
target_link_libraries(epaper PUBLIC pico_stdlib hardware_spi)
target_compile_options(epaper PRIVATE -Wall -Wextra -Werror)

add_executable(ha_ink_display
    src/main.c
    src/app.c
    src/dashboard.c
    src/ink_protocol.c
    src/wifi_session.c
    src/ink_client.c
    src/pair_server.c
    src/device_store.c
    src/factory_reset.c
)

target_include_directories(ha_ink_display PRIVATE src)
target_link_libraries(ha_ink_display
    epaper
    pico_cyw43_arch_lwip_poll
    pico_lwip_mdns
    pico_mbedtls_crypto
    pico_mbedtls_headers
    pico_flash
    pico_rand
    pico_unique_id
    hardware_flash
    hardware_watchdog
)
target_compile_options(ha_ink_display PRIVATE -Wall -Wextra -Werror)

if (EPAPER_USB_LOGS)
    pico_enable_stdio_usb(ha_ink_display 1)
else()
    pico_enable_stdio_usb(ha_ink_display 0)
endif()
pico_enable_stdio_uart(ha_ink_display 0)
pico_add_extra_outputs(ha_ink_display)
