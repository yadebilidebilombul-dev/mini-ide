ENTRY(HandlerReset)

_estack = 0x20004000;

_Min_Heap_Size = 0;
_Min_Stack_Size = 0x80;

MEMORY
{
	FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 58K
	RAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 16K
}

SECTIONS
{
	.text :
	{
		. = ALIGN(4);
		*(.text.isr)
		*(.text)
		*(.text*)
		*(.rodata)
		*(.rodata*)
		*(.glue_7)
		*(.glue_7t)
		*(.eh_frame)
		KEEP(*(.fini))
		. = ALIGN(4);
		_etext = .;
	} >FLASH

	. = ALIGN(4);

	flash_data_start = .;

	.data :
	{
		. = ALIGN(4);
		sram_data_start = .;
		*(.sramtext)
		*(.srambss)
		*(.data)
		*(.data*)
		. = ALIGN(4);
		_edata = .;
	} >RAM AT> FLASH

	sram_data_end = .;

	. = ALIGN(4);
	.bss :
	{
		_sbss = .;
		__bss_start__ = _sbss;
		*(.bss)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
		__bss_end__ = _ebss;
	} >RAM

	._user_heap_stack :
	{
		. = ALIGN(4);
		. = . + _Min_Heap_Size;
		. = . + _Min_Stack_Size;
		. = ALIGN(4);
	} >RAM
}
