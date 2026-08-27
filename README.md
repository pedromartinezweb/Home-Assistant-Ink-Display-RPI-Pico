<p align="center">
  <img src="custom_components/ha_ink_display/brand/icon@2x.png" width="128" alt="Home Assistant Ink Display icon">
</p>

# Home Assistant Ink Display RPI Pico

A configurable, low-power Home Assistant dashboard for Raspberry Pi Pico W, Pico 2 W, and the WeActStudio 2.13-inch black, white, and red e-paper display.

No Arduino knowledge, C editing, Home Assistant token, or fixed entity list is required. The guided installer creates the UF2 with your Wi-Fi credentials. Home Assistant manages the layout, entities, alerts, and update interval afterward.

![Home Assistant e-paper dashboard](docs/rp2040-ink-home-assistant.jpg)

## 1. Install it in Home Assistant

Home Assistant is the main interface for this project. Install the integration before preparing the hardware:

1. Open **HACS → Integrations**.
2. Open the menu in the top-right corner and select **Custom repositories**.
3. Add this repository:

   ```text
   https://github.com/pedromartinezweb/Home-Assistant-Ink-Display-RPI-Pico
   ```

4. Select **Integration** as the category.
5. Search for **Home Assistant Ink Display** and install it.
6. Restart Home Assistant.

The integration includes light and dark mode icons on Home Assistant 2026.3 or newer.

## 2. Manage the display in Home Assistant

After pairing the hardware, open **Ink Display** in the Home Assistant sidebar. This is the only place you need to manage the screen.

You can:

- Add one to four entities to each of two rows, up to eight in total.
- Drag items to reorder them or move them between rows.
- Edit or remove any item without rebuilding the firmware.
- Leave labels and custom units empty.
- Use symbols such as `º` and `°` in labels and units.
- Choose zero, one, or two decimal places.
- Show a value in red when it is above or below a threshold.
- Set the minimum update interval from 60 seconds to 24 hours.

Select **Save changes** after editing. The Pico receives the new configuration during its next check. The default minimum interval is five minutes.

| Setting | Meaning |
|---|---|
| Entity | Home Assistant value shown in the cell |
| Label | Optional short name; empty uses no label |
| Custom unit | Optional unit; empty uses the entity unit |
| Decimal places | Zero, one, or two |
| Red alert | Never, above the threshold, or below the threshold |
| Alert threshold | Numeric value that activates the selected red alert |

The header always shows `ACT HH:MM`. `ACT` is fixed and indicates the time of the last data update.

## 3. Prepare the hardware

### What you need

- Raspberry Pi Pico W or Raspberry Pi Pico 2 W.
- WeActStudio 2.13-inch BWR E-Paper Module.
- Eight female-to-female jumper wires.
- A USB data cable.
- A 2.4 GHz Wi-Fi network.
- A Windows, macOS, Ubuntu, Debian, or Raspberry Pi OS computer.

### Check the display

The solder bridges on the back of the e-paper module must select `4-Lines SPI`:

- `SB1` closed.
- `SB2` open.

Only power the module from 3.3 V.

### Connect the display

Disconnect USB power before changing wires.

| E-paper pin | Pico pin | Physical pin | Purpose |
|---|---|---:|---|
| VCC | 3V3(OUT) | 36 | 3.3 V power |
| GND | GND | 23 | Ground |
| SDA | GP19 | 25 | SPI data |
| SCL | GP18 | 24 | SPI clock |
| CS | GP17 | 22 | Chip select |
| DC | GP20 | 26 | Data or command |
| RES | GP21 | 27 | Display reset |
| BUSY | GP22 | 29 | Display status |

Pico W and Pico 2 W use the same wiring. MISO is not connected.

## 4. Create and install the UF2

Download the project with **Code → Download ZIP** and extract it. Advanced users can clone it:

```sh
git clone https://github.com/pedromartinezweb/Home-Assistant-Ink-Display-RPI-Pico.git
cd Home-Assistant-Ink-Display-RPI-Pico
```

The guided setup checks the required tools, installs missing dependencies, downloads the official Raspberry Pi Pico SDK, asks for the Pico model and Wi-Fi credentials, builds the UF2, and offers to upload it.

### Windows

Double-click `setup.bat`.

### macOS

Double-click `setup.command`. If macOS blocks it, right-click the file, select **Open**, and confirm.

### Ubuntu, Debian, or Raspberry Pi OS

Open a terminal in the extracted project folder and run:

```sh
chmod +x setup.sh
./setup.sh
```

The installer asks only for:

1. Pico W or Pico 2 W.
2. Wi-Fi name.
3. Optional fallback Wi-Fi name (useful when two SSIDs share the same LAN).
4. Wi-Fi password, hidden while typing.

It does not ask for a Home Assistant URL or token. The generated file is stored in `firmware`, and the installer attempts to upload it to the connected Pico.

For the first installation only:

1. Disconnect the Pico from USB.
2. Hold `BOOTSEL` while reconnecting USB.
3. Release it when `RPI-RP2` or `RP2350` appears.
4. Copy the generated UF2 to that drive.
5. Wait for the Pico to restart.

The Wi-Fi configuration and generated UF2 are excluded from Git. Do not share the UF2 because it contains your Wi-Fi credentials.

## 5. Pair the display

After its first complete cleaning cycle, the screen shows a six-digit pairing code.

1. Open **Settings → Devices & services** in Home Assistant.
2. Select the automatically discovered display, or select **Add integration → Home Assistant Ink Display**.
3. Enter the six-digit code shown on the screen.
4. Choose the title, update interval, and number of cells in each row.
5. Select the Home Assistant entity displayed in every cell and finish the wizard.

No IP address is required. If several unpaired displays are available, Home Assistant shows a device list after validating the pairing code.

To change the layout later, open the device in **Settings → Devices & services** and select **Visit**, or open **Ink Display** in the sidebar. The editor previews the two rows and saves all entity changes together.

## How updates work

Home Assistant watches only the configured entities. When one changes, it keeps the newest combined dashboard state. At the configured interval, the Pico wakes briefly and enables Wi-Fi:

- If nothing changed, the display stays asleep.
- If values changed, the Pico downloads one authenticated frame and refreshes once.
- Multiple changes are combined into the same refresh.
- Unavailable or non-numeric values appear as `N/A`.
- If Wi-Fi or Home Assistant is unavailable, the previous e-paper image remains visible.

After pairing or rebooting, the Pico checks every five seconds until it receives its first valid frame. It then uses the interval selected in Home Assistant. This avoids leaving the pairing screen visible while waiting for the first regular update.

The 60-second minimum protects the tri-color panel from continuous updates. Five minutes is recommended for environmental sensors.

## Power and USB updates

The standard build enables the Raspberry Pi USB maintenance interface. With the Pico connected by a USB data cable, `setup.sh` uploads future firmware updates automatically through `picotool`; it resets the Pico into its bootloader in software and installs the UF2 without using `BOOTSEL` or `RUN`.

The first installation cannot be automated, because the existing firmware must already contain this interface. Use `BOOTSEL` once to install this version. Every later update is automatic.

USB maintenance keeps the USB controller awake so the computer can request the update. To use the lowest-power profile for a permanently installed, battery-powered display, build with it disabled:

```sh
EPAPER_USB_MAINTENANCE=OFF ./build.sh pico_w
```

The low-power profile:

- USB serial, USB maintenance, and diagnostic formatting are removed from production firmware.
- Wi-Fi is initialized only for a check and shut down immediately afterward.
- The e-paper controller enters deep sleep after every write.
- The Pico uses the SDK dormant mode between checks.
- A one-second dormant slice provides reliable `BOOTSEL` long-press detection while keeping the CPU asleep between checks.

The Raspberry Pi SDK documents rough board-level measurements around 0.95 mA for an RP2040 Pico in dormant mode and 3.3 mA for an RP2350 Pico 2 in dormant mode. These are reference figures, not measurements of this complete device; the W radio package, regulator, e-paper module, supply voltage, and wiring affect the real total.

For USB diagnostics, build the separate debug profile:

```sh
EPAPER_USB_LOGS=ON ./build.sh pico_w
```

The debug profile adds USB logs. The normal setup script builds the USB-maintenance profile so it can update an already-installed display automatically.

## Pair again or change Wi-Fi

To clear only Home Assistant pairing, hold `BOOTSEL` for at least five seconds while the firmware is running. The Pico erases its device secret and immediately shows a new pairing code without requiring a restart. A short press does nothing. Pairing again reconnects the existing Home Assistant device and preserves its layout.

To change Wi-Fi, run the setup tool again and install the new UF2. Each build gets a new provisioning identifier and clears the previous pairing.

## Common problems

| Problem | What to check |
|---|---|
| The installer cannot install tools | Confirm Internet access and accept the administrator prompt |
| The first upload cannot find a Pico | Use a USB data cable and hold `BOOTSEL` while connecting it. Later uploads are performed automatically. |
| The display never changes | Check every wire, 3.3 V power, and the `SB1`/`SB2` SPI setting |
| Pairing code appears but discovery fails | Confirm both devices use the same LAN and mDNS is not blocked |
| Pairing fails | Enter the current code before restarting the Pico |
| The sidebar editor is missing | Update through HACS and restart Home Assistant |
| Home Assistant reports local HTTP is required | Configure a local HTTP internal URL in Home Assistant |
| A cell shows `N/A` | The entity is unavailable, unknown, or not numeric |
| The screen flashes several colors | This is the normal full waveform of the tri-color panel |
| USB serial logs are missing | The standard build keeps USB firmware updates but disables logs; use the debug profile |

## Security

- Wi-Fi credentials remain in the local untracked configuration and generated UF2.
- The Pico never receives a Home Assistant user token.
- Pairing requires physical access to the code shown on the display.
- Pairing creates a random 256-bit device secret.
- Poll requests and frames use HMAC-SHA256 authentication.
- Normal communication remains on the local network.

## Printable enclosure

The [`enclosure`](enclosure/) folder contains ready-to-slice STL files, editable OpenSCAD source, and assembly instructions.

## Development

Run native framebuffer and protocol tests:

```sh
./test.sh
```

Build manually with the low-power profile:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
./build.sh pico_w
```

| Path | Responsibility |
|---|---|
| `src` | Pico W and Pico 2 W firmware |
| `custom_components/ha_ink_display` | HACS-compatible Home Assistant integration |
| `tests` | Native protocol and rendering tests |
| `enclosure` | Printable enclosure and OpenSCAD source |
| `setup.sh` | macOS and Linux guided installer |
| `setup.ps1` | Windows guided installer |

## Original project

This project is based on [RP2040 INK Display Home Assistant](https://github.com/pedromartinezweb/RP2040-INK-Display-Home-Assistant), whose firmware established the SSD1680 driver, stable full-refresh sequence, low-power lifecycle, and responsive two-row renderer.

## References

- [Home Assistant local brand assets](https://developers.home-assistant.io/docs/core/integration/brand_images/)
- [Raspberry Pi Pico SDK low-power API](https://www.raspberrypi.com/documentation/pico-sdk/high_level.html)
- [WeActStudio E-Paper Module](https://github.com/WeActStudio/WeActStudio.EpaperModule)
- [Raspberry Pi Pico W documentation](https://pip.raspberrypi.com/categories/686-raspberry-pi-pico-w)
