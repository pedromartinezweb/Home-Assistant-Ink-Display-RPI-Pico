#!/bin/sh
set -eu

project_dir=$(unset CDPATH; cd -- "$(dirname -- "$0")" && pwd)
tools_dir="$project_dir/.tools"
sdk_dir="$tools_dir/pico-sdk-2.3.0"

compiler_ready() {
    command -v arm-none-eabi-gcc >/dev/null 2>&1 || return 1
    spec_file=$(arm-none-eabi-gcc -print-file-name=nosys.specs)
    [ "$spec_file" != "nosys.specs" ] && [ -f "$spec_file" ]
}

has_tools() {
    command -v git >/dev/null 2>&1 &&
    command -v cmake >/dev/null 2>&1 &&
    command -v ninja >/dev/null 2>&1 &&
    compiler_ready
}

select_macos_toolchain() {
    compiler=$(find /Applications/ArmGNUToolchain -path '*/arm-none-eabi/bin/arm-none-eabi-gcc' -type f 2>/dev/null | sort -V | tail -n 1)
    [ -n "$compiler" ] || return 1
    toolchain_bin=$(dirname "$compiler")
    PICO_TOOLCHAIN_PATH="$toolchain_bin"
    PATH="$toolchain_bin:$PATH"
    export PICO_TOOLCHAIN_PATH PATH
}

install_macos() {
    if ! command -v brew >/dev/null 2>&1; then
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        if [ -x /opt/homebrew/bin/brew ]; then
            eval "$(/opt/homebrew/bin/brew shellenv)"
        elif [ -x /usr/local/bin/brew ]; then
            eval "$(/usr/local/bin/brew shellenv)"
        fi
    fi
    brew install git cmake ninja picotool
    brew install --cask gcc-arm-embedded
    select_macos_toolchain
}

copy_to_volume() {
    firmware=$1
    for volume in \
        /Volumes/RPI-RP2 \
        /Volumes/RP2350 \
        "/media/${USER:-}/RPI-RP2" \
        "/media/${USER:-}/RP2350" \
        "/run/media/${USER:-}/RPI-RP2" \
        "/run/media/${USER:-}/RP2350"
    do
        if [ -d "$volume" ]; then
            cp "$firmware" "$volume/"
            sync
            printf '%s\n' "Firmware uploaded through $volume."
            return 0
        fi
    done
    return 1
}

upload_firmware() {
    firmware=$1
    if command -v picotool >/dev/null 2>&1 && picotool load -f -v -x "$firmware"; then
        printf '%s\n' "Firmware uploaded with picotool."
        return 0
    fi
    if copy_to_volume "$firmware"; then
        return 0
    fi
    printf '%s\n' "Automatic upload could not find a Pico."
    printf '%s\n' "Hold BOOTSEL while connecting it, then copy the UF2 to the RPI-RP2 or RP2350 drive."
    return 1
}

if [ "$(uname -s)" = "Darwin" ]; then
    select_macos_toolchain || true
fi

install_linux() {
    if ! command -v apt-get >/dev/null 2>&1; then
        printf '%s\n' "Automatic installation currently supports Ubuntu, Debian, and Raspberry Pi OS." >&2
        exit 1
    fi
    admin=""
    if [ "$(id -u)" -ne 0 ]; then
        admin="sudo"
    fi
    $admin apt-get update
    $admin apt-get install -y git cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
    if apt-cache show picotool >/dev/null 2>&1; then
        $admin apt-get install -y picotool
    fi
}

needs_install=false
if ! has_tools; then
    needs_install=true
elif [ "$(uname -s)" = "Darwin" ] && ! command -v picotool >/dev/null 2>&1; then
    needs_install=true
fi

if [ "$needs_install" = true ]; then
    printf '%s\n' "Required build tools are missing. The installer will add them now."
    case "$(uname -s)" in
        Darwin) install_macos ;;
        Linux) install_linux ;;
        *)
            printf '%s\n' "Use setup.bat on Windows." >&2
            exit 1
            ;;
    esac
fi

if ! has_tools; then
    printf '%s\n' "The build tools could not be installed." >&2
    exit 1
fi

mkdir -p "$tools_dir"
if [ ! -f "$sdk_dir/external/pico_sdk_import.cmake" ]; then
    git clone --branch 2.3.0 --depth 1 --recurse-submodules --shallow-submodules https://github.com/raspberrypi/pico-sdk.git "$sdk_dir"
fi

printf '%s\n' "Select your Raspberry Pi Pico:"
printf '%s\n' "1) Pico W"
printf '%s\n' "2) Pico 2 W"
printf '%s' "Choice [1]: "
read -r board_choice
case "${board_choice:-1}" in
    1) board="pico_w" ;;
    2) board="pico2_w" ;;
    *)
        printf '%s\n' "Invalid board selection." >&2
        exit 1
        ;;
esac

printf '%s' "Wi-Fi name (SSID): "
read -r wifi_ssid
if [ -z "$wifi_ssid" ] || [ "${#wifi_ssid}" -gt 32 ]; then
    printf '%s\n' "The Wi-Fi name must contain between 1 and 32 characters." >&2
    exit 1
fi

printf '%s' "Wi-Fi password: "
stty -echo
trap 'stty echo' EXIT INT TERM
read -r wifi_password
stty echo
trap - EXIT INT TERM
printf '\n'
if [ "${#wifi_password}" -lt 8 ] || [ "${#wifi_password}" -gt 63 ]; then
    printf '%s\n' "The Wi-Fi password must contain between 8 and 63 characters." >&2
    exit 1
fi

escaped_ssid=$(printf '%s' "$wifi_ssid" | sed 's/\\/\\\\/g; s/"/\\"/g')
escaped_password=$(printf '%s' "$wifi_password" | sed 's/\\/\\\\/g; s/"/\\"/g')
provisioning_id=$(od -An -N4 -tu4 /dev/urandom | tr -d ' ')
config_file="$project_dir/src/config_local.h"
previous_umask=$(umask)
umask 077
{
    printf '%s\n' '#ifndef CONFIG_LOCAL_H'
    printf '%s\n' '#define CONFIG_LOCAL_H'
    printf '\n'
    printf '#define APP_WIFI_SSID "%s"\n' "$escaped_ssid"
    printf '#define APP_WIFI_PASSWORD "%s"\n' "$escaped_password"
    printf '#define APP_PROVISIONING_ID %sU\n' "$provisioning_id"
    printf '\n'
    printf '%s\n' '#endif'
} > "$config_file"
umask "$previous_umask"

export PICO_SDK_PATH="$sdk_dir"
cmake -E remove_directory "$project_dir/build-$board"
"$project_dir/build.sh" "$board"
mkdir -p "$project_dir/firmware"
cp "$project_dir/build-$board/ha_ink_display.uf2" "$project_dir/firmware/ha_ink_display-$board.uf2"
output="$project_dir/firmware/ha_ink_display-$board.uf2"
printf '\n%s\n' "Firmware ready:"
printf '%s\n' "$output"
printf '%s' "Upload it to the connected Pico now? [Y/n]: "
read -r upload_choice
case "${upload_choice:-y}" in
    y|Y|yes|YES|Yes) upload_firmware "$output" || true ;;
    *) printf '%s\n' "Upload skipped." ;;
esac
