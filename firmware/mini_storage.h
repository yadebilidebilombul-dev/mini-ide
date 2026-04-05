#ifndef CODEX_MINI_IDE_MINI_STORAGE_H
#define CODEX_MINI_IDE_MINI_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	IDE_STORAGE_SAVE_OK = 0,
	IDE_STORAGE_SAVE_UNCHANGED,
	IDE_STORAGE_SAVE_TOO_LARGE,
	IDE_STORAGE_SAVE_FAILED,
} ide_storage_save_result_t;

void IDE_STORAGE_Init(void);
bool IDE_STORAGE_LoadScript(char *buffer, uint16_t buffer_size);
ide_storage_save_result_t IDE_STORAGE_SaveScript(const char *source);

#endif
