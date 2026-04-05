#ifndef CODEX_MINI_IDE_MINI_DISPLAY_H
#define CODEX_MINI_IDE_MINI_DISPLAY_H

#include <stdint.h>

void IDE_DISPLAY_Clear(void);
void IDE_DISPLAY_DrawTextLine(uint8_t line, const char *text);
void IDE_DISPLAY_InvertCell(uint8_t line, uint8_t column);
void IDE_DISPLAY_Present(void);

#endif
