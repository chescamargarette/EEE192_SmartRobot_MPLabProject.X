#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <limits.h>

// C99 bool compatibility
#if !defined(__cplusplus) && !defined(__bool_true_false_are_defined)
#define bool  _Bool
#define true  (1)
#define false (0)
#define __bool_true_false_are_defined 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /* Early platform init: boosts CPU to 24 MHz, configures GCLK_GEN2 at 4 MHz,
     * resets EIC & EVSYS, and enables EIC generic-clock input. */
    extern void platform_init_early(void);

    /* Late platform init. Must complete before interrupts are enabled. */
    extern void platform_init_late(void);

    /* Trigger a system reset. Never returns. */
    extern void __attribute__((noreturn)) platform_do_sys_reset(void);

    // ------------------------------------------------------------------

    /// Descriptor for a read-only buffer
    struct platform_ro_buf_desc {
        const char   *buf; ///< Pointer to first byte
        unsigned int  len; ///< Buffer length in bytes
    };

    /// Declare a read-only buffer descriptor from a string literal
#define DECLARE_PLATFORM_RO_STR_DESC(v_name, str) \
    struct platform_ro_buf_desc v_name = {         \
        .buf = (str),                              \
        .len = sizeof(str) - 1                     \
    }

    // ------------------------------------------------------------------

    /* Add event flags (call from IRQ handlers). */
    extern void platform_evt_add(int evt);

    /* Retrieve and clear all pending event flags (call from main loop). */
    extern int platform_evt_get(void);

    /// Event flag: on-board button pressed
#define PLATFORM_EVT_PB_PRESS (0x00000001)

    /// Initialize GPIO for pushbutton and LED
    extern void platform_PB_LED_init(void);

    /// @{ On-board LED control
    extern void platform_LED_onboard_set(void);
    extern void platform_LED_onboard_clear(void);
    extern void platform_LED_onboard_toggle(void);
    /// @}

    // ------------------------------------------------------------------

    /// Returns raw SysTick occurrence count
    extern uint32_t platform_systick_count(void);

    /// Returns tick difference between lhs and rhs, handling overflow
    extern uint32_t platform_tick_delta_1(uint32_t lhs, uint32_t rhs);

    /// Milliseconds per SysTick occurrence
#define PLATFORM_MS_PER_SYSTICK (10)

    // ------------------------------------------------------------------

    /// Main application loop body
    extern void platform_do_loop_one(void);

    /// Pushbutton event flags
#define PLATFORM_PB_ONBOARD_PRESS   0x0001
#define PLATFORM_PB_ONBOARD_RELEASE 0x0002
#define PLATFORM_PB_ONBOARD_MASK    (PLATFORM_PB_ONBOARD_PRESS | PLATFORM_PB_ONBOARD_RELEASE)

    /// Returns pending pushbutton events from the main loop
    extern uint16_t platform_pb_get_event(void);

    /// LED blink modes
#define PLATFORM_BLINK_OFF    (0) ///< Always off
#define PLATFORM_BLINK_SLOW   (1) ///< 200 ms on / 800 ms off
#define PLATFORM_BLINK_MEDIUM (2) ///< 200 ms on / 500 ms off
#define PLATFORM_BLINK_FAST   (3) ///< 200 ms on / 200 ms off
#define PLATFORM_BLINK_ON     (4) ///< Always on

    extern void platform_blink_modify(uint8_t mode);
    extern void platform_set_brightness(uint8_t brightness);

    // ------------------------------------------------------------------

    /// High-resolution timestamp (seconds + nanoseconds)
    typedef struct platform_timespec_type {
        uint32_t nr_sec;  ///< Seconds
        uint32_t nr_nsec; ///< Nanoseconds
    } platform_timespec_t;

#define PLATFORM_TIMESPEC_ZERO  {0, 0} ///< Zero-value timestamp
#define PLATFORM_TICK_PERIOD_US 5000   ///< SysTick period in microseconds

    extern void platform_timespec_normalize(platform_timespec_t *ts);
    extern int  platform_timespec_compare(const platform_timespec_t *lhs, const platform_timespec_t *rhs);
    extern void platform_tick_count_1(platform_timespec_t *tick);  ///< Get current time (seconds resolution)
    extern void platform_tick_count_2(platform_timespec_t *tick);  ///< Get current time (nanosecond resolution)
    extern void platform_tick_delta_2(platform_timespec_t *diff, const platform_timespec_t *lhs, const platform_timespec_t *rhs);

    extern void platform_internal_usart_cdc_systick_handler(void);
    extern void platform_usart_tick_handler(const platform_timespec_t *tick);

#ifdef __cplusplus
}
#endif
#endif /* PLATFORM_H */
