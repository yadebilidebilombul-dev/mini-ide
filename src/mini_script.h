/* Minimal script engine scaffold for a future UV-K5 mini-IDE firmware. */

#ifndef CODEX_MINI_IDE_MINI_SCRIPT_H
#define CODEX_MINI_IDE_MINI_SCRIPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MINI_MAX_INSTRUCTIONS 64U
#define MINI_MAX_LABELS 16U

typedef enum {
	MINI_OK = 0,
	MINI_ERR_BAD_ARGUMENT,
	MINI_ERR_SYNTAX,
	MINI_ERR_UNKNOWN_COMMAND,
	MINI_ERR_UNKNOWN_PIN,
	MINI_ERR_UNSUPPORTED_PIN,
	MINI_ERR_UNKNOWN_LABEL,
	MINI_ERR_TOO_MANY_INSTRUCTIONS,
	MINI_ERR_TOO_MANY_LABELS,
	MINI_ERR_TOO_MANY_STEPS,
	MINI_ERR_HAL_MISSING,
} mini_result_t;

typedef enum {
	MINI_PIN_MODE_IN = 0,
	MINI_PIN_MODE_OUT = 1,
} mini_pin_mode_t;

typedef enum {
	MINI_OP_PINMODE = 0,
	MINI_OP_WRITE,
	MINI_OP_TOGGLE,
	MINI_OP_SLEEP,
	MINI_OP_WAIT,
	MINI_OP_GOTO,
	MINI_OP_IF_GOTO,
	MINI_OP_END,
} mini_opcode_t;

typedef uint8_t mini_pin_t;

#define MINI_PIN_RED    ((mini_pin_t)0xE0U)
#define MINI_PIN_GREEN  ((mini_pin_t)0xE1U)

typedef struct {
	mini_opcode_t opcode;
	uint8_t arg0;
	uint8_t arg1;
	uint16_t arg2;
} mini_instruction_t;

typedef struct {
	char name[12];
	uint8_t pc;
} mini_label_t;

typedef struct {
	mini_instruction_t instructions[MINI_MAX_INSTRUCTIONS];
	mini_label_t labels[MINI_MAX_LABELS];
	uint8_t instruction_count;
	uint8_t label_count;
} mini_program_t;

typedef struct {
	bool (*is_supported_pin)(mini_pin_t pin);
	void (*pin_mode)(mini_pin_t pin, mini_pin_mode_t mode);
	void (*digital_write)(mini_pin_t pin, bool value);
	bool (*digital_read)(mini_pin_t pin);
	void (*sleep_ms)(uint16_t value);
} mini_hal_t;

typedef struct {
	uint16_t pc;
	uint16_t sleep_remaining_ms;
	mini_pin_t wait_pin;
	uint8_t wait_value;
	bool is_running;
	bool is_waiting;
	mini_result_t last_result;
} mini_vm_t;

mini_result_t MINI_Compile(const char *source, mini_program_t *program, char *error_text, size_t error_text_size);
mini_result_t MINI_Run(const mini_program_t *program, const mini_hal_t *hal, uint16_t max_steps);
void MINI_VM_Init(mini_vm_t *vm);
mini_result_t MINI_VM_Start(mini_vm_t *vm, const mini_program_t *program);
mini_result_t MINI_VM_Tick(mini_vm_t *vm, const mini_program_t *program, const mini_hal_t *hal, uint16_t elapsed_ms, uint16_t instruction_budget);
void MINI_VM_Stop(mini_vm_t *vm);
bool MINI_VM_IsRunning(const mini_vm_t *vm);

mini_result_t MINI_ParsePin(const char *text, mini_pin_t *pin);
char MINI_PinPort(mini_pin_t pin);
uint8_t MINI_PinNumber(mini_pin_t pin);
const char *MINI_ResultString(mini_result_t result);

#endif
