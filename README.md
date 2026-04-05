# Mini IDE Firmware

This folder now contains a standalone firmware for the UV-K5 family.
It does not patch the stock UI. It is its own firmware image with:

- a tiny on-radio code editor,
- phone-style multi-tap text input,
- a simple script compiler,
- a non-blocking script runner.

The target is not full Python. The MCU only has `60 KB FLASH` and `16 KB RAM`, so the practical solution is a compact language that feels familiar, but executes as a small internal VM.

## What is already implemented

- standalone firmware entry point in `firmware/main.c`
- hardware init for LCD, keypad, backlight, stock red/green LEDs and a few script-safe GPIOs
- 4-line LCD UI:
  - 3 editor lines
  - 1 status line
- multi-tap keypad entry
- line split, backspace, cursor movement, line scrolling
- compile and run directly on the radio
- stop running scripts without rebooting

## Current controls

Edit mode:

- `0-9` and `*`: enter text with phone-style multi-tap
- `F`: toggle lowercase / uppercase
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

The runtime intentionally exposes only a small safe set for now:

- `red`: stock red TX LED via BK4819
- `green`: stock green RX LED via BK4819
- `pb14`: external LED2
- `pc3`: stock flashlight transistor output
- `pb6`: backlight output
- `pc5`: PTT line

That keeps the editor usable and avoids letting a script instantly kill the LCD or keypad by reconfiguring their pins.

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
