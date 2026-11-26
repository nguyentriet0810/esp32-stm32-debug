/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file main.c
 * @brief STM32F103C8 PC13 LED: 250 ms on / 250 ms off using HSI and SysTick.
 *
 * Standalone target application, separate from the ESP32 probe firmware. PC13 is active-low;
 * the 500 ms nominal period follows the factory-trimmed internal 8 MHz HSI clock.
 */
#include <stdbool.h>
#include <stdint.h>

/* STM32F1 RM0008 and Cortex-M3 PM0056 memory-mapped registers. */
#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define RCC_CR REG32(UINT32_C(0x40021000))
#define RCC_CFGR REG32(UINT32_C(0x40021004))
#define RCC_APB2ENR REG32(UINT32_C(0x40021018))
#define GPIOC_CRH REG32(UINT32_C(0x40011004))
#define GPIOC_BSRR REG32(UINT32_C(0x40011010))
#define SCB_VTOR REG32(UINT32_C(0xE000ED08))
#define SYSTICK_CTRL REG32(UINT32_C(0xE000E010))
#define SYSTICK_LOAD REG32(UINT32_C(0xE000E014))
#define SYSTICK_VAL REG32(UINT32_C(0xE000E018))

static const uint32_t led_mask = UINT32_C(1) << 13U;
static const uint32_t half_period_ms = 250U;

/* Debugger-visible counters. Only the SysTick ISR updates these after initialization. */
volatile uint32_t blink_uptime_ms;
volatile uint32_t blink_edges;
volatile uint32_t blink_led_on;
volatile uint32_t blink_last_edge_ms;
volatile uint32_t blink_ready;
volatile uint32_t blink_error;

/** @brief Wait for a masked clock status with a finite startup polling budget. */
static bool clock_matches(volatile const uint32_t *reg, uint32_t mask, uint32_t expected)
{
    for (uint32_t attempts = 0U; attempts < 100000U; ++attempts)
    {
        if ((*reg & mask) == expected)
        {
            return true;
        }
    }
    return false;
}

/** @brief Select HSI at 8 MHz and undivided AHB/APB clocks, without using HSE/PLL. */
static bool clock_init(void)
{
    RCC_CR |= UINT32_C(1);
    if (!clock_matches(&RCC_CR, UINT32_C(1) << 1U, UINT32_C(1) << 1U))
    {
        return false;
    }
    RCC_CFGR = 0U;
    if (!clock_matches(&RCC_CFGR, UINT32_C(3) << 2U, 0U))
    {
        return false;
    }
    RCC_CR &= ~((UINT32_C(1) << 24U) | (UINT32_C(1) << 19U) | (UINT32_C(1) << 16U));
    return true;
}

/** @brief Handle one millisecond tick; update PC13 every 250 ticks without a busy wait. */
void SysTick_Handler(void);
void SysTick_Handler(void)
{
    static uint32_t elapsed_ms;
    blink_uptime_ms++;
    elapsed_ms++;
    if (elapsed_ms == half_period_ms)
    {
        elapsed_ms = 0U;
        blink_led_on ^= 1U;
        GPIOC_BSRR = (blink_led_on != 0U) ? led_mask << 16U : led_mask;
        blink_edges++;
        blink_last_edge_ms = blink_uptime_ms;
    }
}

/** @brief Configure PC13 and a 1 ms SysTick, then sleep between timer interrupts. */
int main(void);
int main(void)
{
    SYSTICK_CTRL = 0U;
    if (!clock_init())
    {
        blink_error = 1U;
        for (;;)
        {
            __asm__ volatile("wfi");
        }
    }
    SCB_VTOR = UINT32_C(0x08000000);
    RCC_APB2ENR |= UINT32_C(1) << 4U;
    (void)RCC_APB2ENR;
    GPIOC_BSRR = led_mask;
    /* PC13 output, push-pull, 2 MHz. Leave all other pins at their existing settings. */
    GPIOC_CRH = (GPIOC_CRH & ~(UINT32_C(15) << 20U)) | (UINT32_C(2) << 20U);
    SYSTICK_LOAD = 8000U - 1U;
    SYSTICK_VAL = 0U;
    blink_ready = 1U;
    SYSTICK_CTRL = 7U;
    __asm__ volatile("cpsie i" ::: "memory");
    for (;;)
    {
        __asm__ volatile("wfi");
    }
}
