/* SPDX-License-Identifier: MIT
 * STM32F103C8T6 official limits: Flash 64 KiB, SRAM 20 KiB.
 */
ENTRY(Reset_Handler)
MEMORY {
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
_stack_reserve = 2048;
SECTIONS {
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
    .text : {
        *(.text*) *(.rodata*)
        KEEP(*(.init)) KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH
    .ARM.extab : { *(.ARM.extab*) } > FLASH
    .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*); __exidx_end = .; } > FLASH
    _sidata = LOADADDR(.data);
    .data : {
        . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .;
    } > RAM AT> FLASH
    .bss (NOLOAD) : {
        . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .;
    } > RAM
    .stack_reservation (NOLOAD) : {
        . = ALIGN(8); _end = .; . += _stack_reserve;
    } > RAM
    /DISCARD/ : { *(.note.GNU-stack) *(.comment) }
}
ASSERT(_end + _stack_reserve <= _estack, "RAM does not leave the 2 KiB stack reserve")
