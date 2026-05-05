#include <xc.h>
#include "pins.h"

// Speed Control - EXTREME left bias (right wheel very slow)
#define LEFT_MOTOR_TIME    80   // Left motor very strong
#define RIGHT_MOTOR_TIME   10   // Right motor MINIMUM power
#define MOTOR_OFF_TIME     85   // OFF time

// Alternating control - 1ms interval (very fast)
#define ALTERNATE_INTERVAL_MS  1   // 1 millisecond between alternations

// Alternating boost - ONLY FOR LEFT WHEEL
#define ALTERNATE_BOOST        20  // Additional boost for left wheel

// Debounce timings
#define SENSOR_DEBOUNCE_MS     5   // 5ms debounce for sensor readings
#define STATE_DEBOUNCE_MS      10  // 10ms debounce for state changes
#define STOP_DEBOUNCE_MS      100  // 100ms delay before stopping (ONLY for ALL LO)

// LED Blink Timing
#define BLINK_DELAY 25000

// Global variables for alternating
int alternate_state = 0;  // 0 = normal, 1 = boost left, 2 = no boost
unsigned long last_alternate_time = 0;
unsigned long current_time = 0;

// Global variables for 6 line sensors (raw and debounced)
int raw_left1 = 0, raw_left2 = 0, raw_mid1 = 0, raw_mid2 = 0, raw_right1 = 0, raw_right2 = 0;
int debounced_left1 = 0, debounced_left2 = 0, debounced_mid1 = 0, debounced_mid2 = 0, debounced_right1 = 0, debounced_right2 = 0;

// Debounce timers for each sensor
unsigned long left1_debounce_time = 0, left2_debounce_time = 0;
unsigned long mid1_debounce_time = 0, mid2_debounce_time = 0;
unsigned long right1_debounce_time = 0, right2_debounce_time = 0;

// Combined sensor states for decision making
int left_sensors_active = 0;    // LEFT_LINE_1 OR LEFT_LINE_2
int middle_sensors_active = 0;  // MIDDLE_LINE_1 OR MIDDLE_LINE_2
int right_sensors_active = 0;   // RIGHT_LINE_1 OR RIGHT_LINE_2

// Global variables for stop debounce (ONLY for ALL LO)
unsigned long no_sensors_triggered_time = 0;

// Global variables for state change debounce
int current_state = -1;  // -1 = unknown, 0 = forward, 2 = pivot left, 3 = pivot right
int last_state = -1;
unsigned long state_change_time = 0;

// Store last valid state for ALL HI condition
int last_valid_state = 0;  // 0 = forward, 2 = pivot left, 3 = pivot right

// Enable motors once at startup
void enable_motors(void)
{
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << EN_LEFT_MOTOR) | (1 << EN_RIGHT_MOTOR);
}

// Standard delay for pauses
void pause_delay(void)
{
    for (volatile int i = 0; i < 1000; i++);
}

// Debounced sensor reading function for 6 line sensors
void update_debounced_sensors(void)
{
    // Read raw sensor values for all 6 sensors
    raw_left1 = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << LEFT_LINE_1)) ? 1 : 0;
    raw_left2 = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << LEFT_LINE_2)) ? 1 : 0;
    raw_mid1 = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << MIDDLE_LINE_1)) ? 1 : 0;
    raw_mid2 = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << MIDDLE_LINE_2)) ? 1 : 0;
    raw_right1 = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << RIGHT_LINE_1)) ? 1 : 0;
    raw_right2 = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << RIGHT_LINE_2)) ? 1 : 0;
    
    // Debounce LEFT_LINE_1
    if (raw_left1 != debounced_left1) {
        if (left1_debounce_time == 0) {
            left1_debounce_time = current_time;
        } else if ((current_time - left1_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_left1 = raw_left1;
            left1_debounce_time = 0;
        }
    } else {
        left1_debounce_time = 0;
    }
    
    // Debounce LEFT_LINE_2
    if (raw_left2 != debounced_left2) {
        if (left2_debounce_time == 0) {
            left2_debounce_time = current_time;
        } else if ((current_time - left2_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_left2 = raw_left2;
            left2_debounce_time = 0;
        }
    } else {
        left2_debounce_time = 0;
    }
    
    // Debounce MIDDLE_LINE_1
    if (raw_mid1 != debounced_mid1) {
        if (mid1_debounce_time == 0) {
            mid1_debounce_time = current_time;
        } else if ((current_time - mid1_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_mid1 = raw_mid1;
            mid1_debounce_time = 0;
        }
    } else {
        mid1_debounce_time = 0;
    }
    
    // Debounce MIDDLE_LINE_2
    if (raw_mid2 != debounced_mid2) {
        if (mid2_debounce_time == 0) {
            mid2_debounce_time = current_time;
        } else if ((current_time - mid2_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_mid2 = raw_mid2;
            mid2_debounce_time = 0;
        }
    } else {
        mid2_debounce_time = 0;
    }
    
    // Debounce RIGHT_LINE_1
    if (raw_right1 != debounced_right1) {
        if (right1_debounce_time == 0) {
            right1_debounce_time = current_time;
        } else if ((current_time - right1_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_right1 = raw_right1;
            right1_debounce_time = 0;
        }
    } else {
        right1_debounce_time = 0;
    }
    
    // Debounce RIGHT_LINE_2
    if (raw_right2 != debounced_right2) {
        if (right2_debounce_time == 0) {
            right2_debounce_time = current_time;
        } else if ((current_time - right2_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_right2 = raw_right2;
            right2_debounce_time = 0;
        }
    } else {
        right2_debounce_time = 0;
    }
    
    // Update combined sensor states
    left_sensors_active = debounced_left1 || debounced_left2;
    middle_sensors_active = debounced_mid1 || debounced_mid2;
    right_sensors_active = debounced_right1 || debounced_right2;
}

// Function to determine movement state with new 6-sensor logic
int get_movement_state(void)
{
    int all_sensors_hi = left_sensors_active && middle_sensors_active && right_sensors_active;
    int all_sensors_lo = !left_sensors_active && !middle_sensors_active && !right_sensors_active;
    
    // Rule 4: ALL sensors are HI -> go forward
    if (all_sensors_hi) {
        return 0;  // FORWARD
    }
    
    // Rule 5: ALL sensors are LO -> stop
    if (all_sensors_lo) {
        return 4;  // STOP
    }
    
    // Rule 1: Middle sensors active -> forward
    if (middle_sensors_active) {
        return 0;  // FORWARD
    }
    
    // Rule 2: Left sensors active -> pivot left
    if (left_sensors_active && !right_sensors_active) {
        return 2;  // PIVOT LEFT
    }
    
    // Rule 3: Right sensors active -> pivot right
    if (right_sensors_active && !left_sensors_active) {
        return 3;  // PIVOT RIGHT
    }
    
    // Rule 6: Left + Middle 1 active -> pivot left
    if ((debounced_left1 || debounced_left2) && debounced_mid1) {
        return 2;  // PIVOT LEFT
    }
    
    // Rule 7: Right + Middle 2 active -> pivot right
    if ((debounced_right1 || debounced_right2) && debounced_mid2) {
        return 3;  // PIVOT RIGHT
    }
    
    // Default: forward
    return 0;
}

// Debounced state change function
int get_debounced_state(void)
{
    int new_state = get_movement_state();
    
    // Debounce state changes (10ms)
    if (new_state != last_state) {
        if (state_change_time == 0) {
            state_change_time = current_time;
        } else if ((current_time - state_change_time) >= STATE_DEBOUNCE_MS) {
            last_state = new_state;
            state_change_time = 0;
        } else {
            // Still in debounce period - return previous state
            return last_state;
        }
    } else {
        state_change_time = 0;
    }
    
    return new_state;
}

// FORWARD movement with alternating
void run_motors_forward_alternating(void)
{
    // Forward direction
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << FOR_LEFT_MOTOR) | (1 << FOR_RIGHT_MOTOR);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << REV_LEFT_MOTOR) | (1 << REV_RIGHT_MOTOR);
    
    // Start with extreme right wheel slowdown
    int left_time = LEFT_MOTOR_TIME;
    int right_time = RIGHT_MOTOR_TIME;
    
    // ONLY boost left wheel
    if (alternate_state == 1) {
        left_time = LEFT_MOTOR_TIME;
        right_time = RIGHT_MOTOR_TIME + ALTERNATE_BOOST;
    }
    
    // Left motor PWM (very strong)
    for (volatile int on_ticks = 0; on_ticks < left_time; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << FOR_LEFT_MOTOR);
    
    // Right motor PWM (very weak)
    for (volatile int on_ticks = 0; on_ticks < right_time; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << FOR_RIGHT_MOTOR);
    
    // Both motors off period
    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

// PIVOT LEFT - reverse left, forward right (with speed bias)
void run_motors_pivot_left(void)
{
    // Pivot left: reverse left, forward right
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << REV_LEFT_MOTOR) | (1 << FOR_RIGHT_MOTOR);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << FOR_LEFT_MOTOR) | (1 << REV_RIGHT_MOTOR);
    
    // Left motor reverse (strong)
    for (volatile int on_ticks = 0; on_ticks < LEFT_MOTOR_TIME; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << REV_LEFT_MOTOR);
    
    // Right motor forward (weak)
    for (volatile int on_ticks = 0; on_ticks < RIGHT_MOTOR_TIME; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << FOR_RIGHT_MOTOR);
    
    // Both motors off period
    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

// PIVOT RIGHT - forward left, reverse right (with speed bias)
void run_motors_pivot_right(void)
{
    // Pivot right: forward left, reverse right
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << FOR_LEFT_MOTOR) | (1 << REV_RIGHT_MOTOR);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << REV_LEFT_MOTOR) | (1 << FOR_RIGHT_MOTOR);
    
    // Left motor forward (strong)
    for (volatile int on_ticks = 0; on_ticks < LEFT_MOTOR_TIME; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << FOR_LEFT_MOTOR);
    
    // Right motor reverse (weak)
    for (volatile int on_ticks = 0; on_ticks < RIGHT_MOTOR_TIME; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << REV_RIGHT_MOTOR);
    
    // Both motors off period
    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

// LED functions
void led_on(void)
{
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << ONBOARD_LED);
}

void led_off(void)
{
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << ONBOARD_LED);
}

void stop_motors(void)
{
    // Only clear direction pins, keep motors enabled
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << FOR_LEFT_MOTOR) | (1 << REV_LEFT_MOTOR) | 
                                          (1 << FOR_RIGHT_MOTOR) | (1 << REV_RIGHT_MOTOR);
}

int main(void)
{
    // Set motor pins as outputs
    PORT_SEC_REGS->GROUP[0].PORT_DIRSET = (1 << EN_LEFT_MOTOR) | (1 << EN_RIGHT_MOTOR) | 
                                          (1 << FOR_LEFT_MOTOR) | (1 << REV_LEFT_MOTOR) | 
                                          (1 << FOR_RIGHT_MOTOR) | (1 << REV_RIGHT_MOTOR);
    
    // Set LED as output
    PORT_SEC_REGS->GROUP[0].PORT_DIRSET = (1 << ONBOARD_LED);
    
    // Set all 6 line sensor pins as inputs
    PORT_SEC_REGS->GROUP[0].PORT_DIRCLR = (1 << LEFT_LINE_1) | (1 << LEFT_LINE_2) | 
                                          (1 << MIDDLE_LINE_1) | (1 << MIDDLE_LINE_2) |
                                          (1 << RIGHT_LINE_1) | (1 << RIGHT_LINE_2);
    
    // Enable input buffers for all 6 sensors
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[LEFT_LINE_1] |= PORT_PINCFG_INEN(1);   
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[LEFT_LINE_2] |= PORT_PINCFG_INEN(1);   
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[MIDDLE_LINE_1] |= PORT_PINCFG_INEN(1);    
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[MIDDLE_LINE_2] |= PORT_PINCFG_INEN(1);    
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[RIGHT_LINE_1] |= PORT_PINCFG_INEN(1);   
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[RIGHT_LINE_2] |= PORT_PINCFG_INEN(1);   
    
    // Disable pull-ups for all 6 sensors
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[LEFT_LINE_1] &= ~PORT_PINCFG_PULLEN_Msk;
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[LEFT_LINE_2] &= ~PORT_PINCFG_PULLEN_Msk;
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[MIDDLE_LINE_1] &= ~PORT_PINCFG_PULLEN_Msk;
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[MIDDLE_LINE_2] &= ~PORT_PINCFG_PULLEN_Msk;
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[RIGHT_LINE_1] &= ~PORT_PINCFG_PULLEN_Msk;
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[RIGHT_LINE_2] &= ~PORT_PINCFG_PULLEN_Msk;
    
    // Turn LED ON by default
    led_on();
    
    // Enable motors once at startup
    enable_motors();
   
    int blink_counter = 0;
    int blink_state = 0;
    
    // Initialize timing
    last_alternate_time = 0;
    current_time = 0;
    
    // Initialize debounce timers
    left1_debounce_time = 0;
    left2_debounce_time = 0;
    mid1_debounce_time = 0;
    mid2_debounce_time = 0;
    right1_debounce_time = 0;
    right2_debounce_time = 0;
    no_sensors_triggered_time = 0;
    state_change_time = 0;
    
    // Initialize states
    last_state = -1;
    current_state = -1;
    last_valid_state = 0;  // Start with forward
    
    while (1)
    {
        current_time++;
        
        // Update debounced sensor readings
        update_debounced_sensors();
        
        // Get combined states
        int all_sensors_hi = left_sensors_active && middle_sensors_active && right_sensors_active;
        int all_sensors_lo = !left_sensors_active && !middle_sensors_active && !right_sensors_active;
        int any_sensor_high = left_sensors_active || middle_sensors_active || right_sensors_active;
        
        int should_stop = 0;
        
        if (all_sensors_hi) {
            // ALL HI - maintain last valid state, reset stop timer
            no_sensors_triggered_time = 0;
            current_state = 0;  // Force forward for ALL HI
        }
        else if (all_sensors_lo) {
            // ALL LO - check 100ms debounce
            if (no_sensors_triggered_time == 0) {
                no_sensors_triggered_time = current_time;
            }
            if ((current_time - no_sensors_triggered_time) >= STOP_DEBOUNCE_MS) {
                should_stop = 1;
            }
        }
        else {
            // Normal sensor readings - reset stop timer and update state
            no_sensors_triggered_time = 0;
            
            // Get debounced movement state (10ms debounce)
            current_state = get_debounced_state();
            
            // Update last valid state (only for movement states, not stop)
            if (current_state == 0 || current_state == 2 || current_state == 3) {
                last_valid_state = current_state;
            }
        }
        
        // LED Control
        if (any_sensor_high && !should_stop && !all_sensors_hi) {
            blink_counter++;
            if (blink_counter >= BLINK_DELAY) {
                blink_counter = 0;
                blink_state = !blink_state;
                if (blink_state) {
                    led_on();
                } else {
                    led_off();
                }
            }
        } else {
            led_on();
            blink_counter = 0;
            blink_state = 0;
        }
        
        // Update alternating state every 1ms (ONLY during forward movement)
        if ((current_time - last_alternate_time) >= ALTERNATE_INTERVAL_MS) {
            last_alternate_time = current_time;
            alternate_state++;
            if (alternate_state > 2) {
                alternate_state = 0;
            }
        }
        
        // Execute movement based on conditions
        if (should_stop) {
            // Stop only after 100ms of ALL LO
            stop_motors();
            pause_delay();
        }
        else if (all_sensors_hi) {
            // ALL HI - go forward
            run_motors_forward_alternating();
        }
        else if (current_state == 0) {  // FORWARD
            run_motors_forward_alternating();
        }
        else if (current_state == 2) {  // PIVOT LEFT
            run_motors_pivot_left();
        }
        else if (current_state == 3) {  // PIVOT RIGHT
            run_motors_pivot_right();
        }
        else if (current_state == 4) {  // STOP (from debounced state)
            stop_motors();
            pause_delay();
        }
        else {
            // Unknown state - continue last valid state
            if (last_valid_state == 0) {
                run_motors_forward_alternating();
            }
            else if (last_valid_state == 2) {
                run_motors_pivot_left();
            }
            else if (last_valid_state == 3) {
                run_motors_pivot_right();
            }
            else {
                run_motors_forward_alternating();
            }
        }
    }
}
