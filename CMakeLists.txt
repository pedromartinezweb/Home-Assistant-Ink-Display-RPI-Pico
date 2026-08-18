cmake_minimum_required(VERSION 3.13)

if (NOT DEFINED PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

if (NOT PICO_SDK_PATH)
    message(FATAL_ERROR "Define PICO_SDK_PATH con la ruta del Raspberry Pi Pico SDK")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(epaper_demo C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
option(EPAPER_USB_LOGS "Activar logs USB" ON)

pico_sdk_init()

add_library(epaper STATIC
    src/epd.c
    src/frame.c
    src/epaper.c
)

target_include_directories(epaper PUBLIC src)
target_link_libraries(epaper PUBLIC pico_stdlib hardware_spi)
target_compile_options(epaper PRIVATE -Wall -Wextra -Werror)

add_library(connectivity STATIC
    src/wifi_session.c
    src/home_assistant.c
)

target_include_directories(connectivity PUBLIC src)
target_link_libraries(connectivity PUBLIC pico_stdlib pico_cyw43_arch_lwip_poll)
target_compile_options(connectivity PRIVATE -Wall -Wextra -Werror)

add_executable(epaper_demo
    src/main.c
    src/app.c
    src/dashboard.c
)

target_include_directories(epaper_demo PRIVATE src)
target_link_libraries(epaper_demo epaper connectivity)
target_compile_options(epaper_demo PRIVATE -Wall -Wextra -Werror)

if (EPAPER_USB_LOGS)
    pico_enable_stdio_usb(epaper_demo 1)
else()
    pico_enable_stdio_usb(epaper_demo 0)
endif()
pico_enable_stdio_uart(epaper_demo 0)
pico_add_extra_outputs(epaper_demo)
