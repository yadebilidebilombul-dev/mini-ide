#include "firmware/mini_display.h"

#include <string.h>

#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/spi.h"
#include "driver/gpio.h"
#include "driver/spi.h"
#include "driver/st7565.h"
#include "font.h"

#define IDE_DISPLAY_CHAR_WIDTH   8U
#define IDE_DISPLAY_LINE_COUNT   4U
#define IDE_DISPLAY_PAGE_COUNT   8U
#define IDE_DISPLAY_PAGE_WIDTH   128U

static uint8_t gIdeDisplayBuffer[IDE_DISPLAY_PAGE_COUNT][IDE_DISPLAY_PAGE_WIDTH];

void IDE_DISPLAY_Clear(void)
{
	memset(gIdeDisplayBuffer, 0, sizeof(gIdeDisplayBuffer));
}

void IDE_DISPLAY_DrawTextLine(uint8_t line, const char *text)
{
	uint8_t column;
	const uint8_t page = (uint8_t)(line * 2U);

	if (line >= IDE_DISPLAY_LINE_COUNT) {
		return;
	}

	memset(gIdeDisplayBuffer[page + 0U], 0, IDE_DISPLAY_PAGE_WIDTH);
	memset(gIdeDisplayBuffer[page + 1U], 0, IDE_DISPLAY_PAGE_WIDTH);

	for (column = 0; column < 16U && text[column] != 0; column++) {
		const char ch = text[column];

		if (ch >= ' ' && ch < 0x7F) {
			const uint8_t glyph = (uint8_t)(ch - ' ');

			memcpy(&gIdeDisplayBuffer[page + 0U][column * IDE_DISPLAY_CHAR_WIDTH], &gFontBig[glyph][0], 8U);
			memcpy(&gIdeDisplayBuffer[page + 1U][column * IDE_DISPLAY_CHAR_WIDTH], &gFontBig[glyph][8], 8U);
		}
	}
}

void IDE_DISPLAY_InvertCell(uint8_t line, uint8_t column)
{
	uint8_t x;
	const uint8_t page = (uint8_t)(line * 2U);
	const uint8_t start = (uint8_t)(column * IDE_DISPLAY_CHAR_WIDTH);

	if (line >= IDE_DISPLAY_LINE_COUNT || column >= 16U) {
		return;
	}

	for (x = 0; x < IDE_DISPLAY_CHAR_WIDTH; x++) {
		gIdeDisplayBuffer[page + 0U][start + x] ^= 0xFFU;
		gIdeDisplayBuffer[page + 1U][start + x] ^= 0xFFU;
	}
}

void IDE_DISPLAY_Present(void)
{
	uint8_t page;
	uint8_t column;

	SPI_ToggleMasterMode(&SPI0->CR, false);
	ST7565_WriteByte(0x40);

	for (page = 0; page < IDE_DISPLAY_PAGE_COUNT; page++) {
		ST7565_SelectColumnAndLine(4U, page);
		GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_ST7565_A0);
		for (column = 0; column < IDE_DISPLAY_PAGE_WIDTH; column++) {
			while ((SPI0->FIFOST & SPI_FIFOST_TFF_MASK) != SPI_FIFOST_TFF_BITS_NOT_FULL) {
			}
			SPI0->WDR = gIdeDisplayBuffer[page][column];
		}
		SPI_WaitForUndocumentedTxFifoStatusBit();
	}

	SPI_ToggleMasterMode(&SPI0->CR, true);
}
