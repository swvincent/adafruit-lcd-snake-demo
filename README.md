# Adafruit LCD Snake Demo

Autonomous [Snake](https://en.wikipedia.org/wiki/Snake_(video_game_genre)) demo for an [Arduino Leonardo](https://docs.arduino.cc/hardware/leonardo/) driving an [Adafruit 20×4 HD44780 LCD](https://www.adafruit.com/product/198) over I2C via the [Adafruit I2C/SPI LCD backpack](https://www.adafruit.com/product/292).

The snake chases food on its own—no buttons or joystick required.

## Hardware

| Leonardo | Backpack |
|----------|----------|
| D2 (SDA) | SDA / DAT |
| D3 (SCL) | SCL / CLK |
| 5V | 5V |
| GND | GND |

Power the backpack from the Leonardo (`5V` and `GND`). Hardware I2C on the Leonardo already uses D2/D3, which matches this backpack.

**I2C address:** default offset `0` (`0x20`) with no A0–A2 jumpers soldered. If you change the jumpers, update the sketch constructor (`Adafruit_LiquidCrystal lcd(0);`) to match.

**Contrast:** adjust the backpack’s contrast potentiometer until characters are sharp. If the screen looks blank or solid blocks, the pot is usually the first thing to check.

## Library

Install **Adafruit LiquidCrystal** from the Arduino IDE Library Manager:

1. **Sketch → Include Library → Manage Libraries…**
2. Search for `Adafruit LiquidCrystal`
3. Install the Adafruit package

Docs: [Arduino I2C use](https://learn.adafruit.com/i2c-spi-lcd-backpack/arduino-i2c-use). For the current API, use the library’s [HelloWorld_i2c](https://github.com/adafruit/Adafruit_LiquidCrystal/blob/master/examples/HelloWorld_i2c/HelloWorld_i2c.ino) example (`Adafruit_LiquidCrystal`); some learn-guide snippets still show the older `LiquidCrystal` name.

## Upload

1. Open `snake_demo/snake_demo.ino` in the Arduino IDE
2. **Tools → Board → Arduino Leonardo**
3. Select the correct port
4. Upload

## Behavior

- Autonomous AI only (no player input)
- Grid: 20 columns × 4 rows (one LCD cell per segment)
- Fixed snake length of 8 (does not grow)
- Edges wrap (modulo 20 / 4)
- Prefers moves that avoid overlapping its own body
- One food; respawns on a random empty cell when eaten
- Step rate: 250 ms (`STEP_MS`)
- Custom CGRAM glyphs for a thinner snake and distinct head/food

See [PLAN.md](PLAN.md) for the full design notes.
