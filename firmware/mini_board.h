#ifndef CODEX_MINI_IDE_MINI_BOARD_H
#define CODEX_MINI_IDE_MINI_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "src/mini_script.h"

void IDE_BOARD_Init(void);
void IDE_BOARD_ResetScriptPins(void);
bool IDE_BOARD_IsSupportedPin(mini_pin_t pin);
void IDE_BOARD_PinMode(mini_pin_t pin, mini_pin_mode_t mode);
void IDE_BOARD_DigitalWrite(mini_pin_t pin, bool value);
bool IDE_BOARD_DigitalRead(mini_pin_t pin);
void IDE_BOARD_SleepMs(uint16_t value);

#endif
