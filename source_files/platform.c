// This source file contains the initialization of the entire project.
// NOTE: Do not use, this is not yet integrated to main.c
// For future purposes

#include "platform.h"
#include <xc.h>
#include <stdint.h>
#include <limits.h>
 
// Forward declarations (internal helpers)
static void do_raise_perf_level(void);
static void EIC_init_early(void);
static void EIC_init_late(void);
 
#define SYSTICK_RELOAD_VAL 3750 ///< SysTick reload: 156.25 us @ 24 MHz
 
// ==========================================================================
// Time tracking
 
static volatile platform_timespec_t real_time     = PLATFORM_TIMESPEC_ZERO;
static volatile uint32_t            real_time_ctr  = 0;
static volatile uint32_t            ctr_nres       = 0;
 
/* SysTick ISR: fires every 156.25 us; drives USART tick and high-res clock. */
void __attribute__((interrupt)) SysTick_Handler(void)
{
    static int div_n = 0;
 
    // Drive USART tick handler
    platform_timespec_t tick;
    platform_tick_count_2(&tick);
    platform_usart_tick_handler(&tick);
 
    // Increment app-level tick counter (scaled 64x relative to SysTick)
    ++div_n;
    while (div_n >= 64) {
        div_n -= 64;
        ++ctr_nres;
    }
 
    // Advance high-res clock by one tick period
    platform_timespec_t t = real_time;
    t.nr_nsec += (PLATFORM_TICK_PERIOD_US * 1000);
    while (t.nr_nsec >= 1000000000) {
        t.nr_nsec -= 1000000000;
        ++t.nr_sec;
    }
    ++real_time_ctr;
    real_time = t;
    ++real_time_ctr;
 
    SysTick->VAL = 0; // Reset counter
}
 
// ==========================================================================
// Timespec utilities
 
/* Normalize: carry nanosecond overflow into seconds, clamping at UINT32_MAX. */
void platform_timespec_normalize(platform_timespec_t *ts)
{
    while (ts->nr_nsec >= 1000000000) {
        ts->nr_nsec -= 1000000000;
        if (ts->nr_sec < UINT32_MAX) {
            ++ts->nr_sec;
        } else {
            ts->nr_nsec = (1000000000 - 1);
            break;
        }
    }
}
 
/* Compare two timestamps. Returns -1, 0, or +1. */
int platform_timespec_compare(const platform_timespec_t *lhs, const platform_timespec_t *rhs)
{
    if      (lhs->nr_sec  < rhs->nr_sec)  return -1;
    else if (lhs->nr_sec  > rhs->nr_sec)  return +1;
    else if (lhs->nr_nsec < rhs->nr_nsec) return -1;
    else if (lhs->nr_nsec > rhs->nr_nsec) return +1;
    else                                  return  0;
}
 
/* Read real_time atomically (retry if interrupted mid-copy). */
static void platform_tick_count(platform_timespec_t *tick)
{
    uint32_t cookie;
    do {
        cookie = real_time_ctr;
        *tick  = real_time;
    } while (real_time_ctr != cookie);
}
 
/* Get current time at second resolution. */
void platform_tick_count_1(platform_timespec_t *tick)
{
    platform_tick_count(tick);
}
 
/* Get current time at nanosecond resolution (interpolates SysTick remainder). */
void platform_tick_count_2(platform_timespec_t *tick)
{
    platform_timespec_t t;
    uint32_t s = SYSTICK_RELOAD_VAL - SysTick->VAL;
    platform_tick_count(&t);
    t.nr_nsec += (1000 * s) / 12;
    while (t.nr_nsec >= 1000000000) {
        t.nr_nsec -= 1000000000;
        ++t.nr_sec;
    }
    *tick = t;
}
 
/* Compute lhs - rhs, handling unsigned wraparound. */
void platform_tick_delta_2(platform_timespec_t *diff,
                           const platform_timespec_t *lhs,
                           const platform_timespec_t *rhs)
{
    platform_timespec_t d = PLATFORM_TIMESPEC_ZERO;
    uint32_t c = 0;
 
    if (lhs->nr_sec < rhs->nr_sec) {
        d.nr_sec = (UINT32_MAX - rhs->nr_sec) + lhs->nr_sec + 1;
    } else {
        d.nr_sec = lhs->nr_sec - rhs->nr_sec;
    }
 
    if (lhs->nr_nsec < rhs->nr_nsec) {
        c = rhs->nr_nsec - lhs->nr_nsec;
        while (c >= 1000000000) {
            c -= 1000000000;
            if (d.nr_sec == 0) d.nr_sec = UINT32_MAX;
            else               --d.nr_sec;
        }
        if (d.nr_sec == 0) d.nr_sec = UINT32_MAX;
        else               --d.nr_sec;
    } else {
        d.nr_nsec = lhs->nr_nsec - rhs->nr_nsec;
    }
 
    *diff = d;
}
 
// ==========================================================================
// Platform initialization
 
/* Early init: raise CPU clock, reset EIC/EVSYS. Call before anything else. */
void platform_init_early(void)
{
    do_raise_perf_level();
    EIC_init_early();
    EVSYS_SEC_REGS->EVSYS_CTRLA = 0x01;
    asm("nop"); asm("nop"); asm("nop");
}
 
/*
 * Late init: configure SysTick (reload = 3750 → 156.25 us @ 24 MHz),
 * finalize EIC, set IRQ priorities, and enable interrupts.
 */
void platform_init_late(void)
{
    SysTick->LOAD = 3750;
 
    EIC_init_late();
    asm("nop"); asm("nop");
 
    NVIC_SetPriority(SERCOM3_0_IRQn,    3); // usart.c
    NVIC_SetPriority(SERCOM3_2_IRQn,    3); // usart.c
    NVIC_SetPriority(EIC_EXTINT_2_IRQn, 3); // gpio.c
 
    NVIC_EnableIRQ(SERCOM3_0_IRQn);
    NVIC_EnableIRQ(SERCOM3_2_IRQn);
    NVIC_EnableIRQ(EIC_EXTINT_2_IRQn);
    SysTick->CTRL = 0x00000007;
}
 
/* Trigger a system reset via SCB.AIRCR. Never returns. */
void __attribute__((noreturn)) platform_do_sys_reset(void)
{
    uint32_t sc = SCB->AIRCR;
    sc &= ~(0x00000004);
    sc ^=  (0xFFFF0004);
    SCB->AIRCR = sc;
    while (1) { asm("nop"); }
}
 
// ==========================================================================
// SysTick counters
 
/* Returns raw app-level tick count; retries if read races with ISR update. */
uint32_t platform_systick_count(void)
{
    uint32_t res;
    do {
        res = ctr_nres;
        asm("nop");
    } while (res != ctr_nres);
    return res;
}
 
/* Returns lhs - rhs accounting for uint32 wraparound. */
uint32_t platform_tick_delta_1(uint32_t lhs, uint32_t rhs)
{
    if (rhs <= lhs) {
        return lhs - rhs;           // Normal case
    } else {
        uint32_t res = rhs - lhs;
        asm("nop");
        res -= (UINT32_MAX - 1);    // Wraparound case
        return res;
    }
}
 
// ==========================================================================
// Internal: clock and EIC setup
 
/* Switch CPU from 4 MHz (reset default) to 24 MHz via DFLL48M / GCLK_GEN0. */
static void do_raise_perf_level(void)
{
    uint32_t tmp_reg = 0;
 
    // Switch power level to PL2 (required for > 12 MHz operation)
    PM_REGS->PM_INTFLAG = 0x01;
    PM_REGS->PM_PLCFG   = 0x02;
    while ((PM_REGS->PM_INTFLAG & 0x01) == 0) { asm("nop"); }
    PM_REGS->PM_INTFLAG = 0x01;
 
    // Set NVM wait states to 3 (minimum 2 required at PL2 / 24 MHz)
    NVMCTRL_SEC_REGS->NVMCTRL_CTRLB = (3 << 1);
 
    // Power up VDDPLL (STARTUP=2 for 1.1 uF + 20% margin on Curiosity Nano)
    SUPC_REGS->SUPC_VREGPLL = 0x00000202;
    while ((SUPC_REGS->SUPC_STATUS & (1 << 18)) == 0) { asm("nop"); }
 
    // Configure DFLL48M: disable ONDEMAND, write calibration, then enable
    OSCCTRL_REGS->OSCCTRL_DFLLCTRL = 0x0000;
    while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0) { asm("nop"); }
 
    tmp_reg  = SW_CALIB_FUSES_REGS->FUSES_SW_CALIB_WORD_0 & 0x7E000000;
    asm("nop");
    tmp_reg >>= 15;
    tmp_reg  |= ((512 << 0) & 0x000003FF); // FINE value (arbitrary)
    OSCCTRL_REGS->OSCCTRL_DFLLVAL = tmp_reg;
    while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0) { asm("nop"); }
 
    OSCCTRL_REGS->OSCCTRL_DFLLCTRL |= 0x0002;
    while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0) { asm("nop"); }
 
    // Configure GCLK_GEN2 at 4 MHz from OSC16M (peripheral clock source)
    GCLK_REGS->GCLK_GENCTRL[2] = 0x00010105;
    while ((GCLK_REGS->GCLK_SYNCBUSY & (1 << 4)) != 0) { asm("nop"); }
 
    // Switch GCLK_GEN0 to DFLL48M with DIV=2 → 24 MHz CPU clock
    GCLK_REGS->GCLK_GENCTRL[0] = 0x00020107;
    while ((GCLK_REGS->GCLK_SYNCBUSY & (1 << 2)) != 0) { asm("nop"); }
}
 
/* Early EIC setup: enable GCLK2 for EIC and reset the controller. */
static void EIC_init_early(void)
{
    // Select GCLK2 and enable peripheral clock for EIC
    GCLK_REGS->GCLK_PCHCTRL[4] = 0x00000042;
    while ((GCLK_REGS->GCLK_PCHCTRL[4] & (1 << 6)) == 0) { asm("nop"); }
 
    // Reset EIC and wait for sync
    EIC_SEC_REGS->EIC_CTRLA = 0x01;
    while ((EIC_SEC_REGS->EIC_SYNCBUSY & (1 << 0)) != 0) { asm("nop"); }
}
 
/* Late EIC setup: clear flags and enable the controller. */
static void EIC_init_late(void)
{
    EIC_SEC_REGS->EIC_INTFLAG  = 0x8000FFFF;
    EIC_SEC_REGS->EIC_CTRLA   |= 0x02;
    while ((EIC_SEC_REGS->EIC_SYNCBUSY & (1 << 1)) != 0) { asm("nop"); }
}
 
// ==========================================================================
// Event flags
 
static volatile int evt_flags = 0;
 
/* OR ev into the pending event flags (call from IRQ handlers). */
void platform_evt_add(int ev)
{
    evt_flags |= ev;
}
 
/* Return and clear pending event flags (call from main loop). */
int platform_evt_get(void)
{
    int y = evt_flags;
    asm("nop");         // Guard against new events arriving mid-clear
    evt_flags &= ~(y);
    return y;
}
