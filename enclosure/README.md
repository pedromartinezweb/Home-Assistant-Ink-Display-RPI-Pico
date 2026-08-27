# Printable enclosure

This folder contains a two-piece enclosure for the WeActStudio 2.13-inch e-paper module and a Raspberry Pi Pico W.

## Files

- `rp2040-ink-case-body.stl`: front enclosure only.
- `rp2040-ink-case-lid.stl`: rear cover only.
- `rp2040-ink-case-print-plate.stl`: compact ready-to-slice plate containing both parts.
- `rp2040-ink-case.scad`: editable parametric OpenSCAD source.

Import the STL you need and keep the scale at 100%. The individual body and lid files are easier to inspect or place manually. The print plate keeps only 3 mm between both parts.

## Print settings

| Setting | Recommended value |
|---|---|
| Material | PLA or PETG |
| Layer height | 0.20 mm |
| Walls | 3 |
| Top and bottom layers | 4 |
| Infill | 15-20% |
| Supports | Not required |
| Orientation | Use the imported orientation |

## Assembly

1. Place the e-paper module behind the front window.
2. Align its four mounting holes with the internal bosses.
3. Secure it with four short M2 self-tapping screws. Start with M2 x 4 mm and do not overtighten.
4. Fix the Pico W to the inside of the rear cover with two narrow zip ties through the paired slots.
5. Point the Pico USB connector toward the side opening.
6. Connect the wires and press the rear cover into the enclosure.

## Dimensions and fit

The enclosure uses the official WeActStudio module dimensions of 72 x 30 mm, four 3.2 mm mounting holes, a 66.4 x 24.4 mm center-to-center mounting pattern, and the official Pico W board size of 51 x 21 mm.

The rear cover has 0.25 mm clearance on each side. If your printer produces tight parts, increase `lid_clearance` in the OpenSCAD source to `0.35` or `0.40`, regenerate the STL, and print again.

Hardware revisions, pin headers, solder joints, and wire routing can change the required internal space. Check the fit before tightening screws or leaving the device unattended.

## Sources

- [WeActStudio E-Paper Module hardware files](https://github.com/WeActStudio/WeActStudio.EpaperModule/tree/master/Hardware)
- [Raspberry Pi Pico W mechanical documentation](https://datasheets.raspberrypi.com/picow/pico-w-datasheet.pdf)
