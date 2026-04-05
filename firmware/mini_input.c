#include "firmware/mini_input.h"

#define IDE_INPUT_DEBOUNCE_TICKS   2U
#define IDE_INPUT_LONG_PRESS_MS    600U
#define IDE_INPUT_TICK_MS          10U

static KEY_Code_t gRawKey = KEY_INVALID;
static KEY_Code_t gStableKey = KEY_INVALID;
static uint8_t gDebounceTicks;
static uint16_t gHoldMs;
static bool gLongSent;

void IDE_INPUT_Init(void)
{
	gRawKey = KEY_INVALID;
	gStableKey = KEY_INVALID;
	gDebounceTicks = 0U;
	gHoldMs = 0U;
	gLongSent = false;
}

bool IDE_INPUT_Tick(ide_input_event_t *event)
{
	const KEY_Code_t sample = KEYBOARD_Poll();

	event->type = IDE_INPUT_EVENT_NONE;
	event->key = KEY_INVALID;

	if (sample != gRawKey) {
		gRawKey = sample;
		gDebounceTicks = 0U;
	} else if (gDebounceTicks < 0xFFU) {
		gDebounceTicks++;
	}

	if (gDebounceTicks == IDE_INPUT_DEBOUNCE_TICKS && gStableKey != gRawKey) {
		gStableKey = gRawKey;
		gHoldMs = 0U;
		gLongSent = false;
		if (gStableKey != KEY_INVALID) {
			event->type = IDE_INPUT_EVENT_PRESS;
			event->key = gStableKey;
			return true;
		}
	}

	if (gStableKey != KEY_INVALID && !gLongSent) {
		gHoldMs = (uint16_t)(gHoldMs + IDE_INPUT_TICK_MS);
		if (gHoldMs >= IDE_INPUT_LONG_PRESS_MS) {
			gLongSent = true;
			event->type = IDE_INPUT_EVENT_LONG;
			event->key = gStableKey;
			return true;
		}
	}

	return false;
}
