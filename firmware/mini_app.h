#ifndef CODEX_MINI_IDE_MINI_APP_H
#define CODEX_MINI_IDE_MINI_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "firmware/mini_input.h"

void IDE_APP_Init(void);
void IDE_APP_Tick(uint16_t elapsed_ms);
void IDE_APP_HandleEvent(const ide_input_event_t *event);
bool IDE_APP_NeedsRender(void);
void IDE_APP_Render(void);

#endif
