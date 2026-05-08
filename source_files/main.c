#include <xc.h>
#include <stdbool.h>
#include <stdint.h>
#include "pins.h"


// ====== Clock and Delay ======
#define F_CPU 1000000UL // Change this to match your ACTUAL hardware speed

static inline void delay_us(uint32_t us) {
    uint32_t count = (F_CPU / 1000000UL) * us;
    while (count--) {
        __asm__ volatile ("nop");
    }
}

static inline void delay_ms(uint32_t ms) {
    delay_us(ms * 1000UL);
}

// ====== I2C LCD Configuration ======
#define LCD_I2C_ADDR  0x27    // Try 0x3F if this doesn't work
#define I2C_BAUD      0xE8    // 100 kHz @ 48 MHz

// ====== I2C Functions ======
void I2C_Init(void){
    // Enable GCLK for SERCOM2 (peripheral clock)
    GCLK_REGS->GCLK_PCHCTRL[19] = GCLK_PCHCTRL_GEN(0x0) | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[19] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk);

    // Enable bus clock
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM2_Msk;

    // Configure pins for SERCOM2 (PA08=SDA, PA09=SCL)
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[LCD_SDA] = 0x5U;
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[LCD_SCL] = 0x5U;
    PORT_SEC_REGS->GROUP[0].PORT_PMUX[6]    = 0x22U;

    // Reset SERCOM2
    SERCOM2_REGS->I2CM.SERCOM_CTRLA = SERCOM_I2CM_CTRLA_SWRST_Msk;
    while (SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY);

    // Configure I2C master mode
    SERCOM2_REGS->I2CM.SERCOM_CTRLB = SERCOM_I2CM_CTRLB_SMEN_Msk;
    while (SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY);

    // Set baud rate
    SERCOM2_REGS->I2CM.SERCOM_BAUD = I2C_BAUD;
    
    // Set mode, SDA hold, enable
    SERCOM2_REGS->I2CM.SERCOM_CTRLA = SERCOM_I2CM_CTRLA_MODE_I2C_MASTER 
                                    | SERCOM_I2CM_CTRLA_SDAHOLD_75NS 
                                    | SERCOM_I2CM_CTRLA_SPEED_SM 
                                    | SERCOM_I2CM_CTRLA_ENABLE_Msk;
    while (SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY);

    // Set bus state to IDLE
    SERCOM2_REGS->I2CM.SERCOM_STATUS = SERCOM_I2CM_STATUS_BUSSTATE(1);

    while (SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY);

}

bool I2C_WriteByte(uint8_t addr, uint8_t data) {
    // Wait for bus idle
    while ((SERCOM2_REGS->I2CM.SERCOM_STATUS & SERCOM_I2CM_STATUS_BUSSTATE_Msk) 
           != SERCOM_I2CM_STATUS_BUSSTATE(1));

    // Send address + write bit
    SERCOM2_REGS->I2CM.SERCOM_ADDR = (addr << 1) | 0;
    while (SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY);

    // Wait for address transfer complete
    while (!(SERCOM2_REGS->I2CM.SERCOM_INTFLAG & SERCOM_I2CM_INTFLAG_MB_Msk));

    // Check NAK
    if (SERCOM2_REGS->I2CM.SERCOM_STATUS & SERCOM_I2CM_STATUS_RXNACK_Msk) {
        SERCOM2_REGS->I2CM.SERCOM_CTRLB |= SERCOM_I2CM_CTRLB_CMD(3);
        return false;

    }

    // Clear flag
    SERCOM2_REGS->I2CM.SERCOM_INTFLAG = SERCOM_I2CM_INTFLAG_MB_Msk;

    // Send data byte
    SERCOM2_REGS->I2CM.SERCOM_DATA = data;
    while (SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY);

    // Wait for data transfer complete
    while (!(SERCOM2_REGS->I2CM.SERCOM_INTFLAG & SERCOM_I2CM_INTFLAG_MB_Msk));
    if (SERCOM2_REGS->I2CM.SERCOM_STATUS & SERCOM_I2CM_STATUS_RXNACK_Msk) {
        SERCOM2_REGS->I2CM.SERCOM_CTRLB |= SERCOM_I2CM_CTRLB_CMD(3);
        return false;

    }

    // Generate STOP
    SERCOM2_REGS->I2CM.SERCOM_CTRLB |= SERCOM_I2CM_CTRLB_CMD(3);
    while (SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY);

    return true;

}


// ====== LCD Functions ======
#define LCD_RS         0x01  // P0
#define LCD_EN         0x04  // P2
#define LCD_BACKLIGHT  0x08  // P3

static uint8_t lcd_bk = LCD_BACKLIGHT;

static void lcd_i2c_send(uint8_t data) {
    I2C_WriteByte(LCD_I2C_ADDR, data);

}

static void lcd_strobe(uint8_t data) {
    lcd_i2c_send(data | LCD_EN);
    delay_us(1000);
    lcd_i2c_send(data & ~LCD_EN);
    delay_us(1000);
}

static void lcd_nibble(uint8_t nibble, uint8_t isData) {
    uint8_t data = (nibble & 0x0F) << 4;
    data |= lcd_bk;
    if (isData) data |= LCD_RS;
    lcd_strobe(data);
}

void LCD_Cmd(uint8_t cmd) {
    lcd_nibble(cmd >> 4, 0);
    lcd_nibble(cmd & 0x0F, 0);
    delay_ms(5);
}

void LCD_Data(uint8_t data) {
    lcd_nibble(data >> 4, 1);
    lcd_nibble(data & 0x0F, 1);
    delay_us(100);
}

void LCD_String(const char *str) {
    while (*str) {
        LCD_Data(*str++);
    }
}

void LCD_SetCursor(uint8_t row, uint8_t col) {
    uint8_t row_addr[] = {0x00, 0x40};
    LCD_Cmd(0x80 | (row_addr[row] + col));
}

void LCD_Init(void) {
    I2C_Init();
    delay_ms(50);

    // Reset sequence (8-bit mode commands)
    lcd_nibble(0x03, 0);   delay_ms(10);
    lcd_nibble(0x03, 0);   delay_ms(10);
    lcd_nibble(0x03, 0);   delay_ms(10);
    lcd_nibble(0x02, 0);   delay_ms(10);   // switch to 4-bit

    // Configure LCD
    LCD_Cmd(0x28);        // 2 lines, 5x7 font
    LCD_Cmd(0x0C);        // Display ON, cursor OFF
    LCD_Cmd(0x06);        // Entry mode: increment, no shift
    LCD_Cmd(0x01);        // Clear display
    delay_ms(20);
}

// ====== Original Motor/Sensor Code ======

// Speed Control
#define LEFT_MOTOR_TIME    80
#define RIGHT_MOTOR_TIME   10
#define MOTOR_OFF_TIME     85

#define ALTERNATE_INTERVAL_MS  1
#define ALTERNATE_BOOST        20

#define SENSOR_DEBOUNCE_MS     5
#define STATE_DEBOUNCE_MS      10
#define STOP_DEBOUNCE_MS      100

#define BLINK_DELAY 25000

// Global variables
int alternate_state = 0;
unsigned long last_alternate_time = 0;
unsigned long current_time = 0;

int raw_left1 = 0, raw_left2 = 0, raw_mid1 = 0, raw_mid2 = 0, raw_right1 = 0, raw_right2 = 0;
int debounced_left1 = 0, debounced_left2 = 0, debounced_mid1 = 0, debounced_mid2 = 0, debounced_right1 = 0, debounced_right2 = 0;

unsigned long left1_debounce_time = 0, left2_debounce_time = 0;
unsigned long mid1_debounce_time = 0, mid2_debounce_time = 0;
unsigned long right1_debounce_time = 0, right2_debounce_time = 0;

int left_sensors_active = 0;
int middle_sensors_active = 0;
int right_sensors_active = 0;

unsigned long no_sensors_triggered_time = 0;
int current_state = -1;
int last_state = -1;
unsigned long state_change_time = 0;
int last_valid_state = 0;

void enable_motors(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << EN_LEFT_MOTOR) | (1 << EN_RIGHT_MOTOR);
}

void pause_delay(void) {
    for (volatile int i = 0; i < 1000; i++);
}

void update_debounced_sensors(void) {
    raw_left1 = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << LEFT_LINE_1)) ? 1 : 0;
    raw_left2 = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << LEFT_LINE_2)) ? 1 : 0;
    raw_mid1 = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << MIDDLE_LINE_1)) ? 1 : 0;
    raw_mid2 = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << MIDDLE_LINE_2)) ? 1 : 0;
    raw_right1 = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << RIGHT_LINE_1)) ? 1 : 0;
    raw_right2 = (PORT_SEC_REGS->GROUP[0].PORT_IN & (1 << RIGHT_LINE_2)) ? 1 : 0;

    if (raw_left1 != debounced_left1) {
        if (left1_debounce_time == 0) left1_debounce_time = current_time;
        else if ((current_time - left1_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_left1 = raw_left1;
            left1_debounce_time = 0;
        }
    } else left1_debounce_time = 0;

    if (raw_left2 != debounced_left2) {
        if (left2_debounce_time == 0) left2_debounce_time = current_time;
        else if ((current_time - left2_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_left2 = raw_left2;
            left2_debounce_time = 0;
        }
    } else left2_debounce_time = 0;

    if (raw_mid1 != debounced_mid1) {
        if (mid1_debounce_time == 0) mid1_debounce_time = current_time;
        else if ((current_time - mid1_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_mid1 = raw_mid1;
            mid1_debounce_time = 0;
        }
    } else mid1_debounce_time = 0;

    if (raw_mid2 != debounced_mid2) {
        if (mid2_debounce_time == 0) mid2_debounce_time = current_time;
        else if ((current_time - mid2_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_mid2 = raw_mid2;
            mid2_debounce_time = 0;
        }
    } else mid2_debounce_time = 0;

    if (raw_right1 != debounced_right1) {
        if (right1_debounce_time == 0) right1_debounce_time = current_time;
        else if ((current_time - right1_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_right1 = raw_right1;
            right1_debounce_time = 0;
        }
    } else right1_debounce_time = 0;

    if (raw_right2 != debounced_right2) {
        if (right2_debounce_time == 0) right2_debounce_time = current_time;
        else if ((current_time - right2_debounce_time) >= SENSOR_DEBOUNCE_MS) {
            debounced_right2 = raw_right2;
            right2_debounce_time = 0;
        }
    } else right2_debounce_time = 0;

    left_sensors_active = debounced_left1 || debounced_left2;
    middle_sensors_active = debounced_mid1 || debounced_mid2;
    right_sensors_active = debounced_right1 || debounced_right2;
}

int get_movement_state(void) {
    if (left_sensors_active && middle_sensors_active && right_sensors_active) return 0;
    if (!left_sensors_active && !middle_sensors_active && !right_sensors_active) return 4;
    if (middle_sensors_active) return 0;
    if (left_sensors_active && !right_sensors_active) return 2;
    if (right_sensors_active && !left_sensors_active) return 3;
    if ((debounced_left1 || debounced_left2) && debounced_mid1) return 2;
    if ((debounced_right1 || debounced_right2) && debounced_mid2) return 3;
    return 0;
}

int get_debounced_state(void) {
    int new_state = get_movement_state();
    if (new_state != last_state) {
        if (state_change_time == 0) state_change_time = current_time;
        else if ((current_time - state_change_time) >= STATE_DEBOUNCE_MS) {
            last_state = new_state;
            state_change_time = 0;
        } else return last_state;
    } else state_change_time = 0;
    return new_state;
}

void run_motors_forward_alternating(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << FOR_LEFT_MOTOR) | (1 << FOR_RIGHT_MOTOR);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << REV_LEFT_MOTOR) | (1 << REV_RIGHT_MOTOR);

    int left_time = LEFT_MOTOR_TIME;
    int right_time = RIGHT_MOTOR_TIME;

    if (alternate_state == 1) {
        left_time = LEFT_MOTOR_TIME;
        right_time = RIGHT_MOTOR_TIME + ALTERNATE_BOOST;
    }

    for (volatile int on_ticks = 0; on_ticks < left_time; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << FOR_LEFT_MOTOR);

    for (volatile int on_ticks = 0; on_ticks < right_time; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << FOR_RIGHT_MOTOR); 

    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

void run_motors_pivot_left(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << REV_LEFT_MOTOR) | (1 << FOR_RIGHT_MOTOR);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << FOR_LEFT_MOTOR) | (1 << REV_RIGHT_MOTOR);

    for (volatile int on_ticks = 0; on_ticks < LEFT_MOTOR_TIME; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << REV_LEFT_MOTOR);

    for (volatile int on_ticks = 0; on_ticks < RIGHT_MOTOR_TIME; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << FOR_RIGHT_MOTOR);

    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

void run_motors_pivot_right(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << FOR_LEFT_MOTOR) | (1 << REV_RIGHT_MOTOR);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << REV_LEFT_MOTOR) | (1 << FOR_RIGHT_MOTOR);

    for (volatile int on_ticks = 0; on_ticks < LEFT_MOTOR_TIME; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << FOR_LEFT_MOTOR);

    for (volatile int on_ticks = 0; on_ticks < RIGHT_MOTOR_TIME; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << REV_RIGHT_MOTOR);

    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

void led_on(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << ONBOARD_LED);
}

void led_off(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << ONBOARD_LED);
}

void stop_motors(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << FOR_LEFT_MOTOR) | (1 << REV_LEFT_MOTOR) | 
                                          (1 << FOR_RIGHT_MOTOR) | (1 << REV_RIGHT_MOTOR);
}

// ====== Main ======
int main(void) {
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

    // Initialize LCD
    LCD_Init();
    delay_ms(100);

    // Test message on LCD
    LCD_String("      Mode:");
    LCD_SetCursor(1, 0);
    LCD_String(" Line following");

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
        static int last_lcd_state = -1;
        int current_display_state = should_stop ? 4 : (all_sensors_hi ? 0 : current_state);

        if (current_display_state != last_lcd_state) {
            LCD_String(" Line following");
            LCD_SetCursor(1, 0); // Move to second line
            if (should_stop || current_display_state == 4) {
                LCD_String("    STOPPED     ");
            } else if (all_sensors_hi) {
                LCD_String("  FORWARD (ALL) ");
            } else if (current_display_state == 0) {
                LCD_String("     FORWARD    ");
            } else if (current_display_state == 2) {
                LCD_String("   PIVOT LEFT   ");
            } else if (current_display_state == 3) {
                LCD_String("  PIVOT RIGHT   ");
            } else {
                LCD_String("SEARCHING...    ");
            }
            last_lcd_state = current_display_state;
        }

        // ====== Execute movement based on conditions ======
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
