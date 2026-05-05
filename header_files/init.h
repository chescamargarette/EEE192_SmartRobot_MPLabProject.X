/*
 * @file platform.h (init.h)
 *
 * Initial version:  EEE 158 AY2025-26 Sem 1 Handlers
 * Modifications by: <
 * Franchesca Margarette Visitacion
 * 2022-35114
 * Section WUV
 * >
 *
 * Platform-support functionality, core
 */

/*
 * The outermost #if-#endif block makes sure that if this header file is
 * included multiple times (virtually always the case in production code), we
 * don't get multiple-definition errors from the compiler.
 *
 * (C/C++ Standard, "One-definition Rule" [ODR])
 */
#ifndef EEE158_PLATFORM_INIT_H
#define EEE158_PLATFORM_INIT_H

#include <stdint.h>
#include <limits.h>

// C99 compatibility with C++/C23's ``bool`` datatype
#if !defined(__cplusplus) && !defined(__bool_true_false_are_defined)
#define bool _Bool
#define true (1)
#define false (0)
#define __bool_true_false_are_defined 1
#endif

/*
 * The "extern 'C'" block makes sure that, if this file gets included from
 * C++, name mangling is not performed.
 *
 * (You can search the C++ standard to find out about name mangling.)
 */
#ifdef __cplusplus
extern "C" {
#endif
    
    /*
	 * Perform early platform initialization
	 * 
	 * @details
	 * The initialization phase is split into "early" and "late" parts.
	 * Once the full initialization process is complete, interrupts are
	 * enabled and the code must be ready to process them anytime. For this
	 * reason, the application must initialize all it needs between this
	 * function and platform_init_late().
	 * 
	 * @remarks
	 * Upon return from this function, the platform will have the following
	 * characteristics:
	 *	- CPU clock boosted to 24 MHz;
	 *	- GCLK_GEN2 configured and enabled for 4 MHz from OSC16M;
	 *	- EIC & EVSYS reset; and
	 *	- EIC's generic-clock input enabled.
	 */
    extern void platform_init_early(void);

    /*
	 * Perform late platform initialization
	 * 
	 * @note
	 * See platform_init_early() for the rationale behind this function.
     */
    extern void platform_init_late(void);

    /*
     * Trigger a system reset
     * 
     * @note
     * This function never returns.
     */
    extern void __attribute__((noreturn)) platform_do_sys_reset(void);

    // ==================================================================

    /// Simple descriptor for a constant-content buffer
    struct platform_ro_buf_desc {
		/// First byte in the buffer
		const char *buf;

		/// Number of bytes in the buffer
		unsigned int len;
	};
    
    /**
	 * Declare a buffer containing the given constant C string
	 *
	 * @param v_name	Variable name
	 * @param str		String contents
	 */
#define DECLARE_PLATFORM_RO_STR_DESC(v_name, str)	\
	struct platform_ro_buf_desc v_name = {	\
		.buf = (str),				\
		.len = sizeof(str)-1			\
	}
    
    // ==================================================================

	/*
	 * Add platform-event flags for retrieval via platform_evt_get()
	 * 
	 * This function is intended to be called from IRQ handlers.
	 *
	 * @note
	 * The set of event flags is completely application-defined. When
	 * defining flags, make sure no two flags use the same bit.
     */
    extern void platform_evt_add(int evt);
    
    /*
	 * Get available platform-event flags
	 *
	 * This function is intended to be called from within the main infinite
	 * loop.
	 *
	 * @note
	 * The set of event flags is completely application-defined. In
	 * addition, this function clears all added event flags so that they
	 * will not appear again until re-added via a call to
	 * platform_evt_add().
	 */
    extern int platform_evt_get(void);
    
    /// If set, the on-board button was pressed
#define PLATFORM_EVT_PB_PRESS    (0x00000001)
    
    /// Initialize the GPIO
    extern void platform_PB_LED_init(void);
    
    /*
	 * These set of functions control the on-board LED.
	 *
	 * @{
	 */
    extern void platform_LED_onboard_set(void);
    extern void platform_LED_onboard_clear(void);
    extern void platform_LED_onboard_toggle(void);
    //!<@}

	// ==================================================================

	/// Get the current number of SysTick occurrences, as a raw value
    extern uint32_t platform_systick_count(void);
    
    /// Get the difference between two ticks, considering overflow
    extern uint32_t platform_tick_delta_1(uint32_t lhs, uint32_t rhs);

    /// Number of milliseconds corresponding to one SysTick occurrence
#define PLATFORM_MS_PER_SYSTICK    (10)
    
    
    // ==================================================================

    // Declaring overall loop function for main application logic.
    extern void platform_do_loop_one(void);

    // Defining pushbutton events constants.
#define PLATFORM_PB_ONBOARD_PRESS    0x0001 // Means event = PB pressed
#define PLATFORM_PB_ONBOARD_RELEASE    0x0002 // Means event = PB released
#define PLATFORM_PB_ONBOARD_MASK    (PLATFORM_PB_ONBOARD_PRESS | PLATFORM_PB_ONBOARD_RELEASE) // Means event = combined
    
    // Declaring function for getting PUSHBUTTON events in main loop.
    extern uint16_t platform_pb_get_event(void);
    
    // Defining LED blinking modes, to be used/ implemented elsewhere.
#define PLATFORM_BLINK_OFF    (0) // FULL OFF
#define PLATFORM_BLINK_SLOW    (1) // 200 ON 800 OFF
#define PLATFORM_BLINK_MEDIUM    (2) // 200 ON 500 OFF
#define PLATFORM_BLINK_FAST    (3) // 200 ON 200 ON
#define PLATFORM_BLINK_ON    (4) // FUll ON
    
    // Declaring functions for LED control, to be implemented elsewhere.
    extern void platform_blink_modify(uint8_t mode);
    extern void platform_set_brightness(uint8_t brightness);
    
    // ==================================================================
    
// This section handles accurate SysTick.
    
    // Setting high-resolution time structure..
    typedef struct platform_timespec_type {
        uint32_t    nr_sec; // seconds
        uint32_t    nr_nsec; // nanoseconds
    } platform_timespec_t;

    // Declaring time constants.
#define PLATFORM_TIMESPEC_ZERO {0, 0} // Zero time value
#define PLATFORM_TICK_PERIOD_US    5000 // SysTick in ms
    
    // Declaring time manipulation functions.
    extern void platform_timespec_normalize(platform_timespec_t *ts);
    extern int platform_timespec_compare(const platform_timespec_t *lhs, const platform_timespec_t *rhs); // Compare times
    extern void platform_tick_count_1(platform_timespec_t *tick); // Get current system time (secs)
    extern void platform_tick_count_2(platform_timespec_t *tick); // Get current system time (nano secs)
    extern void platform_tick_delta_2(platform_timespec_t *diff, const platform_timespec_t *lhs, const platform_timespec_t *rhs); // Calc time difference

    // Declaring USART interrupt handler for SysTick
    extern void platform_internal_usart_cdc_systick_handler(void);
    extern void platform_usart_tick_handler(const platform_timespec_t *tick);

#ifdef __cplusplus
}
#endif
#endif
