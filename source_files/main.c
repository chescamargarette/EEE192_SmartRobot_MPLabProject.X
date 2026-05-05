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

// Global variables for sensor debouncing
int raw_left = 0, raw_mid = 0, raw_right = 0;
int debounced_left = 0, debounced_mid = 0, debounced_right = 0;
unsigned long left_debounce_time = 0, mid_debounce_time = 0, right_debounce_time = 0;

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

// Debounced sensor reading function
void update_debounced_sensors(void)
{
    // Read raw sensor values
    raw_left = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << LEFT_SENSOR)) ? 1 : 0;
    raw_mid = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << MID_SENSOR)) ? 1 : 0;
    raw_right = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << RIGHT_SENSOR)) ? 1 : 0;
    
    // Debounce LEFT sensor
    if (raw_left != debounced_left) {
        if (left_debounce_time == 0) {
            left_debounce_time = current_time;
        } else if ((current_time - left_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_left = raw_left;
            left_debounce_time = 0;
        }
    } else {
        left_debounce_time = 0;
    }
    
    // Debounce MID sensor
    if (raw_mid != debounced_mid) {
        if (mid_debounce_time == 0) {
            mid_debounce_time = current_time;
        } else if ((current_time - mid_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_mid = raw_mid;
            mid_debounce_time = 0;
        }
    } else {
        mid_debounce_time = 0;
    }
    
    // Debounce RIGHT sensor
    if (raw_right != debounced_right) {
        if (right_debounce_time == 0) {
            right_debounce_time = current_time;
        } else if ((current_time - right_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_right = raw_right;
            right_debounce_time = 0;
        }
    } else {
        right_debounce_time = 0;
    }
}

// Function to determine movement state with 10ms debounce
// NEW LOGIC: left AND right HIGH but center LOW = FORWARD (treat as position glitch)
int get_debounced_state(int left, int mid, int right)
{
    int new_state = -1;
    
    // NEW: If left AND right are HIGH, but center is LOW -> go FORWARD
    // This handles the robot being positioned between lines or sensor glitch
    if (left && right && !mid) {
        new_state = 0;  // FORWARD (treat as centered)
    }
    // Determine new state based on debounced sensors
    else if (mid && !left && !right) {
        new_state = 0;  // FORWARD
    }
    else if (left && mid && !right) {
        new_state = 2;  // PIVOT LEFT (from slightly left)
    }
    else if (!left && mid && right) {
        new_state = 3;  // PIVOT RIGHT (from slightly right)
    }
    else if (left && !mid && !right) {
        new_state = 2;  // PIVOT LEFT (hard left)
    }
    else if (!left && !mid && right) {
        new_state = 3;  // PIVOT RIGHT (hard right)
    }
    else {
        new_state = 4;  // STOP (for ALL LO after debounce)
    }
    
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
    
    // Set line sensor pins as inputs
    PORT_SEC_REGS->GROUP[0].PORT_DIRCLR = (1 << LEFT_SENSOR) | (1 << MID_SENSOR) | (1 << RIGHT_SENSOR);
    
    // Enable input buffers for sensors
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[LEFT_SENSOR] |= PORT_PINCFG_INEN(1);   
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[MID_SENSOR] |= PORT_PINCFG_INEN(1);    
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[RIGHT_SENSOR] |= PORT_PINCFG_INEN(1);  
    
    // Disable pull-ups
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[LEFT_SENSOR] &= ~PORT_PINCFG_PULLEN_Msk;
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[MID_SENSOR] &= ~PORT_PINCFG_PULLEN_Msk;
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[RIGHT_SENSOR] &= ~PORT_PINCFG_PULLEN_Msk;
    
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
    left_debounce_time = 0;
    mid_debounce_time = 0;
    right_debounce_time = 0;
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
        
        // Check for ALL HI (no debounce, immediate)
        int all_sensors_hi = debounced_left && debounced_mid && debounced_right;
        
        // Check for left AND right HIGH but center LOW (treat as forward, not stop)
        int left_right_only = debounced_left && debounced_right && !debounced_mid;
        
        // Check for ALL LO (with 100ms debounce)
        int all_sensors_lo = !debounced_left && !debounced_mid && !debounced_right;
        
        int should_stop = 0;
        
        if (all_sensors_hi) {
            // ALL HI - maintain last valid state, reset stop timer
            no_sensors_triggered_time = 0;
            // Keep using last_valid_state (no state change)
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
            // The get_debounced_state function now handles left+right=forward
            current_state = get_debounced_state(debounced_left, debounced_mid, debounced_right);
            
            // Update last valid state (only for movement states, not stop)
            if (current_state == 0 || current_state == 2 || current_state == 3) {
                last_valid_state = current_state;
            }
        }
        
        int any_sensor_high = debounced_left || debounced_mid || debounced_right;
        
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
        
        if (current_state == 0) {  // FORWARD
            run_motors_forward_alternating();
        // Execute movement based on conditions
        }
        else if (should_stop) {
            // Stop only after 100ms of ALL LO
            stop_motors();
            pause_delay();
        }
        else if (all_sensors_hi) {
            // Forward direction                          
            // ALL HI - continue last valid state (no debounce, immediate)
            
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
        else if (left_right_only) {
            // NEW: Left and Right HIGH but Center LOW -> go straight forward
            // This handles position glitches where robot is between lines
            run_motors_forward_alternating();
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
