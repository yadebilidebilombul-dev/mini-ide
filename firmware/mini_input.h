#ifndef CODEX_MINI_IDE_MINI_INPUT_H
#define CODEX_MINI_IDE_MINI_INPUT_H

#include <stdbool.h>

#include "driver/keyboard.h"

typedef enum {
	IDE_INPUT_EVENT_NONE = 0,
	IDE_INPUT_EVENT_PRESS,
	IDE_INPUT_EVENT_LONG,
} ide_input_event_type_t;

typedef struct {
	ide_input_event_type_t type;
	KEY_Code_t key;
} ide_input_event_t;

void IDE_INPUT_Init(void);
bool IDE_INPUT_Tick(ide_input_event_t *event);

#endif
