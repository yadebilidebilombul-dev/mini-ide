/* Minimal script engine scaffold for a future UV-K5 mini-IDE firmware. */

#include "mini_script.h"

#include <string.h>

#define MINI_LINE_CAP 64U
#define MINI_TOKEN_CAP 16U
#define MINI_MAX_TOKENS 6U
typedef struct {
	char items[MINI_MAX_TOKENS][MINI_TOKEN_CAP];
	uint8_t count;
} mini_tokens_t;

static void MINI_SetError(char *error_text, size_t error_text_size, const char *message)
{
	size_t i;

	if (error_text == 0 || error_text_size == 0U) {
		return;
	}

	for (i = 0; i + 1U < error_text_size && message[i] != 0; i++) {
		error_text[i] = message[i];
	}
	error_text[i] = 0;
}

static char MINI_ToUpper(char c)
{
	if (c >= 'a' && c <= 'z') {
		return (char)(c - ('a' - 'A'));
	}
	return c;
}

static bool MINI_IsSpace(char c)
{
	return c == ' ' || c == '\t';
}

static bool MINI_StringEquals(const char *a, const char *b)
{
	while (*a != 0 && *b != 0) {
		if (MINI_ToUpper(*a) != MINI_ToUpper(*b)) {
			return false;
		}
		a++;
		b++;
	}

	return *a == 0 && *b == 0;
}

static const char *MINI_ReadLine(const char *source, char *line, size_t line_size)
{
	size_t length = 0U;

	if (source == 0 || *source == 0) {
		if (line_size != 0U) {
			line[0] = 0;
		}
		return 0;
	}

	while (*source != 0 && *source != '\n') {
		if (*source != '\r' && length + 1U < line_size) {
			line[length++] = *source;
		}
		source++;
	}

	if (*source == '\n') {
		source++;
	}

	line[length] = 0;
	return source;
}

static void MINI_Trim(char *line)
{
	size_t start = 0U;
	size_t end = strlen(line);
	size_t i;

	while (line[start] != 0 && MINI_IsSpace(line[start])) {
		start++;
	}
	while (end > start && MINI_IsSpace(line[end - 1U])) {
		end--;
	}
	if (start == 0U && line[end] == 0) {
		line[end] = 0;
		return;
	}

	for (i = 0; start + i < end; i++) {
		line[i] = line[start + i];
	}
	line[i] = 0;
}

static void MINI_Tokenize(const char *line, mini_tokens_t *tokens)
{
	size_t index = 0U;

	tokens->count = 0U;

	while (*line != 0) {
		size_t out = 0U;

		while (MINI_IsSpace(*line)) {
			line++;
		}
		if (*line == 0 || *line == '#') {
			return;
		}
		if (tokens->count >= MINI_MAX_TOKENS) {
			return;
		}

		while (*line != 0 && !MINI_IsSpace(*line) && *line != '#') {
			if (out + 1U < MINI_TOKEN_CAP) {
				tokens->items[tokens->count][out++] = *line;
			}
			line++;
		}
		tokens->items[tokens->count][out] = 0;
		tokens->count++;

		while (*line != 0 && *line != '#' && MINI_IsSpace(*line)) {
			line++;
		}

		index++;
		if (index >= MINI_MAX_TOKENS) {
			return;
		}
	}
}

static bool MINI_ParseUnsigned16(const char *text, uint16_t *value)
{
	uint32_t acc = 0U;

	if (text == 0 || *text == 0) {
		return false;
	}

	while (*text != 0) {
		if (*text < '0' || *text > '9') {
			return false;
		}
		acc = (acc * 10U) + (uint32_t)(*text - '0');
		if (acc > 65535U) {
			return false;
		}
		text++;
	}

	*value = (uint16_t)acc;
	return true;
}

static bool MINI_ParseBit(const char *text, uint8_t *value)
{
	uint16_t parsed;

	if (!MINI_ParseUnsigned16(text, &parsed) || parsed > 1U) {
		return false;
	}

	*value = (uint8_t)parsed;
	return true;
}

static bool MINI_PinIsSupported(const mini_hal_t *hal, mini_pin_t pin)
{
	if (hal->is_supported_pin == 0) {
		return true;
	}

	return hal->is_supported_pin(pin);
}

mini_result_t MINI_ParsePin(const char *text, mini_pin_t *pin)
{
	uint16_t number;
	char port;

	if (text == 0 || pin == 0) {
		return MINI_ERR_BAD_ARGUMENT;
	}
	if (text[0] != 'P' && text[0] != 'p') {
		return MINI_ERR_UNKNOWN_PIN;
	}

	port = MINI_ToUpper(text[1]);
	if (port < 'A' || port > 'C') {
		return MINI_ERR_UNKNOWN_PIN;
	}
	if (!MINI_ParseUnsigned16(text + 2, &number) || number > 15U) {
		return MINI_ERR_UNKNOWN_PIN;
	}

	*pin = (mini_pin_t)((((uint8_t)(port - 'A')) & 0x03U) << 5);
	*pin |= (mini_pin_t)(number & 0x1FU);
	return MINI_OK;
}

char MINI_PinPort(mini_pin_t pin)
{
	return (char)('A' + ((pin >> 5) & 0x03U));
}

uint8_t MINI_PinNumber(mini_pin_t pin)
{
	return (uint8_t)(pin & 0x1FU);
}

static int16_t MINI_FindLabel(const mini_program_t *program, const char *name)
{
	uint8_t i;

	for (i = 0; i < program->label_count; i++) {
		if (MINI_StringEquals(program->labels[i].name, name)) {
			return (int16_t)program->labels[i].pc;
		}
	}

	return -1;
}

static mini_result_t MINI_AddLabel(mini_program_t *program, const char *name)
{
	size_t i;

	if (program->label_count >= MINI_MAX_LABELS) {
		return MINI_ERR_TOO_MANY_LABELS;
	}
	if (MINI_FindLabel(program, name) >= 0) {
		return MINI_OK;
	}

	for (i = 0; i + 1U < sizeof(program->labels[0].name) && name[i] != 0; i++) {
		program->labels[program->label_count].name[i] = name[i];
	}
	program->labels[program->label_count].name[i] = 0;
	program->labels[program->label_count].pc = program->instruction_count;
	program->label_count++;
	return MINI_OK;
}

static mini_result_t MINI_FirstPassLine(mini_program_t *program, const mini_tokens_t *tokens)
{
	if (tokens->count == 0U) {
		return MINI_OK;
	}
	if (MINI_StringEquals(tokens->items[0], "label")) {
		if (tokens->count != 2U) {
			return MINI_ERR_SYNTAX;
		}
		return MINI_AddLabel(program, tokens->items[1]);
	}
	if (program->instruction_count >= MINI_MAX_INSTRUCTIONS) {
		return MINI_ERR_TOO_MANY_INSTRUCTIONS;
	}
	program->instruction_count++;
	return MINI_OK;
}

static mini_result_t MINI_EmitInstruction(mini_program_t *program, const mini_instruction_t *instruction)
{
	if (program->instruction_count >= MINI_MAX_INSTRUCTIONS) {
		return MINI_ERR_TOO_MANY_INSTRUCTIONS;
	}

	program->instructions[program->instruction_count] = *instruction;
	program->instruction_count++;
	return MINI_OK;
}

static mini_result_t MINI_CompileLine(mini_program_t *program, const mini_tokens_t *tokens)
{
	mini_instruction_t instruction;
	mini_result_t result;
	mini_pin_t pin;
	uint16_t value16;
	uint8_t bit;
	int16_t label_pc;

	if (tokens->count == 0U) {
		return MINI_OK;
	}
	if (MINI_StringEquals(tokens->items[0], "label")) {
		return MINI_OK;
	}

	memset(&instruction, 0, sizeof(instruction));

	if (MINI_StringEquals(tokens->items[0], "pinmode")) {
		if (tokens->count != 3U) {
			return MINI_ERR_SYNTAX;
		}
		result = MINI_ParsePin(tokens->items[1], &pin);
		if (result != MINI_OK) {
			return result;
		}
		instruction.arg0 = pin;
		if (MINI_StringEquals(tokens->items[2], "in")) {
			instruction.opcode = MINI_OP_PINMODE;
			instruction.arg1 = MINI_PIN_MODE_IN;
			return MINI_EmitInstruction(program, &instruction);
		}
		if (MINI_StringEquals(tokens->items[2], "out")) {
			instruction.opcode = MINI_OP_PINMODE;
			instruction.arg1 = MINI_PIN_MODE_OUT;
			return MINI_EmitInstruction(program, &instruction);
		}
		return MINI_ERR_SYNTAX;
	}

	if (MINI_StringEquals(tokens->items[0], "write")) {
		if (tokens->count != 3U) {
			return MINI_ERR_SYNTAX;
		}
		result = MINI_ParsePin(tokens->items[1], &pin);
		if (result != MINI_OK) {
			return result;
		}
		instruction.arg0 = pin;
		if (!MINI_ParseBit(tokens->items[2], &bit)) {
			return MINI_ERR_BAD_ARGUMENT;
		}
		instruction.opcode = MINI_OP_WRITE;
		instruction.arg1 = bit;
		return MINI_EmitInstruction(program, &instruction);
	}

	if (MINI_StringEquals(tokens->items[0], "toggle")) {
		if (tokens->count != 2U) {
			return MINI_ERR_SYNTAX;
		}
		result = MINI_ParsePin(tokens->items[1], &pin);
		if (result != MINI_OK) {
			return result;
		}
		instruction.arg0 = pin;
		instruction.opcode = MINI_OP_TOGGLE;
		return MINI_EmitInstruction(program, &instruction);
	}

	if (MINI_StringEquals(tokens->items[0], "sleep")) {
		if (tokens->count != 2U) {
			return MINI_ERR_SYNTAX;
		}
		if (!MINI_ParseUnsigned16(tokens->items[1], &value16)) {
			return MINI_ERR_BAD_ARGUMENT;
		}
		instruction.opcode = MINI_OP_SLEEP;
		instruction.arg2 = value16;
		return MINI_EmitInstruction(program, &instruction);
	}

	if (MINI_StringEquals(tokens->items[0], "wait")) {
		if (tokens->count != 3U) {
			return MINI_ERR_SYNTAX;
		}
		result = MINI_ParsePin(tokens->items[1], &pin);
		if (result != MINI_OK) {
			return result;
		}
		instruction.arg0 = pin;
		if (!MINI_ParseBit(tokens->items[2], &bit)) {
			return MINI_ERR_BAD_ARGUMENT;
		}
		instruction.opcode = MINI_OP_WAIT;
		instruction.arg1 = bit;
		return MINI_EmitInstruction(program, &instruction);
	}

	if (MINI_StringEquals(tokens->items[0], "beep")) {
		if (tokens->count != 3U) {
			return MINI_ERR_SYNTAX;
		}
		if (!MINI_ParseUnsigned16(tokens->items[1], &value16)) {
			return MINI_ERR_BAD_ARGUMENT;
		}
		instruction.opcode = MINI_OP_BEEP;
		instruction.arg0 = value16;
		if (!MINI_ParseUnsigned16(tokens->items[2], &value16)) {
			return MINI_ERR_BAD_ARGUMENT;
		}
		instruction.arg1 = value16;
		return MINI_EmitInstruction(program, &instruction);
	}

	if (MINI_StringEquals(tokens->items[0], "goto")) {
		if (tokens->count != 2U) {
			return MINI_ERR_SYNTAX;
		}
		label_pc = MINI_FindLabel(program, tokens->items[1]);
		if (label_pc < 0) {
			return MINI_ERR_UNKNOWN_LABEL;
		}
		instruction.opcode = MINI_OP_GOTO;
		instruction.arg2 = (uint16_t)label_pc;
		return MINI_EmitInstruction(program, &instruction);
	}

	if (MINI_StringEquals(tokens->items[0], "if")) {
		if (tokens->count != 5U || !MINI_StringEquals(tokens->items[3], "goto")) {
			return MINI_ERR_SYNTAX;
		}
		result = MINI_ParsePin(tokens->items[1], &pin);
		if (result != MINI_OK) {
			return result;
		}
		instruction.arg0 = pin;
		if (!MINI_ParseBit(tokens->items[2], &bit)) {
			return MINI_ERR_BAD_ARGUMENT;
		}
		label_pc = MINI_FindLabel(program, tokens->items[4]);
		if (label_pc < 0) {
			return MINI_ERR_UNKNOWN_LABEL;
		}
		instruction.opcode = MINI_OP_IF_GOTO;
		instruction.arg1 = bit;
		instruction.arg2 = (uint16_t)label_pc;
		return MINI_EmitInstruction(program, &instruction);
	}

	if (MINI_StringEquals(tokens->items[0], "end")) {
		if (tokens->count != 1U) {
			return MINI_ERR_SYNTAX;
		}
		instruction.opcode = MINI_OP_END;
		return MINI_EmitInstruction(program, &instruction);
	}

	return MINI_ERR_UNKNOWN_COMMAND;
}

mini_result_t MINI_Compile(const char *source, mini_program_t *program, char *error_text, size_t error_text_size)
{
	const char *cursor;
	char line[MINI_LINE_CAP];
	mini_tokens_t tokens;
	mini_result_t result;

	if (source == 0 || program == 0) {
		MINI_SetError(error_text, error_text_size, "bad argument");
		return MINI_ERR_BAD_ARGUMENT;
	}

	memset(program, 0, sizeof(*program));

	cursor = source;
	while ((cursor = MINI_ReadLine(cursor, line, sizeof(line))) != 0) {
		MINI_Trim(line);
		MINI_Tokenize(line, &tokens);
		result = MINI_FirstPassLine(program, &tokens);
		if (result != MINI_OK) {
			MINI_SetError(error_text, error_text_size, MINI_ResultString(result));
			return result;
		}
	}

	program->instruction_count = 0U;

	cursor = source;
	while ((cursor = MINI_ReadLine(cursor, line, sizeof(line))) != 0) {
		MINI_Trim(line);
		MINI_Tokenize(line, &tokens);
		result = MINI_CompileLine(program, &tokens);
		if (result != MINI_OK) {
			MINI_SetError(error_text, error_text_size, MINI_ResultString(result));
			return result;
		}
	}

	MINI_SetError(error_text, error_text_size, "ok");
	return MINI_OK;
}

void MINI_VM_Init(mini_vm_t *vm)
{
	if (vm == 0) {
		return;
	}

	memset(vm, 0, sizeof(*vm));
	vm->last_result = MINI_OK;
}

mini_result_t MINI_VM_Start(mini_vm_t *vm, const mini_program_t *program)
{
	if (vm == 0 || program == 0) {
		return MINI_ERR_BAD_ARGUMENT;
	}

	MINI_VM_Init(vm);
	vm->is_running = true;
	vm->last_result = MINI_OK;
	return MINI_OK;
}

void MINI_VM_Stop(mini_vm_t *vm)
{
	if (vm == 0) {
		return;
	}

	vm->is_running = false;
	vm->is_waiting = false;
	vm->sleep_remaining_ms = 0U;
}

bool MINI_VM_IsRunning(const mini_vm_t *vm)
{
	return vm != 0 && vm->is_running;
}

mini_result_t MINI_VM_Tick(mini_vm_t *vm, const mini_program_t *program, const mini_hal_t *hal, uint16_t elapsed_ms, uint16_t instruction_budget)
{
	if (vm == 0 || program == 0 || hal == 0) {
		return MINI_ERR_BAD_ARGUMENT;
	}
	if (hal->pin_mode == 0 || hal->digital_write == 0 || hal->digital_read == 0 || hal->beep_start == 0 || hal->beep_stop == 0 || hal->sleep_ms == 0) {
		vm->last_result = MINI_ERR_HAL_MISSING;
		vm->is_running = false;
		return vm->last_result;
	}
	if (!vm->is_running) {
		return vm->last_result;
	}

	if (vm->sleep_remaining_ms != 0U) {
		if (elapsed_ms < vm->sleep_remaining_ms) {
			vm->sleep_remaining_ms -= elapsed_ms;
			return MINI_OK;
		}

		elapsed_ms = (uint16_t)(elapsed_ms - vm->sleep_remaining_ms);
		vm->sleep_remaining_ms = 0U;
		if (vm->tone_active) {
			hal->beep_stop();
			vm->tone_active = false;
		}
	}

	if (vm->is_waiting) {
		if ((uint8_t)hal->digital_read(vm->wait_pin) != vm->wait_value) {
			return MINI_OK;
		}
		vm->is_waiting = false;
	}

	while (vm->pc < program->instruction_count && instruction_budget != 0U) {
		const mini_instruction_t *instruction = &program->instructions[vm->pc];

		instruction_budget--;

		switch (instruction->opcode) {
		case MINI_OP_PINMODE:
			if (!MINI_PinIsSupported(hal, instruction->arg0)) {
				vm->last_result = MINI_ERR_UNSUPPORTED_PIN;
				vm->is_running = false;
				return vm->last_result;
			}
			hal->pin_mode(instruction->arg0, (mini_pin_mode_t)instruction->arg1);
			vm->pc++;
			break;

		case MINI_OP_WRITE:
			if (!MINI_PinIsSupported(hal, instruction->arg0)) {
				vm->last_result = MINI_ERR_UNSUPPORTED_PIN;
				vm->is_running = false;
				return vm->last_result;
			}
			hal->digital_write(instruction->arg0, instruction->arg1 != 0U);
			vm->pc++;
			break;

		case MINI_OP_TOGGLE:
			if (!MINI_PinIsSupported(hal, instruction->arg0)) {
				vm->last_result = MINI_ERR_UNSUPPORTED_PIN;
				vm->is_running = false;
				return vm->last_result;
			}
			hal->digital_write(instruction->arg0, !hal->digital_read(instruction->arg0));
			vm->pc++;
			break;

		case MINI_OP_SLEEP:
			if (instruction->arg2 > elapsed_ms) {
				vm->sleep_remaining_ms = (uint16_t)(instruction->arg2 - elapsed_ms);
				vm->pc++;
				return MINI_OK;
			}
			elapsed_ms = (uint16_t)(elapsed_ms - instruction->arg2);
			vm->pc++;
			break;

		case MINI_OP_WAIT:
			if (!MINI_PinIsSupported(hal, instruction->arg0)) {
				vm->last_result = MINI_ERR_UNSUPPORTED_PIN;
				vm->is_running = false;
				return vm->last_result;
			}
			if ((uint8_t)hal->digital_read(instruction->arg0) != instruction->arg1) {
				vm->wait_pin = instruction->arg0;
				vm->wait_value = instruction->arg1;
				vm->is_waiting = true;
				vm->pc++;
				return MINI_OK;
			}
			vm->pc++;
			break;

		case MINI_OP_BEEP:
			hal->beep_start(instruction->arg0);
			vm->tone_active = true;
			vm->sleep_remaining_ms = instruction->arg1;
			vm->pc++;
			return MINI_OK;

		case MINI_OP_GOTO:
			vm->pc = instruction->arg2;
			continue;

		case MINI_OP_IF_GOTO:
			if ((uint8_t)hal->digital_read(instruction->arg0) == instruction->arg1) {
				vm->pc = instruction->arg2;
				continue;
			}
			vm->pc++;
			break;

		case MINI_OP_END:
			vm->is_running = false;
			vm->last_result = MINI_OK;
			return MINI_OK;

		default:
			vm->is_running = false;
			vm->last_result = MINI_ERR_UNKNOWN_COMMAND;
			return vm->last_result;
		}
	}

	if (vm->pc >= program->instruction_count) {
		vm->is_running = false;
		vm->last_result = MINI_OK;
	}

	return MINI_OK;
}

mini_result_t MINI_Run(const mini_program_t *program, const mini_hal_t *hal, uint16_t max_steps)
{
	mini_vm_t vm;
	uint16_t steps = 0U;

	if (program == 0 || hal == 0) {
		return MINI_ERR_BAD_ARGUMENT;
	}
	if (hal->pin_mode == 0 || hal->digital_write == 0 || hal->digital_read == 0 || hal->beep_start == 0 || hal->beep_stop == 0 || hal->sleep_ms == 0) {
		return MINI_ERR_HAL_MISSING;
	}

	MINI_VM_Start(&vm, program);
	while (MINI_VM_IsRunning(&vm)) {
		mini_result_t result;

		if (steps++ >= max_steps) {
			return MINI_ERR_TOO_MANY_STEPS;
		}

		result = MINI_VM_Tick(&vm, program, hal, 1U, 1U);
		if (result != MINI_OK) {
			return result;
		}
		if (vm.sleep_remaining_ms != 0U) {
			hal->sleep_ms(vm.sleep_remaining_ms);
			vm.sleep_remaining_ms = 0U;
			if (vm.tone_active) {
				hal->beep_stop();
				vm.tone_active = false;
			}
		}
	}

	return vm.last_result;
}

const char *MINI_ResultString(mini_result_t result)
{
	switch (result) {
	case MINI_OK:
		return "ok";
	case MINI_ERR_BAD_ARGUMENT:
		return "bad argument";
	case MINI_ERR_SYNTAX:
		return "syntax error";
	case MINI_ERR_UNKNOWN_COMMAND:
		return "unknown command";
	case MINI_ERR_UNKNOWN_PIN:
		return "unknown pin";
	case MINI_ERR_UNSUPPORTED_PIN:
		return "unsupported pin";
	case MINI_ERR_UNKNOWN_LABEL:
		return "unknown label";
	case MINI_ERR_TOO_MANY_INSTRUCTIONS:
		return "too many instructions";
	case MINI_ERR_TOO_MANY_LABELS:
		return "too many labels";
	case MINI_ERR_TOO_MANY_STEPS:
		return "too many steps";
	case MINI_ERR_HAL_MISSING:
		return "hal missing";
	default:
		return "unknown error";
	}
}
