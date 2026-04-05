# Mini Script Language v0

This is the first scripting language for the standalone mini-IDE firmware.
It is deliberately small and easy to parse on the DP32G030.

## Format

- one command per line
- empty lines are allowed
- comments start with `#`
- pin names are written as `pa0`, `pb14`, `pc5`
- labels are used for loops and jumps

## Commands

`pinmode <PIN> <in|out>`

- configure a pin as input or output

`write <PIN> <0|1>`

- write a logic level to an output pin

`toggle <PIN>`

- invert an output pin

`sleep <MS>`

- wait a number of milliseconds

`wait <PIN> <0|1>`

- pause until a pin reaches the requested level

`label <NAME>`

- define a jump label

`goto <NAME>`

- jump to a label

`if <PIN> <0|1> goto <NAME>`

- conditional jump based on pin input level

`end`

- stop program execution

## Example: blink LED2

```text
pinmode pb14 out
label loop
write pb14 1
sleep 100
write pb14 0
sleep 100
goto loop
```

## Example: button to LED

```text
pinmode pc5 in
pinmode pb14 out
label loop
if pc5 0 goto pressed
write pb14 0
goto loop
label pressed
write pb14 1
goto loop
```

## Notes

- pin names are case-insensitive
- labels are case-insensitive in the current implementation
- the current firmware runtime exposes only a small safe whitelist of pins
