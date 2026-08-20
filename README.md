# RP2040 INK Display Home Assistant

A small Home Assistant dashboard for a Raspberry Pi Pico W and a 2.13-inch WeActStudio black, white, and red e-paper display.

It shows up to eight Home Assistant sensor values in two configurable rows. The display controller is active only while an image is being written, Wi-Fi is disabled between updates, and the last image remains visible without power.

![RP2040 Home Assistant e-paper dashboard](docs/rp2040-ink-home-assistant.jpg)

## What you need

- Raspberry Pi Pico W or Pico 2 W.
- WeActStudio 2.13-inch BWR E-Paper Module.
- Eight female-to-female jumper wires.
- A USB cable that supports data.
- A computer connected to the same network as Home Assistant.
- Optional: the [printable enclosure](enclosure/).

No Arduino installation is used. The firmware is written in C with the official Raspberry Pi Pico SDK.

## 1. Prepare the display

Check the small solder bridges on the back of the e-paper board. It must use `4-Lines SPI`:

- `SB1` closed.
- `SB2` open.

Do not power the display from 5 V.

## 2. Connect the wires

Disconnect the USB cable before changing any wire.

| E-paper pin | Pico W pin | Physical pin | Purpose |
|---|---|---:|---|
| VCC | 3V3(OUT) | 36 | 3.3 V power |
| GND | GND | 23 | Ground |
| SDA | GP19 | 25 | SPI data |
| SCL | GP18 | 24 | SPI clock |
| CS | GP17 | 22 | Chip select |
| DC | GP20 | 26 | Data or command |
| RES | GP21 | 27 | Display reset |
| BUSY | GP22 | 29 | Display status |

The Pico W and Pico 2 W use the same connections. MISO is not connected.

## 3. Install the build tools

### macOS

Install [Homebrew](https://brew.sh) first, then run:

```sh
brew install cmake ninja arm-none-eabi-gcc git
git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git "$HOME/pico-sdk"
export PICO_SDK_PATH="$HOME/pico-sdk"
```

Add the last `export` command to `~/.zshrc` if you want it to remain available after restarting the terminal.

### Ubuntu, Debian, or Raspberry Pi OS

```sh
sudo apt update
sudo apt install -y build-essential cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi git
git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git "$HOME/pico-sdk"
export PICO_SDK_PATH="$HOME/pico-sdk"
```

### Windows

The simplest route is Ubuntu through WSL, using the Ubuntu commands above. Alternatively, install the official [Raspberry Pi Pico extension for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico) and let it install the Pico SDK and toolchain.

## 4. Download this project

Open a terminal and run:

```sh
git clone https://github.com/pedromartinezweb/RP2040-INK-Display-Home-Assistant.git
cd RP2040-INK-Display-Home-Assistant
cp src/config_local.example.h src/config_local.h
```

## 5. Get the Home Assistant details

You need three things:

1. The Home Assistant address. A local IP such as `192.168.1.20` is usually more reliable than `homeassistant.local`.
2. A token. In Home Assistant, open your user profile, go to **Security**, find **Long-Lived Access Tokens**, select **Create Token**, and copy it.
3. The entity IDs to display. Open **Settings → Devices & services → Entities**, select a sensor, and copy its entity ID, for example `sensor.living_room_temperature`.

Each selected entity must have a numeric state. Text entities such as `good`, `open`, or `home` cannot be displayed as values.

## 6. Enter your settings

Open `src/config_local.h` in a text editor. Start by replacing only these values:

```c
#define APP_WIFI_SSID "MY_WIFI"
#define APP_WIFI_PASSWORD "MY_WIFI_PASSWORD"
#define APP_HA_HOST "192.168.1.20"
#define APP_HA_PORT 8123
#define APP_HA_TOKEN "MY_LONG_LIVED_ACCESS_TOKEN"
#define APP_REFRESH_SECONDS 300
```

Do not add `http://` or `/api` to `APP_HA_HOST`.

Now replace the example entity IDs in `APP_VIEW_ITEMS`:

```c
#define APP_VIEW_ITEMS \
    APP_ITEM("sensor.living_room_co2", "CO2", "PPM", 1, 0, 1000) \
    APP_ITEM("sensor.outdoor_temperature", "OUTSIDE", "C", 1, 1, DASHBOARD_NO_RED) \
    APP_ITEM("sensor.living_room_temperature", "TEMP", "C", 2, 1, DASHBOARD_NO_RED) \
    APP_ITEM("sensor.living_room_humidity", "HUM", "%", 2, 0, DASHBOARD_NO_RED)
```

This example places two values on the first row and two on the second. The final `\` is required on every line except the last one.

## 7. Build the firmware

For a Pico W:

```sh
./build.sh pico_w
```

For a Pico 2 W:

```sh
./build.sh pico2_w
```

For Pico W, the file to install will be:

```text
build-pico_w/epaper_demo.uf2
```

## 8. Install it on the Pico

1. Disconnect the Pico from USB.
2. Hold down the `BOOTSEL` button.
3. Connect the USB cable while continuing to hold the button.
4. Release the button when a drive named `RPI-RP2` appears.
5. Copy `epaper_demo.uf2` to that drive.
6. The Pico restarts automatically.

The first display update can take about 20 seconds. The e-paper may flash black, white, and red during a complete refresh; this is normal.

## Change the dashboard

Each `APP_ITEM` creates one value cell:

```c
APP_ITEM("entity_id", "LABEL", "UNIT", row, decimals, red_threshold)
```

| Setting | Example | Meaning |
|---|---|---|
| Entity ID | `"sensor.co2"` | Entity copied from Home Assistant |
| Label | `"CO2"` | Name shown above the value |
| Unit | `"PPM"` | Text shown after or below the value; use `""` for none |
| Row | `1` | First or second dashboard row |
| Decimals | `0` | Number of decimal places: `0`, `1`, or `2` |
| Red threshold | `1000` | Value becomes red when it is greater than this number |

Use `DASHBOARD_NO_RED` instead of a number to keep the value black.

You can put one to four items in each row, with no more than eight items in total. Items are placed from left to right in the same order as the configuration file. Their widths and font sizes are calculated automatically.

You can also change the heading and the short update label:

```c
#define APP_UI_TITLE "HOUSE"
#define APP_UI_UPDATED "ACT"
```

The font supports uppercase `A-Z`, digits, spaces, `-`, `.`, `:`, `%`, and `/`. Short labels are easier to read, especially with three or four items in a row.

## Common problems

| Problem | What to check |
|---|---|
| The screen never changes | Check VCC, GND, RES, BUSY, and the `SB1`/`SB2` SPI setting |
| The old image remains visible | Wait at least 20 seconds; e-paper keeps its image without power |
| Wi-Fi does not connect | Check the name and password and use a 2.4 GHz Wi-Fi network |
| Home Assistant values are missing | Check the IP, token, entity IDs, and that every state is numeric |
| `Raspberry Pi Pico SDK not found` | Run `export PICO_SDK_PATH="$HOME/pico-sdk"` in the same terminal |
| `Arm GNU Toolchain not found` | Reinstall `arm-none-eabi-gcc` and open a new terminal |
| Build succeeds but the Pico does not start | Confirm that you built for `pico_w`, not `pico2_w`, or vice versa |

USB diagnostic logs are enabled by default. A serial terminal at `115200` baud shows connection, Home Assistant, and display activity.

## Refresh behavior and power use

The default refresh interval is five minutes. Wi-Fi is enabled only while reading Home Assistant, then switched off. The e-paper controller enters deep sleep after an update.

The firmware transfers only the changed display region when possible. This saves processor time and SPI traffic, but this tri-color panel still performs a full-screen physical waveform. On tested hardware, the display remains busy for approximately 18.2 seconds.

If no value changed, no image is written. If Wi-Fi or Home Assistant fails, the last valid image remains visible.

## Printable enclosure

The [`enclosure`](enclosure/) folder contains:

- A ready-to-slice STL with the front enclosure and rear cover.
- An editable OpenSCAD source.
- Printing and assembly instructions.

The model uses the official WeActStudio display mounting dimensions and the official Pico W board size. It is a first printable design, so make a fit check before tightening the screws.

## Run the software tests

The drawing and dashboard tests run on the computer without a Pico:

```sh
./test.sh
```

## Project structure

| File | Responsibility |
|---|---|
| `src/epd.c` | SSD1680 commands, SPI, reset, and BUSY recovery |
| `src/epaper.c` | Display state and update selection |
| `src/frame.c` | Framebuffer, font, and drawing primitives |
| `src/dashboard.c` | Configurable two-row layout and value formatting |
| `src/home_assistant.c` | Home Assistant entity reader and timeouts |
| `src/wifi_session.c` | Wi-Fi connection and shutdown |
| `src/app.c` | Update schedule and retry policy |
| `src/main.c` | Pin configuration and startup |

## Security

- Never publish `src/config_local.h`.
- Never share a UF2 built with real credentials; the credentials are embedded in it.
- Do not expose Home Assistant port `8123` to the Internet.
- Use a dedicated token and revoke it if the Pico is lost or no longer used.

## Hardware references

- [WeActStudio E-Paper Module repository](https://github.com/WeActStudio/WeActStudio.EpaperModule)
- [Raspberry Pi Pico W documentation](https://pip.raspberrypi.com/categories/686-raspberry-pi-pico-w)
- [Raspberry Pi Pico W datasheet](https://datasheets.raspberrypi.com/picow/pico-w-datasheet.pdf)
