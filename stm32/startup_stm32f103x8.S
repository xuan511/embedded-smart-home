/* SPDX-License-Identifier: MIT
 * Cortex-M3 / STM32F103 medium-density vector order: ST RM0008 section 10.
 * Self-contained startup for GNU Arm assembler; not a USB bootloader image.
 */
.syntax unified
.cpu cortex-m3
.thumb
.section .isr_vector,"a",%progbits
.global vectors
vectors:
    .word _estack
    .word Reset_Handler
    .word Default_Handler /* NMI */
    .word Default_Handler /* HardFault */
    .word Default_Handler /* MemManage */
    .word Default_Handler /* BusFault */
    .word Default_Handler /* UsageFault */
    .word 0,0,0,0
    .word Default_Handler /* SVCall */
    .word Default_Handler /* Debug monitor */
    .word 0
    .word Default_Handler /* PendSV */
    .word SysTick_Handler
    .rept 37 /* WWDG through SPI2 IRQ 36 */
    .word Default_Handler
    .endr
    .word USART1_IRQHandler /* IRQ 37 */
    .rept 22 /* IRQ 38..59, last implemented USBWakeUp is IRQ 42 */
    .word Default_Handler
    .endr
.section .text.Reset_Handler,"ax",%progbits
.thumb_func
.global Reset_Handler
Reset_Handler:
    cpsid i
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:
    cmp r1, r2
    bcs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
2:
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
3:
    cmp r1, r2
    bcs 4f
    str r3, [r1], #4
    b 3b
4:
    bl main
    b .
.section .text.Default_Handler,"ax",%progbits
.thumb_func
Default_Handler:
    b .
.section .note.GNU-stack,"",%progbits
