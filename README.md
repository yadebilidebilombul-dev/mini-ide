# Mini IDE Firmware

This repository contains a standalone firmware for the UV-K5 family. It does not patch the stock UI. Instead, it provides its own firmware image with:

- a tiny on-radio code editor,
- phone-style multi-tap text input,
- a simple script compiler,
- a non-blocking script runner.

The target is not full Python. The MCU only has `60 KB FLASH` and `16 KB RAM`, so the practical solution is a compact language that feels familiar, but executes as a small internal VM.

## What is already implemented

- standalone firmware entry point in `firmware/main.c`
- hardware init for LCD, keypad, backlight, flashlight output and a few script-safe GPIOs
- 4-line LCD UI:
  - 3 editor lines
  - 1 status line
- multi-tap keypad entry
- line split, backspace, cursor movement, line scrolling
- compile and run directly on the radio
- stop running scripts without rebooting
- speaker tone command via `beep <hz> <ms>`

## Current controls

Edit mode:

- `0-9` and `*`: enter text with phone-style multi-tap
- `F`: toggle lowercase / uppercase
- long `F`: save current script to reserved flash
- `MENU`: split line at cursor
- long `MENU`: open the IDE help screen
- `PTT`: compile and run script
- `EXIT`: backspace
- long `EXIT`: clear current line
- `UP` / `DOWN`: move between lines
- `SIDE1`: move cursor left
- `SIDE2`: move cursor right

Help mode:

- `UP` / `DOWN`: previous / next help page
- `SIDE1` / `SIDE2`: previous / next help page
- `MENU`, long `MENU`, or `EXIT`: close help

Run mode:

- `PTT`: stop script

## Current safe script pins

The runtime exposes 26 safe GPIO pins carefully selected to not interfere with the radio's critical subsystems:

**GPIOA (4 pins) - Free pins:**
- `pa0`: free output/input
- `pa1`: free output/input
- `pa2`: free output/input
- `pa15`: free output/input

**GPIOB (10 pins) - Mix of original + free pins:**
- `pb0`: free output/input
- `pb1`: free output/input
- `pb2`: free output/input
- `pb3`: free output/input
- `pb4`: free output/input
- `pb5`: free output/input
- `pb6`: backlight output
- `pb12`: free output/input
- `pb13`: free output/input
- `pb14`: external LED2 output

**GPIOC (12 pins) - Original PTT + Flashlight + Free pins:**
- `pc3`: stock flashlight transistor output
- `pc5`: PTT line (input/output capable)
- `pc6`: free output/input
- `pc7`: free output/input
- `pc8`: free output/input
- `pc9`: free output/input
- `pc10`: free output/input
- `pc11`: free output/input
- `pc12`: free output/input
- `pc13`: free output/input
- `pc14`: free output/input
- `pc15`: free output/input

All pins are verified not to be used by:
- Radio IC (BK4819) SCN/SCL/SDA
- LCD (ST7565) control lines
- SPI flash interface
- Keyboard matrix
- Audio path
- Battery monitoring ADC
- UART/debugger pins
- FM radio IC (BK1080)

This deliberately excludes critical pins to keep the radio functional and prevent script-based damage.

## Files

- `LANGUAGE.md`: current script syntax
- `src/mini_script.h`: compiler + VM API
- `src/mini_script.c`: compiler + VM implementation
- `firmware/mini_board.c`: low-level hardware init and GPIO HAL
- `firmware/mini_display.c`: 4-line framebuffer renderer
- `firmware/mini_input.c`: debounced keypad input with long-press handling
- `firmware/mini_app.c`: editor UI and script launcher
- `firmware/main.c`: firmware entry point
- `build.ps1`: standalone build script for Windows

## Build output

Running `build.ps1` creates:

- `mini-ide.elf`
- `mini-ide.bin`
- `mini-ide.packed.bin`

Use `mini-ide.bin` for raw flashing tools and `mini-ide.packed.bin` for stock-style updaters.
