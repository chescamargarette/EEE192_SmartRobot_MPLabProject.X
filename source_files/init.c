/*
 * init.c
 *
 * Initial version:  EEE 158 AY2025-26 Sem 1 Handlers
 * Modifications by: <
 * Franchesca Margarette Visitacion
 * 2022-35114
 * Section WUV
 * >
 *
 * Core platform initialization + SysTick functionality
 */

#include "include.h"

#include "../init.h"

#include <xc.h>
#include <stdint.h>
#include <limits.h>

// Defined elsewhere in this file
static void do_raise_perf_level(void);
static void EIC_init_early(void);
static void EIC_init_late(void);

// Defining global constant for SysTick value
#define SYSTICK_RELOAD_VAL 3750

// ==========================================================================

// Declaring variables for consistent time tracking.
static volatile platform_timespec_t real_time = PLATFORM_TIMESPEC_ZERO;
static volatile uint32_t real_time_ctr = 0;

/*
 * SysTick is an optional facility defined by the ARMv8-M architecture that
 * allows for periodic interrupts, much like the "hours" beep on some watches.
 * For example, with a 4MHz clock SysTick can be configured to trigger periodic
 * interrupts every 50 ms.
 *
 * - PIC32CM LE00/LS00/LS60 implements this facility.
 * - The IRQ name is fixed by the architectural specification.
 *
 * See platform_init_late() for more details on when this IRQ handler is
 * invoked.
 */
static volatile uint32_t ctr_nres = 0;
void __attribute__((interrupt)) SysTick_Handler(void)
{
	static int div_n = 0;

    // Adding high-res time tracking:
	platform_timespec_t tick;
	platform_tick_count_2(&tick);
	platform_usart_tick_handler(&tick);
	// platform_internal_usart_cdc_systick_handler();
    
	++div_n;
	while (div_n >= 64) {
		div_n -= 64;
		++ctr_nres;
	}
    
    // Adding high-res time tracking:
    platform_timespec_t t = real_time;
    t.nr_nsec += (PLATFORM_TICK_PERIOD_US * 1000);
    while (t.nr_nsec >= 1000000000) {
        t.nr_nsec -= 1000000000;
        ++t.nr_sec;
    }
    ++real_time_ctr;
    real_time = t;
    ++real_time_ctr;
    
    SysTick->VAL = 0; // Resets counter
}

// ==========================================================================
// Implementing time tracking/ manipulation functions from init.h.
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
int platform_timespec_compare(const platform_timespec_t *lhs, const platform_timespec_t *rhs)
{
    if (lhs->nr_sec < rhs->nr_sec)
        return -1;
    else if (lhs->nr_sec > rhs->nr_sec)
        return +1;
    else if (lhs->nr_nsec < rhs->nr_nsec)
        return -1;
    else if (lhs->nr_nsec > rhs->nr_nsec)
        return +1;
    else
        return 0;
}
void platform_tick_count(platform_timespec_t *tick)
{
    uint32_t cookie;
    
    do {
        cookie = real_time_ctr;
        *tick = real_time;
    } while (real_time_ctr != cookie);
}
void platform_tick_count_1(platform_timespec_t *tick)
{
    platform_tick_count(tick);
}
void platform_tick_count_2(platform_timespec_t *tick)
{
    platform_timespec_t t;
    uint32_t s = SYSTICK_RELOAD_VAL - SysTick->VAL;
    platform_tick_count(&t);
    t.nr_nsec += (1000 * s)/12;
    while (t.nr_nsec >= 1000000000) {
        t.nr_nsec -= 1000000000;
        ++t.nr_sec;
    }
    *tick = t;
}
void platform_tick_delta_2(platform_timespec_t *diff, const platform_timespec_t *lhs, const platform_timespec_t *rhs)
{
    platform_timespec_t d = PLATFORM_TIMESPEC_ZERO;
    uint32_t c = 0;
    
    if (lhs->nr_sec < rhs->nr_sec) {
        d.nr_sec = (UINT32_MAX - rhs->nr_sec) + lhs->nr_sec + 1;
    } else {
        d.nr_sec = lhs->nr_sec - rhs->nr_sec;
    }
    
    if (lhs->nr_sec < rhs->nr_sec) {
        c = rhs->nr_sec - lhs->nr_sec;
        while (c >= 1000000000) {
            c -= 1000000000;
            if (d.nr_sec == 0) {
                d.nr_sec = UINT32_MAX;
            } else {
                --d.nr_sec;
            }
        }
        if (d.nr_sec == 0) {
            d.nr_sec = UINT32_MAX;
        } else {
            --d.nr_sec;
        }
    } else {
        d.nr_nsec = lhs->nr_nsec - rhs->nr_nsec;
    }
    
    *diff = d;
    return;
}
// ==========================================================================

// Initialize the platform
void platform_init_early(void)
{
    // MUST BE CALLED FIRST
	do_raise_perf_level();
    
    // Other early initialization
	EIC_init_early();
	EVSYS_SEC_REGS->EVSYS_CTRLA = 0x01;
	asm("nop");
	asm("nop");
	asm("nop");
    
    // Enabling of timers, SysTick, and interrupts must be done last.
	return;
}

// Initialize the platform
void platform_init_late(void)
{
    /*
	 * Configure SysTick
	 *
	 * One tick for SysTick corresponds to one CPU clock period; that is,
	 * (after do_raise_perf_level() is called):
	 *
	 * (24 MHz)^(-1) = approx. 41.666667 ns
	 *
	 * The following period values are relevant:
	 * 	- 156.25 us for USART (see usart.c for details)
	 * 	- 10,000 us for application-level tick (arbitrarily [?] chosen)
	 *
	 * SysTick must be configured for the lowest (in this case, USART):
	 * 	156.25 / 0.041666667 = 3750
	 *
	 * All other period values will be scaled relative to the lowest:
	 * 	- Application-level tick: (10,000 / 156.25) = 64
	 */
	SysTick->LOAD = 3750;
    
    // Enabling of timers, SysTick, and interrupts must be done last.
	EIC_init_late();
	asm("nop");
	asm("nop");

	NVIC_SetPriority(SERCOM3_0_IRQn, 3);    // usart.c
	NVIC_SetPriority(SERCOM3_2_IRQn, 3);    // usart.c
	NVIC_SetPriority(EIC_EXTINT_2_IRQn, 3); // gpio.c

	NVIC_EnableIRQ(SERCOM3_0_IRQn);
	NVIC_EnableIRQ(SERCOM3_2_IRQn);
	NVIC_EnableIRQ(EIC_EXTINT_2_IRQn);
	SysTick->CTRL = 0x00000007;
	return;
}

// Trigger a system reset
void __attribute__((noreturn)) platform_do_sys_reset(void)
{
	uint32_t sc = SCB->AIRCR;
    
    /*
	 * Set up the correct value for SCB.AIRCR; this must be written as a
	 * single operation.
	 * 
	 * See the Arm-v8M Architecture Reference Manual for more details.
	 */
	sc &= ~(0x00000004);
	sc ^=  (0xFFFF0004);
	
    /*
	 * Once this statement starts executing, control does not return to
	 * this function.
	 */
	SCB->AIRCR = sc;
    
    /*
	 * Just a formality, but necessary to ensure this function never
	 * returns to its caller.
	 */
	while (1) {
		asm("nop");
	}
}

// ==========================================================================

// Tick count
uint32_t platform_systick_count(void)
{
	uint32_t res;
    
    /*
	 * This loop guards against the possibility of returning inconsistent
	 * data to the caller -- in the unlikely case that the copy was
	 * interrupted, it is retried.
	 */
	do {
		res = ctr_nres;
		asm("nop");
	} while (res != ctr_nres);

	return res;
}

/// Tick difference, aware of wrap-arounds
uint32_t platform_tick_delta_1(uint32_t lhs, uint32_t rhs)
{
	uint32_t res;

	if (rhs <= lhs) {
        // Normal case
		res = lhs - rhs;
	} else {
        // Wrap-around case
		res  = rhs - lhs;
		asm("nop");
		res -= (UINT32_MAX - 1);
	}
	return res;
}

// ==========================================================================

/*
 * Change the CPU clock from its default of 4 MHz at reset, to 24 MHz
 *
 * As this fundamentally affects operation of the microcontroller, call this
 * as early as possible.
 *
 * This is necessary because most peripherals must run slower than the CPU
 * core clock, yet the former needs to run relatively fast in scenarios like
 * PWM/input-capture setups and SERCOM monitoring (cue: sampling theorem).
 */
void do_raise_perf_level(void)
{
	uint32_t tmp_reg = 0;
    
    /*
	 * The chip starts in PL0, which emphasizes energy efficiency over
	 * performance. However, we need the latter for the clock frequency
	 * we will be using (~24 MHz); hence, switch to PL2 before continuing.
	 */
	PM_REGS->PM_INTFLAG = 0x01;
	PM_REGS->PM_PLCFG = 0x02;
	while ((PM_REGS->PM_INTFLAG & 0x01) == 0) {
		asm("nop");
	}
	PM_REGS->PM_INTFLAG = 0x01;
    
    /*
	 * Before we power up the 48MHz DFPLL, we need to ensure that all
	 * electrical characteristics remain respected after the change.
	 *
	 * For the NVM controller (which handles Flash memory, as well as
	 * reads of factory-calibration data; and is clocked at CPU frequency),
	 * the default is to have zero wait states. Unfortunately, at PL2 the
	 * resulting maximum allowable frequency is 11 MHz, which is below our
	 * target CPU frequency of 24 MHz. To rectify this, add enough wait
	 * states; here, 3 is used to provide some margin. (The lowest
	 * allowable in this context is 2.)
	 */
	NVMCTRL_SEC_REGS->NVMCTRL_CTRLB = (3 << 1) ;
    
    /*
	 * Power up the 48MHz DFPLL.
	 *
	 * On the Curiosity Nano Board, VDDPLL has a 1.1uF capacitance
	 * connected in parallel. Assuming a ~20% error, we have
	 * STARTUP >= (1.32uF)/(1uF) = 1.32; as this is not an integer, choose
	 * the next HIGHER value.
	 */
	SUPC_REGS->SUPC_VREGPLL = 0x00000202;
	while ((SUPC_REGS->SUPC_STATUS & (1 << 18)) == 0) {
		asm("nop");
	}
    
    /*
	 * Configure the 48MHz DFPLL.
	 *
	 * Start with disabling ONDEMAND...
	 */
	OSCCTRL_REGS->OSCCTRL_DFLLCTRL = 0x0000;
	while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0) {
		asm("nop");
	}
    
    /*
	 * ... then writing the calibration values (which MUST be done as a
	 * single write, hence the use of a temporary variable)...
	 *
	 * NOTE: The "FINE" value is arbitrary.
	 */
	tmp_reg = SW_CALIB_FUSES_REGS->FUSES_SW_CALIB_WORD_0 & 0x7E000000;
	asm("nop");
	tmp_reg >>= 15;
	tmp_reg |= ((512 << 0) & 0x000003ff);
	OSCCTRL_REGS->OSCCTRL_DFLLVAL = tmp_reg;
	while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0) {
		asm("nop");
	}
    
    /*
	 * ... then enabling.
	 *
	 * Because DFLL48M will be used as the CPU clock source
	 * (via GCLK_GEN0), ONDEMAND cannot be enabled for this oscillator.
	 */
	OSCCTRL_REGS->OSCCTRL_DFLLCTRL |= 0x0002;
	while ((OSCCTRL_REGS->OSCCTRL_STATUS & (1 << 24)) == 0) {
		asm("nop");
	}
    
    /*
	 * Because GCLK_GEN0 will be reconfigured to 24 MHz (the new CPU clock
	 * frequency), a second channel (here, GCLK_GEN2) is configured to
	 * take GCLK_GEN0's place as a peripheral clock source for
	 * slow/medium-speed peripherals.
	 *
	 * NOTE: GCLK_GEN1 is a special instance with additional features. As
	 *       said features (with their attendant complexity) are not
	 *       needed, use a normal instance.
	 *
	 * NOTE: GENCTRL must be written as a single operation; thus, to both
	 *       set and clear bits a read-modify-write operation with a
	 *       temporary variable is required (cue PMUX in PORT).
	 */
	GCLK_REGS->GCLK_GENCTRL[2] = 0x00010105;
	while ((GCLK_REGS->GCLK_SYNCBUSY & (1 << 4)) != 0) {
		asm("nop");
	}
    
    /*
	 * Switch over GCLK_GEN0 from OSC16M to DFLL48M, with DIV=2 to get
	 * 24 MHz. (48 MHz is unnecessarily too fast...)
	 */
	GCLK_REGS->GCLK_GENCTRL[0] = 0x00020107;
	while ((GCLK_REGS->GCLK_SYNCBUSY & (1 << 2)) != 0) {
		asm("nop");
	}
    
    // Done. We're now at 24 MHz.
	return;
}

// Configure EIC here; do not enable it yet
void EIC_init_early(void)
{
    /*
	 * =============================================================
	 * Configure GCLK to enable EIC's filtering + debouncing
	 *
	 * This assignment selects GCLK2, then enables GCLK for EIC.
	 * =============================================================
	 */
	GCLK_REGS->GCLK_PCHCTRL[4] = 0x00000042;
	while ((GCLK_REGS->GCLK_PCHCTRL[4] & (1 << 6)) == 0) {
        // Wait for synchronization
		asm("nop");
	}

    /*
	 * =============================================================
	 * Configure EIC itself. First tasks should be setting the SWRST bit
	 * to 1, then waiting for the reset to complete (cue SYNCBUSY).
	 *
	 * ENABLE should be set last.
	 * =============================================================
	 */
	EIC_SEC_REGS->EIC_CTRLA = 0x01; // Reset
	while ((EIC_SEC_REGS->EIC_SYNCBUSY & (1 << 0)) != 0) {
        // Wait for synchronization
		asm("nop");
	}

    // Done for now
	return;
}

// Late initialization for EIC
void EIC_init_late(void)
{
    // Enable EIC
	EIC_SEC_REGS->EIC_INTFLAG = 0x8000FFFF;
	EIC_SEC_REGS->EIC_CTRLA |= 0x02;
	while ((EIC_SEC_REGS->EIC_SYNCBUSY & (1 << 1)) != 0) {
        // Wait for synchronization
		asm("nop");
	}
	return;
}

// ==========================================================================

// Add an event
static volatile int evt_flags = 0;
void platform_evt_add(int ev)
{
	evt_flags |= ev;
	return;
}

// Get any pending events
int platform_evt_get(void)
{
	int y = evt_flags;

    /*
	 * Since a new event may have come in since the copy to 'y', the latter
	 * is used to clear only the corresponding bit/s.
	 */
	asm("nop");
	evt_flags &= ~(y);
	return y;
}
