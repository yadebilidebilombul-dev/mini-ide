#include <stdint.h>

#include "firmware/mini_app.h"
#include "firmware/mini_board.h"
#include "firmware/mini_input.h"

static volatile uint32_t gPendingTicks;

void SystickHandler(void);
void Main(void);

void SystickHandler(void)
{
	gPendingTicks++;
}

void Main(void)
{
	ide_input_event_t event;

	IDE_BOARD_Init();
	IDE_INPUT_Init();
	IDE_APP_Init();

	while (1) {
		while (gPendingTicks != 0U) {
			gPendingTicks--;
			while (IDE_INPUT_Tick(&event)) {
				IDE_APP_HandleEvent(&event);
			}
			IDE_APP_Tick(10U);
		}

		if (IDE_APP_NeedsRender()) {
			IDE_APP_Render();
		}
	}
}
