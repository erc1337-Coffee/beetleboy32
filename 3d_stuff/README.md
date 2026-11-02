# Beetleboy32 — Design Specs

Everything here was made by Opus 5.5 high

## Bill of materials

| Part | Spec |
| --- | --- |
| Board | Waveshare ESP32-S3-Touch-LCD-1.69 |
| Battery | 603040 LiPo, 3.7 V, 800 mAh, protected, MX1.25 plug |
| LEDs | WS2812B strip, 3–6 LEDs |
| Case | Shell, cover, divider (printed) |

## Case

| Feature | Value (mm) |
| --- | --- |
| Outer size | 53.5 W × 40.5 H × 26.9 D |
| Corner radius (seen from the front) | 11.0 |
| Front and back edge rounding | 4.0 |
| Walls | 3.2 sides, 1.0 front bezel, 1.8 back |
| Board pocket | 41.63 × 33.63, corner radius 4.25 (board + 0.25 per side) |
| Battery zone behind the board | 47.1 × 34.1, corner radius 3.0 |
| Battery pocket (inside the cover walls) | 45 × 32 × 7.6 deep, walls 1.0 |
| Screen window | 34.03 × 29.37, corner radius 5.0, shifted 0.85 left and 0.4 up to match the image |
| USB-C slot | 9.6 × 4.6, centred on the bottom edge, centre 8.2 behind the glass |
| USB-C plug recess (outside) | 12.6 × 7.6 |
| Divider wire slot | 9 × 5, bottom-left |

## Wiring

| From | To |
| --- | --- |
| Battery plug | Board **BAT** connector (red = +) |
| LED strip + | Board **3V3** |
| LED strip G | Board **G** |
| LED strip DIN | Board **18** |

- Free pads: 17, 3, 2. SCL/SDA are used by touch, IMU, RTC. 5V is live only on USB.
- LED brightness capped at \~20% in code.
- Solder at 330–350 °C (leaded), 350–370 °C (lead-free).

## Assembly

1. Board face down into the shell, USB-C into its hole.
2. Divider on top, wire slot bottom-left.
3. Battery into the cover pocket, wires through the slot to BAT.
4. Press the cover into the shell.

## Files

- `beetleboy_shell.stl`, `beetleboy_cover.stl`, `beetleboy_divider.stl`, `beetleboy_all_parts.3mf`
- `beetleboy.blend` (Blender 4.3), `beetleboy_blender.py` (rebuilds the scene from the numbers at its top)
