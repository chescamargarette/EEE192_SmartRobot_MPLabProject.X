// This source file contains the execution of the motor control block.
// NOTE: Do not use, this is not yet integrated to main.c
// For future purposes

#include <xc.h>
#include "pins.h"
#include "motor.h"

// Private variables
static int left_on_time;
static int right_on_time;

void init_motors(void) {
    // Set motor pins as outputs
    PORT_SEC_REGS->GROUP[0].PORT_DIRSET = (1 << LEFT_MOTOR_EN) | (1 << RIGHT_MOTOR_EN) | 
                                          (1 << LEFT_MOTOR_FOR) | (1 << LEFT_MOTOR_REV) | 
                                          (1 << RIGHT_MOTOR_FOR) | (1 << RIGHT_MOTOR_REV);
    
    // Calculate PWM on-times based on speed percentages
    left_on_time = (LEFT_MOTOR_SPEED * PWM_CYCLE) / 100;
    right_on_time = (RIGHT_MOTOR_SPEED * PWM_CYCLE) / 100;
}

void enable_motors(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << LEFT_MOTOR_EN) | (1 << RIGHT_MOTOR_EN);
}

void delay_motors(void) {
    for (volatile int i = 0; i < 1000; i++);
}

void run_motors_forward(void) {
    // Turn both motors ON
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << LEFT_MOTOR_FOR) | (1 << RIGHT_MOTOR_FOR);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_REV) | (1 << RIGHT_MOTOR_REV);
    
    // PWM cycle - both motors run simultaneously
    for (int t = 0; t < PWM_CYCLE; t++) {
        if (t == left_on_time) {
            PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR);
        }
        if (t == right_on_time) {
            PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << RIGHT_MOTOR_FOR);
        }
        __asm__ volatile ("nop");
    }
    
    // Ensure both motors are OFF at end of PWM cycle
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR) | (1 << RIGHT_MOTOR_FOR);
    
    // Off time between cycles
    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

void run_motors_reverse(void) {
    // Turn both motors ON in reverse
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << LEFT_MOTOR_REV) | (1 << RIGHT_MOTOR_REV);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR) | (1 << RIGHT_MOTOR_FOR);
    
    // PWM cycle - both motors run simultaneously
    for (int t = 0; t < PWM_CYCLE; t++) {
        if (t == left_on_time) {
            PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_REV);
        }
        if (t == right_on_time) {
            PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << RIGHT_MOTOR_REV);
        }
        __asm__ volatile ("nop");
    }
    
    // Ensure both motors are OFF
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_REV) | (1 << RIGHT_MOTOR_REV);
    
    // Off time between cycles
    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

void turn_motors_left(void) {
    // Pivot left: Left reverse, Right forward — both start simultaneously
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << LEFT_MOTOR_REV) | (1 << RIGHT_MOTOR_FOR);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR) | (1 << RIGHT_MOTOR_REV);
    
    // Both motors run together in one shared PWM loop
    for (int t = 0; t < PWM_CYCLE; t++) {
        if (t == left_on_time) {
            PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_REV);
        }
        if (t == right_on_time) {
            PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << RIGHT_MOTOR_FOR);
        }
        __asm__ volatile ("nop");
    }

    // Ensure both motors are OFF
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_REV) | (1 << RIGHT_MOTOR_FOR);

    // Off time
    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

void turn_motors_right(void) {
    // Pivot right: Left forward, Right reverse — both start simultaneously
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << LEFT_MOTOR_FOR) | (1 << RIGHT_MOTOR_REV);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_REV) | (1 << RIGHT_MOTOR_FOR);
    
    // Both motors run together in one shared PWM loop
    for (int t = 0; t < PWM_CYCLE; t++) {
        if (t == left_on_time) {
            PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR);
        }
        if (t == right_on_time) {
            PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << RIGHT_MOTOR_REV);
        }
        __asm__ volatile ("nop");
    }

    // Ensure both motors are OFF
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR) | (1 << RIGHT_MOTOR_REV);

    // Off time
    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

void stop_motors(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR) | (1 << LEFT_MOTOR_REV) | 
                                          (1 << RIGHT_MOTOR_FOR) | (1 << RIGHT_MOTOR_REV);
}
