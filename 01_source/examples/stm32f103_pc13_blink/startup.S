/* SPDX-License-Identifier: GPL-3.0-or-later */
/* @file startup.S @brief Cortex-M3 vectors and C data/BSS initialization. */
.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector,"a",%progbits
.balign 256
.global vector_table
vector_table:
    .word _estack
    .word Reset_Handler
    .word Default_Handler /* NMI */
    .word Default_Handler /* HardFault */
    .word Default_Handler /* MemManage */
    .word Default_Handler /* BusFault */
    .word Default_Handler /* UsageFault */
    .word 0, 0, 0, 0
    .word Default_Handler /* SVCall */
    .word Default_Handler /* DebugMon */
    .word 0
    .word Default_Handler /* PendSV */
    .word SysTick_Handler
    .rept 43 /* STM32F103 medium-density IRQ0..IRQ42 */
    .word Default_Handler
    .endr

.section .text.Reset_Handler,"ax",%progbits
.global Reset_Handler
.type Reset_Handler,%function
.thumb_func
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
.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler,"ax",%progbits
.type Default_Handler,%function
.thumb_func
Default_Handler:
    b .
.size Default_Handler, .-Default_Handler
.section .note.GNU-stack,"",%progbits
