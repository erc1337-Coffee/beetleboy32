# Hardware

BeetleBoy32 runs on the **Waveshare ESP32-S3-Touch-LCD-1.69**: an ESP32-S3 with 16 MB flash, 8 MB PSRAM, a 1.69" 240×280 touchscreen (ST7789V2 + CST816T), a buzzer, an RTC and a battery connector. The app uses the screen in landscape (280×240).

All pin numbers are in [`include/board_config.h`](../include/board_config.h).

## What's used

| Part | Pins | Used for |
|---|---|---|
| Display (ST7789V2, SPI) | DC 4, CS 5, SCK 6, MOSI 7, RST 8, backlight 15 | The UI |
| Touch (CST816T, I2C 0x15) | SDA 11, SCL 10, RST 13, INT 14 | Taps and swipes, waking the screen |
| RTC (PCF85063, I2C 0x51) | shared I2C | Keeps the time while powered off; set from the internet when online |
| Buzzer | 42 | Sound effects (volume in Settings) |
| Battery voltage | 1 (VBAT / 3) | The battery percentage in the menu |
| Power key | SYS_EN 41, SYS_OUT 40 | On battery, the firmware holds SYS_EN high to stay powered |

The board also has a motion sensor (QMI8658). The firmware doesn't use it.

## Optional extras

**Buttons.** Wire each one between its GPIO and GND. No resistor is needed: the internal pull-up is used.

| Button | GPIO | Does |
|---|---|---|
| Cancel | 2 | Back (hold 3 s: factory reset) |
| OK | 17 | Press the focused button |

The board has no Next button: GPIO 18 drives the LEDs. To add one, define `PIN_BTN_NEXT` on a free pin. GPIO 3 is free but is a strapping pin: don't hold it low at boot.

**LEDs.** 4 WS2812B ("NeoPixel") LEDs, chained: power from 3V3 and GND, data on GPIO 18 (`PIN_LEDS`, `LED_COUNT`). They glow green while the screen is on and animate on events (a poke, a hunt or craft in progress, a result). Settings → LED turns them off or sets the glow's brightness.

At full white, 4 LEDs draw about 240 mA from the 3V3 line, which is a lot for the board's regulator. The effects stay in white, green and cyan, at full brightness only for a few seconds.

## Flashing

```sh
./run flash      # build and flash over USB-C
./run monitor    # serial log (opening the port restarts the board)
```

If flashing fails to connect: hold **BOOT**, tap **RST**, release BOOT, then retry.

## Power

- **Screen off** after the idle time set in Settings (default 60 s). Wi-Fi stays connected, so pokes still arrive (checked every 45 s instead of 15 s). A tap or a button turns it back on. The LEDs go dark with the screen.
- **Sleep** after a longer idle time (default 30 min): the chip light-sleeps with Wi-Fi off until the next cooldown ends, or a touch or button wakes it.

  Waking from sleep by touch hasn't been confirmed on the board yet. With no buttons wired, keep the sleep time long.
- The CPU stays at 240 MHz: switching its speed while Wi-Fi runs hangs the chip with this Arduino core.

## Wi-Fi

The transmit power is set to 13 dBm. At full power (20 dBm) this board's transmissions came out garbled and routers never answered; 13 dBm joins reliably.
