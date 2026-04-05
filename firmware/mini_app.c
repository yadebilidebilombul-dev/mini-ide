#include "firmware/mini_app.h"

#include <string.h>

#include "firmware/mini_board.h"
#include "firmware/mini_display.h"
#include "src/mini_script.h"

#define IDE_MAX_LINES            16U
#define IDE_LINE_CAP             32U
#define IDE_VISIBLE_LINES        3U
#define IDE_STATUS_LINE          3U
#define IDE_CURSOR_BLINK_MS      400U
#define IDE_MULTI_TAP_MS         900U
#define IDE_STATUS_MS            1800U
#define IDE_VM_BUDGET            16U
#define IDE_HELP_PAGE_COUNT      4U

typedef struct {
	char lines[IDE_MAX_LINES][IDE_LINE_CAP + 1U];
	uint8_t cursor_line;
	uint8_t cursor_col;
	uint8_t viewport_top;
	bool uppercase;
	bool dirty;
	bool cursor_visible;
	uint16_t cursor_blink_ms;
	char status_text[17];
	uint16_t status_timeout_ms;
	bool is_running;
	bool help_visible;
	uint8_t help_page;
	bool pending_active;
	KEY_Code_t pending_key;
	uint8_t pending_index;
	uint8_t pending_line;
	uint8_t pending_col;
	uint16_t pending_timeout_ms;
	mini_program_t program;
	mini_vm_t vm;
} ide_app_t;

static ide_app_t gIdeApp;

static const mini_hal_t gIdeHal = {
	IDE_BOARD_IsSupportedPin,
	IDE_BOARD_PinMode,
	IDE_BOARD_DigitalWrite,
	IDE_BOARD_DigitalRead,
	IDE_BOARD_SleepMs,
};

static const char *const gIdeHelpPages[IDE_HELP_PAGE_COUNT][3] = {
	{
		"long menu help  ",
		"ptt run or stop ",
		"f case exit bksp",
	},
	{
		"0-9 * multi tap ",
		"menu split line ",
		"side mv updn row",
	},
	{
		"pinmode red out ",
		"write red 1     ",
		"write green 1   ",
	},
	{
		"sleep 100       ",
		"label loop      ",
		"if pc5 0 goto x ",
	},
};

static void IDE_APP_CopyText16(char *dest, const char *src)
{
	uint8_t i;

	for (i = 0; i < 16U && src[i] != 0; i++) {
		dest[i] = src[i];
	}
	for (; i < 16U; i++) {
		dest[i] = ' ';
	}
	dest[16] = 0;
}

static uint8_t IDE_APP_LineLength(uint8_t line)
{
	uint8_t length = 0U;

	while (length < IDE_LINE_CAP && gIdeApp.lines[line][length] != 0) {
		length++;
	}

	return length;
}

static uint8_t IDE_APP_LastUsedLine(void)
{
	int16_t line;

	for (line = IDE_MAX_LINES - 1; line >= 0; line--) {
		if (gIdeApp.lines[line][0] != 0) {
			return (uint8_t)line;
		}
	}

	return 0U;
}

static void IDE_APP_SetStatus(const char *text, uint16_t timeout_ms)
{
	IDE_APP_CopyText16(gIdeApp.status_text, text);
	gIdeApp.status_timeout_ms = timeout_ms;
	gIdeApp.dirty = true;
}

static void IDE_APP_ClampCursor(void)
{
	const uint8_t length = IDE_APP_LineLength(gIdeApp.cursor_line);

	if (gIdeApp.cursor_col > length) {
		gIdeApp.cursor_col = length;
	}
}

static void IDE_APP_UpdateViewport(void)
{
	if (gIdeApp.cursor_line < gIdeApp.viewport_top) {
		gIdeApp.viewport_top = gIdeApp.cursor_line;
	} else if (gIdeApp.cursor_line >= gIdeApp.viewport_top + IDE_VISIBLE_LINES) {
		gIdeApp.viewport_top = (uint8_t)(gIdeApp.cursor_line - IDE_VISIBLE_LINES + 1U);
	}
}

static char IDE_APP_ApplyCase(char value)
{
	if (gIdeApp.uppercase && value >= 'a' && value <= 'z') {
		return (char)(value - ('a' - 'A'));
	}

	return value;
}

static const char *IDE_APP_KeyGroup(KEY_Code_t key)
{
	switch (key) {
	case KEY_0:
		return " 0";
	case KEY_1:
		return ".,#1";
	case KEY_2:
		return "abc2";
	case KEY_3:
		return "def3";
	case KEY_4:
		return "ghi4";
	case KEY_5:
		return "jkl5";
	case KEY_6:
		return "mno6";
	case KEY_7:
		return "pqrs7";
	case KEY_8:
		return "tuv8";
	case KEY_9:
		return "wxyz9";
	case KEY_STAR:
		return "_-*/=()";
	default:
		return 0;
	}
}

static void IDE_APP_InsertCharAtCursor(char value)
{
	char *line = gIdeApp.lines[gIdeApp.cursor_line];
	const uint8_t length = IDE_APP_LineLength(gIdeApp.cursor_line);
	uint8_t index;

	if (length >= IDE_LINE_CAP) {
		IDE_APP_SetStatus("line is full", IDE_STATUS_MS);
		return;
	}

	for (index = length + 1U; index > gIdeApp.cursor_col; index--) {
		line[index] = line[index - 1U];
	}

	line[gIdeApp.cursor_col] = value;
	gIdeApp.dirty = true;
}

static void IDE_APP_DeletePendingChar(void)
{
	char *line = gIdeApp.lines[gIdeApp.pending_line];
	const uint8_t length = IDE_APP_LineLength(gIdeApp.pending_line);
	uint8_t index;

	for (index = gIdeApp.pending_col; index < length; index++) {
		line[index] = line[index + 1U];
	}
}

static void IDE_APP_FinalizePendingTap(void)
{
	if (!gIdeApp.pending_active) {
		return;
	}

	gIdeApp.cursor_line = gIdeApp.pending_line;
	gIdeApp.cursor_col = (uint8_t)(gIdeApp.pending_col + 1U);
	gIdeApp.pending_active = false;
	gIdeApp.pending_key = KEY_INVALID;
	gIdeApp.pending_timeout_ms = 0U;
	IDE_APP_ClampCursor();
	gIdeApp.dirty = true;
}

static void IDE_APP_CancelPendingTap(void)
{
	if (!gIdeApp.pending_active) {
		return;
	}

	IDE_APP_DeletePendingChar();
	gIdeApp.cursor_line = gIdeApp.pending_line;
	gIdeApp.cursor_col = gIdeApp.pending_col;
	gIdeApp.pending_active = false;
	gIdeApp.pending_key = KEY_INVALID;
	gIdeApp.pending_timeout_ms = 0U;
	gIdeApp.dirty = true;
}

static void IDE_APP_HandleTextKey(KEY_Code_t key)
{
	const char *group = IDE_APP_KeyGroup(key);
	const uint8_t group_length = (uint8_t)strlen(group);

	if (group == 0) {
		return;
	}

	if (gIdeApp.pending_active && gIdeApp.pending_key == key) {
		gIdeApp.pending_index = (uint8_t)((gIdeApp.pending_index + 1U) % group_length);
		gIdeApp.lines[gIdeApp.pending_line][gIdeApp.pending_col] = IDE_APP_ApplyCase(group[gIdeApp.pending_index]);
		gIdeApp.pending_timeout_ms = IDE_MULTI_TAP_MS;
		gIdeApp.dirty = true;
		return;
	}

	IDE_APP_FinalizePendingTap();
	IDE_APP_InsertCharAtCursor(IDE_APP_ApplyCase(group[0]));
	gIdeApp.pending_active = true;
	gIdeApp.pending_key = key;
	gIdeApp.pending_index = 0U;
	gIdeApp.pending_line = gIdeApp.cursor_line;
	gIdeApp.pending_col = gIdeApp.cursor_col;
	gIdeApp.pending_timeout_ms = IDE_MULTI_TAP_MS;
}

static void IDE_APP_MoveCursorLeft(void)
{
	IDE_APP_FinalizePendingTap();
	if (gIdeApp.cursor_col > 0U) {
		gIdeApp.cursor_col--;
	} else if (gIdeApp.cursor_line > 0U) {
		gIdeApp.cursor_line--;
		gIdeApp.cursor_col = IDE_APP_LineLength(gIdeApp.cursor_line);
	}
	IDE_APP_UpdateViewport();
	gIdeApp.dirty = true;
}

static void IDE_APP_MoveCursorRight(void)
{
	const uint8_t length = IDE_APP_LineLength(gIdeApp.cursor_line);

	IDE_APP_FinalizePendingTap();
	if (gIdeApp.cursor_col < length) {
		gIdeApp.cursor_col++;
	} else if (gIdeApp.cursor_line + 1U < IDE_MAX_LINES) {
		gIdeApp.cursor_line++;
		gIdeApp.cursor_col = 0U;
	}
	IDE_APP_UpdateViewport();
	gIdeApp.dirty = true;
}

static void IDE_APP_MoveCursorVertical(int8_t delta)
{
	int16_t line = (int16_t)gIdeApp.cursor_line + delta;

	IDE_APP_FinalizePendingTap();
	if (line < 0) {
		line = 0;
	} else if (line >= IDE_MAX_LINES) {
		line = IDE_MAX_LINES - 1;
	}

	gIdeApp.cursor_line = (uint8_t)line;
	IDE_APP_ClampCursor();
	IDE_APP_UpdateViewport();
	gIdeApp.dirty = true;
}

static void IDE_APP_SplitLine(void)
{
	uint8_t line;
	char tail[IDE_LINE_CAP + 1U];
	char *current;

	IDE_APP_FinalizePendingTap();
	if (IDE_APP_LastUsedLine() >= IDE_MAX_LINES - 1U && gIdeApp.lines[IDE_MAX_LINES - 1U][0] != 0) {
		IDE_APP_SetStatus("no free lines", IDE_STATUS_MS);
		return;
	}

	current = gIdeApp.lines[gIdeApp.cursor_line];
	strcpy(tail, &current[gIdeApp.cursor_col]);
	current[gIdeApp.cursor_col] = 0;

	for (line = IDE_MAX_LINES - 1U; line > gIdeApp.cursor_line + 1U; line--) {
		strcpy(gIdeApp.lines[line], gIdeApp.lines[line - 1U]);
	}
	strcpy(gIdeApp.lines[gIdeApp.cursor_line + 1U], tail);

	gIdeApp.cursor_line++;
	gIdeApp.cursor_col = 0U;
	IDE_APP_UpdateViewport();
	gIdeApp.dirty = true;
}

static void IDE_APP_Backspace(void)
{
	char *line = gIdeApp.lines[gIdeApp.cursor_line];
	const uint8_t length = IDE_APP_LineLength(gIdeApp.cursor_line);
	uint8_t index;

	if (gIdeApp.pending_active) {
		IDE_APP_CancelPendingTap();
		return;
	}

	if (gIdeApp.cursor_col > 0U) {
		for (index = gIdeApp.cursor_col - 1U; index < length; index++) {
			line[index] = line[index + 1U];
		}
		gIdeApp.cursor_col--;
		gIdeApp.dirty = true;
		return;
	}

	if (gIdeApp.cursor_line > 0U) {
		const uint8_t previous_length = IDE_APP_LineLength((uint8_t)(gIdeApp.cursor_line - 1U));

		if (previous_length + length > IDE_LINE_CAP) {
			IDE_APP_SetStatus("merge too long", IDE_STATUS_MS);
			return;
		}

		strcat(gIdeApp.lines[gIdeApp.cursor_line - 1U], line);
		for (index = gIdeApp.cursor_line; index + 1U < IDE_MAX_LINES; index++) {
			strcpy(gIdeApp.lines[index], gIdeApp.lines[index + 1U]);
		}
		gIdeApp.lines[IDE_MAX_LINES - 1U][0] = 0;
		gIdeApp.cursor_line--;
		gIdeApp.cursor_col = previous_length;
		IDE_APP_UpdateViewport();
		gIdeApp.dirty = true;
	}
}

static void IDE_APP_ClearLine(void)
{
	IDE_APP_CancelPendingTap();
	gIdeApp.lines[gIdeApp.cursor_line][0] = 0;
	gIdeApp.cursor_col = 0U;
	gIdeApp.dirty = true;
}

static void IDE_APP_BuildSource(char *buffer, uint16_t buffer_size)
{
	uint8_t line;
	uint16_t offset = 0U;
	const uint8_t last_used = IDE_APP_LastUsedLine();

	buffer[0] = 0;
	for (line = 0; line <= last_used && offset + 2U < buffer_size; line++) {
		const uint8_t length = IDE_APP_LineLength(line);
		uint8_t index;

		for (index = 0; index < length && offset + 2U < buffer_size; index++) {
			buffer[offset++] = gIdeApp.lines[line][index];
		}
		buffer[offset++] = '\n';
	}
	buffer[offset] = 0;
}

static void IDE_APP_StartProgram(void)
{
	char source[(IDE_MAX_LINES * (IDE_LINE_CAP + 1U)) + 1U];
	char error_text[17];
	mini_result_t result;

	IDE_APP_FinalizePendingTap();
	IDE_APP_BuildSource(source, sizeof(source));
	result = MINI_Compile(source, &gIdeApp.program, error_text, sizeof(error_text));
	if (result != MINI_OK) {
		IDE_APP_SetStatus(error_text, IDE_STATUS_MS);
		return;
	}

	result = MINI_VM_Start(&gIdeApp.vm, &gIdeApp.program);
	if (result != MINI_OK) {
		IDE_APP_SetStatus(MINI_ResultString(result), IDE_STATUS_MS);
		return;
	}

	gIdeApp.is_running = true;
	IDE_APP_SetStatus("running", 0U);
}

static void IDE_APP_StopProgram(const char *reason)
{
	MINI_VM_Stop(&gIdeApp.vm);
	IDE_BOARD_ResetScriptPins();
	gIdeApp.is_running = false;
	IDE_APP_SetStatus(reason, IDE_STATUS_MS);
}

static void IDE_APP_SetDefaultStatus(char *status)
{
	static const char edit_template[] = "edit l00 c00 lo ";
	static const char help_template[] = "help 0/0 updn x";
	static const char run_template[] = "run ptt stop   ";

	if (gIdeApp.is_running) {
		IDE_APP_CopyText16(status, run_template);
		return;
	}
	if (gIdeApp.help_visible) {
		IDE_APP_CopyText16(status, help_template);
		status[5] = (char)('1' + gIdeApp.help_page);
		status[7] = (char)('0' + IDE_HELP_PAGE_COUNT);
		return;
	}

	IDE_APP_CopyText16(status, edit_template);
	status[6] = (char)('0' + ((gIdeApp.cursor_line + 1U) / 10U));
	status[7] = (char)('0' + ((gIdeApp.cursor_line + 1U) % 10U));
	status[10] = (char)('0' + (gIdeApp.cursor_col / 10U));
	status[11] = (char)('0' + (gIdeApp.cursor_col % 10U));
	status[13] = gIdeApp.uppercase ? 'u' : 'l';
	status[14] = gIdeApp.uppercase ? 'p' : 'o';
}

void IDE_APP_Init(void)
{
	static const char *const default_script[] = {
		"pinmode red out",
		"pinmode green out",
		"label loop",
		"write red 1",
		"sleep 100",
		"write red 0",
		"write green 1",
		"sleep 100",
		"write green 0",
		"sleep 100",
		"goto loop",
	};
	uint8_t line;

	memset(&gIdeApp, 0, sizeof(gIdeApp));
	for (line = 0; line < sizeof(default_script) / sizeof(default_script[0]); line++) {
		strcpy(gIdeApp.lines[line], default_script[line]);
	}

	gIdeApp.cursor_visible = true;
	gIdeApp.cursor_blink_ms = IDE_CURSOR_BLINK_MS;
	gIdeApp.dirty = true;
	gIdeApp.viewport_top = 0U;
	IDE_BOARD_ResetScriptPins();
	IDE_APP_SetStatus("ptt run/stop", IDE_STATUS_MS);
}

void IDE_APP_Tick(uint16_t elapsed_ms)
{
	if (gIdeApp.status_timeout_ms != 0U) {
		if (elapsed_ms < gIdeApp.status_timeout_ms) {
			gIdeApp.status_timeout_ms = (uint16_t)(gIdeApp.status_timeout_ms - elapsed_ms);
		} else {
			gIdeApp.status_timeout_ms = 0U;
			gIdeApp.dirty = true;
		}
	}

	if (gIdeApp.pending_active) {
		if (elapsed_ms < gIdeApp.pending_timeout_ms) {
			gIdeApp.pending_timeout_ms = (uint16_t)(gIdeApp.pending_timeout_ms - elapsed_ms);
		} else {
			IDE_APP_FinalizePendingTap();
		}
	}

	if (!gIdeApp.is_running) {
		if (elapsed_ms < gIdeApp.cursor_blink_ms) {
			gIdeApp.cursor_blink_ms = (uint16_t)(gIdeApp.cursor_blink_ms - elapsed_ms);
		} else {
			gIdeApp.cursor_blink_ms = IDE_CURSOR_BLINK_MS;
			gIdeApp.cursor_visible = !gIdeApp.cursor_visible;
			gIdeApp.dirty = true;
		}
	} else {
		mini_result_t result = MINI_VM_Tick(&gIdeApp.vm, &gIdeApp.program, &gIdeHal, elapsed_ms, IDE_VM_BUDGET);

		if (result != MINI_OK) {
			IDE_APP_StopProgram(MINI_ResultString(result));
		} else if (!MINI_VM_IsRunning(&gIdeApp.vm)) {
			IDE_BOARD_ResetScriptPins();
			gIdeApp.is_running = false;
			IDE_APP_SetStatus("done", IDE_STATUS_MS);
		}
	}
}

void IDE_APP_HandleEvent(const ide_input_event_t *event)
{
	if (event->type == IDE_INPUT_EVENT_NONE) {
		return;
	}

	if (gIdeApp.is_running) {
		if (event->type == IDE_INPUT_EVENT_PRESS && event->key == KEY_PTT) {
			IDE_APP_StopProgram("stopped");
		}
		return;
	}

	if (gIdeApp.help_visible) {
		if (event->type == IDE_INPUT_EVENT_LONG && event->key == KEY_MENU) {
			gIdeApp.help_visible = false;
			gIdeApp.dirty = true;
			return;
		}

		if (event->type != IDE_INPUT_EVENT_PRESS) {
			return;
		}

		switch (event->key) {
		case KEY_MENU:
		case KEY_EXIT:
			gIdeApp.help_visible = false;
			gIdeApp.dirty = true;
			return;

		case KEY_UP:
		case KEY_SIDE1:
			if (gIdeApp.help_page > 0U) {
				gIdeApp.help_page--;
				gIdeApp.dirty = true;
			}
			return;

		case KEY_DOWN:
		case KEY_SIDE2:
			if (gIdeApp.help_page + 1U < IDE_HELP_PAGE_COUNT) {
				gIdeApp.help_page++;
				gIdeApp.dirty = true;
			}
			return;

		default:
			return;
		}
	}

	if (event->type == IDE_INPUT_EVENT_LONG) {
		switch (event->key) {
		case KEY_MENU:
			IDE_APP_FinalizePendingTap();
			gIdeApp.help_visible = true;
			gIdeApp.help_page = 0U;
			gIdeApp.dirty = true;
			return;

		case KEY_EXIT:
			IDE_APP_ClearLine();
			return;

		default:
			return;
		}
	}

	switch (event->key) {
	case KEY_0:
	case KEY_1:
	case KEY_2:
	case KEY_3:
	case KEY_4:
	case KEY_5:
	case KEY_6:
	case KEY_7:
	case KEY_8:
	case KEY_9:
	case KEY_STAR:
		IDE_APP_HandleTextKey(event->key);
		break;

	case KEY_MENU:
		IDE_APP_SplitLine();
		break;

	case KEY_PTT:
		IDE_APP_StartProgram();
		break;

	case KEY_EXIT:
		IDE_APP_Backspace();
		break;

	case KEY_UP:
		IDE_APP_MoveCursorVertical(-1);
		break;

	case KEY_DOWN:
		IDE_APP_MoveCursorVertical(1);
		break;

	case KEY_SIDE1:
		IDE_APP_MoveCursorLeft();
		break;

	case KEY_SIDE2:
		IDE_APP_MoveCursorRight();
		break;

	case KEY_F:
		IDE_APP_FinalizePendingTap();
		gIdeApp.uppercase = !gIdeApp.uppercase;
		IDE_APP_SetStatus(gIdeApp.uppercase ? "uppercase" : "lowercase", IDE_STATUS_MS);
		break;

	default:
		break;
	}
}

bool IDE_APP_NeedsRender(void)
{
	return gIdeApp.dirty;
}

void IDE_APP_Render(void)
{
	uint8_t visible;
	char status[17];

	IDE_APP_UpdateViewport();
	IDE_DISPLAY_Clear();

	for (visible = 0; visible < IDE_VISIBLE_LINES; visible++) {
		if (gIdeApp.help_visible) {
			IDE_DISPLAY_DrawTextLine(visible, gIdeHelpPages[gIdeApp.help_page][visible]);
		} else {
			char line_text[17];
			const uint8_t line_index = (uint8_t)(gIdeApp.viewport_top + visible);
			const char *source = gIdeApp.lines[line_index];
			uint8_t window_start = 0U;
			uint8_t cursor_visible_column = 0U;

			IDE_APP_CopyText16(line_text, "");
			if (line_index == gIdeApp.cursor_line) {
				if (gIdeApp.cursor_col >= 16U) {
					window_start = (uint8_t)(gIdeApp.cursor_col - 15U);
				}
				cursor_visible_column = (uint8_t)(gIdeApp.cursor_col - window_start);
			}

			if (window_start < IDE_LINE_CAP) {
				IDE_APP_CopyText16(line_text, source + window_start);
			}

			IDE_DISPLAY_DrawTextLine(visible, line_text);
			if (line_index == gIdeApp.cursor_line && !gIdeApp.is_running) {
				if (gIdeApp.pending_active && gIdeApp.pending_line == gIdeApp.cursor_line) {
					cursor_visible_column = (uint8_t)(gIdeApp.pending_col - window_start);
					if (cursor_visible_column < 16U) {
						IDE_DISPLAY_InvertCell(visible, cursor_visible_column);
					}
				} else if (gIdeApp.cursor_visible && cursor_visible_column < 16U) {
					IDE_DISPLAY_InvertCell(visible, cursor_visible_column);
				}
			}
		}
	}

	if (gIdeApp.status_timeout_ms != 0U) {
		IDE_APP_CopyText16(status, gIdeApp.status_text);
	} else {
		IDE_APP_SetDefaultStatus(status);
	}

	IDE_DISPLAY_DrawTextLine(IDE_STATUS_LINE, status);
	IDE_DISPLAY_Present();
	gIdeApp.dirty = false;
}
