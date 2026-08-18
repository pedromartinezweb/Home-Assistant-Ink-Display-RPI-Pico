#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
workspace_dir=$(dirname "$project_dir")
board=${1:-pico_w}
usb_logs=${EPAPER_USB_LOGS:-ON}

case "$board" in
    pico_w|pico2_w) ;;
    *)
        echo "Unsupported board: $board" >&2
        exit 1
        ;;
esac

build_dir="$project_dir/build-$board"

if [ -z "${PICO_SDK_PATH:-}" ]; then
    sdk_dir="$workspace_dir/.tools/pico-sdk"
    if [ -f "$sdk_dir/external/pico_sdk_import.cmake" ]; then
        PICO_SDK_PATH="$sdk_dir"
        export PICO_SDK_PATH
    fi
fi

if [ -z "${PICO_SDK_PATH:-}" ]; then
    echo "Raspberry Pi Pico SDK not found" >&2
    exit 1
fi

if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo "Arm GNU Toolchain not found" >&2
    exit 1
fi

cmake -S "$project_dir" -B "$build_dir" -G Ninja -DPICO_BOARD="$board" -DCMAKE_BUILD_TYPE=Release -DEPAPER_USB_LOGS="$usb_logs"
cmake --build "$build_dir"
