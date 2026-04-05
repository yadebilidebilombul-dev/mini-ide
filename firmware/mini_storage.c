#include "firmware/mini_storage.h"

#include <string.h>

#include "ARMCM0.h"
#include "bsp/dp32g030/flash.h"
#include "driver/flash.h"
#include "sram-overlay.h"

#define IDE_STORAGE_BASE         0x0000E800U
#define IDE_STORAGE_SIZE         0x00000800U
#define IDE_STORAGE_MAGIC        0x4544494DU
#define IDE_STORAGE_VERSION      1U
#define IDE_STORAGE_HEADER_WORDS 4U

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t length;
	uint32_t checksum;
	uint32_t reserved;
} ide_storage_header_t;

static uint32_t IDE_STORAGE_Checksum(const uint8_t *data, uint16_t length) __attribute__((section(".sramtext")));
static uint32_t IDE_STORAGE_Checksum(const uint8_t *data, uint16_t length)
{
	uint32_t hash = 2166136261U;
	uint16_t i;

	for (i = 0; i < length; i++) {
		hash ^= data[i];
		hash *= 16777619U;
	}

	return hash;
}

static bool IDE_STORAGE_BytesEqual(const uint8_t *left, const uint8_t *right, uint16_t length) __attribute__((section(".sramtext")));
static bool IDE_STORAGE_BytesEqual(const uint8_t *left, const uint8_t *right, uint16_t length)
{
	uint16_t i;

	for (i = 0; i < length; i++) {
		if (left[i] != right[i]) {
			return false;
		}
	}

	return true;
}

static const ide_storage_header_t *IDE_STORAGE_GetHeader(void)
{
	return (const ide_storage_header_t *)IDE_STORAGE_BASE;
}

static const uint8_t *IDE_STORAGE_GetPayload(void)
{
	return (const uint8_t *)(IDE_STORAGE_BASE + sizeof(ide_storage_header_t));
}

static bool IDE_STORAGE_IsBlank(void)
{
	const uint32_t *words = (const uint32_t *)IDE_STORAGE_BASE;
	uint16_t i;

	for (i = 0; i < (IDE_STORAGE_SIZE / 4U); i++) {
		if (words[i] != 0xFFFFFFFFU) {
			return false;
		}
	}

	return true;
}

static bool IDE_STORAGE_HeaderIsValid(const ide_storage_header_t *header)
{
	if (header->magic != IDE_STORAGE_MAGIC || header->version != IDE_STORAGE_VERSION) {
		return false;
	}

	if (header->length == 0U || header->length > (IDE_STORAGE_SIZE - sizeof(ide_storage_header_t) - 1U)) {
		return false;
	}

	return IDE_STORAGE_Checksum(IDE_STORAGE_GetPayload(), header->length) == header->checksum;
}

static void IDE_STORAGE_FlashWait(void) __attribute__((section(".sramtext")));
static void IDE_STORAGE_FlashWait(void)
{
	while (overlay_FLASH_IsBusy()) {
	}
}

static bool IDE_STORAGE_FlashEraseSector(uint32_t address) __attribute__((section(".sramtext")));
static bool IDE_STORAGE_FlashEraseSector(uint32_t address)
{
	overlay_FLASH_SetArea(FLASH_AREA_MAIN);
	overlay_FLASH_SetMode(FLASH_MODE_ERASE);
	FLASH_ADDR = address >> 2;
	overlay_FLASH_Start();
	IDE_STORAGE_FlashWait();
	overlay_FLASH_SetMode(FLASH_MODE_READ_AHB);
	overlay_FLASH_Lock();
	return true;
}

static bool IDE_STORAGE_FlashProgramWord(uint32_t address, uint32_t value) __attribute__((section(".sramtext")));
static bool IDE_STORAGE_FlashProgramWord(uint32_t address, uint32_t value)
{
	overlay_FLASH_SetArea(FLASH_AREA_MAIN);
	overlay_FLASH_SetMode(FLASH_MODE_PROGRAM);
	FLASH_ADDR = (address >> 2) + 0xC000U;
	FLASH_WDATA = value;
	overlay_FLASH_Start();
	while ((FLASH_ST & FLASH_ST_PROG_BUF_EMPTY_MASK) == FLASH_ST_PROG_BUF_EMPTY_BITS_NOT_EMPTY) {
	}
	IDE_STORAGE_FlashWait();
	overlay_FLASH_SetMode(FLASH_MODE_READ_AHB);
	overlay_FLASH_Lock();
	return true;
}

static bool IDE_STORAGE_Verify(const char *source, uint16_t length) __attribute__((section(".sramtext")));
static bool IDE_STORAGE_Verify(const char *source, uint16_t length)
{
	const ide_storage_header_t *header = IDE_STORAGE_GetHeader();

	if (header->magic != IDE_STORAGE_MAGIC || header->version != IDE_STORAGE_VERSION || header->length != length) {
		return false;
	}

	if (header->checksum != IDE_STORAGE_Checksum((const uint8_t *)source, length)) {
		return false;
	}

	return IDE_STORAGE_BytesEqual(IDE_STORAGE_GetPayload(), (const uint8_t *)source, length);
}

void IDE_STORAGE_Init(void)
{
	overlay_FLASH_MainClock = 48000000U;
	overlay_FLASH_ClockMultiplier = 48U;
	FLASH_Init(FLASH_READ_MODE_2_CYCLE);
}

bool IDE_STORAGE_LoadScript(char *buffer, uint16_t buffer_size)
{
	const ide_storage_header_t *header = IDE_STORAGE_GetHeader();
	uint16_t length;

	if (buffer == 0 || buffer_size == 0U) {
		return false;
	}
	if (!IDE_STORAGE_HeaderIsValid(header)) {
		return false;
	}

	length = header->length;
	if (length + 1U > buffer_size) {
		return false;
	}

	memcpy(buffer, IDE_STORAGE_GetPayload(), length);
	buffer[length] = 0;
	return true;
}

ide_storage_save_result_t IDE_STORAGE_SaveScript(const char *source) __attribute__((section(".sramtext")));
ide_storage_save_result_t IDE_STORAGE_SaveScript(const char *source)
{
	ide_storage_header_t header;
	uint32_t address;
	uint32_t word;
	uint16_t length;
	uint16_t i;
	bool irq_enabled;

	if (source == 0) {
		return IDE_STORAGE_SAVE_FAILED;
	}

	length = (uint16_t)strlen(source);
	if (length == 0U || length > (IDE_STORAGE_SIZE - sizeof(ide_storage_header_t) - 1U)) {
		return IDE_STORAGE_SAVE_TOO_LARGE;
	}

	if (!IDE_STORAGE_IsBlank() && IDE_STORAGE_HeaderIsValid(IDE_STORAGE_GetHeader())) {
		const ide_storage_header_t *current = IDE_STORAGE_GetHeader();

		if (current->length == length && memcmp(IDE_STORAGE_GetPayload(), source, length) == 0) {
			return IDE_STORAGE_SAVE_UNCHANGED;
		}
	}

	header.magic = IDE_STORAGE_MAGIC;
	header.version = IDE_STORAGE_VERSION;
	header.length = length;
	header.checksum = IDE_STORAGE_Checksum((const uint8_t *)source, length);
	header.reserved = 0xFFFFFFFFU;

	irq_enabled = (__get_PRIMASK() == 0U);
	__disable_irq();

	for (address = IDE_STORAGE_BASE; address < IDE_STORAGE_BASE + IDE_STORAGE_SIZE; address += 0x100U) {
		if (!IDE_STORAGE_FlashEraseSector(address)) {
			if (irq_enabled) {
				__enable_irq();
			}
			return IDE_STORAGE_SAVE_FAILED;
		}
	}

	address = IDE_STORAGE_BASE;
	if (!IDE_STORAGE_FlashProgramWord(address + 0U, header.magic) ||
		!IDE_STORAGE_FlashProgramWord(address + 4U, ((uint32_t)header.version << 16) | header.length) ||
		!IDE_STORAGE_FlashProgramWord(address + 8U, header.checksum) ||
		!IDE_STORAGE_FlashProgramWord(address + 12U, header.reserved)) {
		if (irq_enabled) {
			__enable_irq();
		}
		return IDE_STORAGE_SAVE_FAILED;
	}

	address += sizeof(ide_storage_header_t);
	for (i = 0; i < length; i += 4U) {
		word = 0xFFFFFFFFU;
		if (i + 0U < length) {
			word = (word & ~0x000000FFU) | ((uint32_t)(uint8_t)source[i + 0U] << 0);
		}
		if (i + 1U < length) {
			word = (word & ~0x0000FF00U) | ((uint32_t)(uint8_t)source[i + 1U] << 8);
		}
		if (i + 2U < length) {
			word = (word & ~0x00FF0000U) | ((uint32_t)(uint8_t)source[i + 2U] << 16);
		}
		if (i + 3U < length) {
			word = (word & ~0xFF000000U) | ((uint32_t)(uint8_t)source[i + 3U] << 24);
		}
		if (!IDE_STORAGE_FlashProgramWord(address + i, word)) {
			if (irq_enabled) {
				__enable_irq();
			}
			return IDE_STORAGE_SAVE_FAILED;
		}
	}

	if (irq_enabled) {
		__enable_irq();
	}

	if (!IDE_STORAGE_Verify(source, length)) {
		return IDE_STORAGE_SAVE_FAILED;
	}

	return IDE_STORAGE_SAVE_OK;
}
