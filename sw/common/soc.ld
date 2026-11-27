/* Hazard3 example SoC on Colorlight i5: 64 kB SRAM at 0x0.
   Trap vector at 0x0 (MTVEC_INIT = 0), reset vector at 0x40. */
OUTPUT_ARCH(riscv)
ENTRY(_reset)

MEMORY { RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 64K }

SECTIONS
{
	.text : {
		KEEP(*(.vectors))
		*(.text .text.*)
		*(.rodata .rodata.* .srodata .srodata.*)
	} > RAM
	.data : {
		. = ALIGN(4);
		__global_pointer$ = . + 0x800;
		*(.sdata .sdata.* .data .data.*)
	} > RAM
	.bss (NOLOAD) : {
		. = ALIGN(4);
		__bss_start = .;
		*(.sbss .sbss.* .bss .bss.* COMMON)
		. = ALIGN(4);
		__bss_end = .;
	} > RAM
	__stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
