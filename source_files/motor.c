// This source file contains the execution of the motor control block.
// NOTE: Do not use, this is not yet integrated to main.c
// For future purposes

#include <xc.h>
#include "pins.h"
#include "motor.h"

// Declare Variables
static int left_on_time;
static int right_on_time;

// Function Definitions
void init_motors(void) {
    // Set motor pins as outputs
    PORT_SEC_REGS->GROUP[0].PORT_DIRSET = (1 << LEFT_MOTOR_EN) | (1 << RIGHT_MOTOR_EN) | 
                                          (1 << LEFT_MOTOR_FOR) | (1 << LEFT_MOTOR_REV) | 
                                          (1 << RIGHT_MOTOR_FOR) | (1 << RIGHT_MOTOR_REV);
    // Calculate PWM on-times based on speed percentages
    // Scale 0-100 to microseconds within PWM cycle
    left_on_time = (LEFT_MOTOR_SPEED * PWM_CYCLE) / 100;
    right_on_time = (RIGHT_MOTOR_SPEED * PWM_CYCLE) / 100;
}

void enable_motors(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << LEFT_MOTOR_EN) | (1 << RIGHT_MOTOR_EN);
}

void delay_motors(void) {
    for (volatile int i = 0; i < 1000; i++); // Adjust '1000' as needed
}

void stop_motors(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR) | (1 << LEFT_MOTOR_REV) | 
                                          (1 << RIGHT_MOTOR_FOR) | (1 << RIGHT_MOTOR_REV);
}

void run_motors_forward(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << LEFT_MOTOR_FOR) | (1 << RIGHT_MOTOR_FOR);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_REV) | (1 << RIGHT_MOTOR_REV);
    for (int t = 0; t < PWM_CYCLE; t++) {
        if (t == left_on_time) {
            PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR);
        }
        if (t == right_on_time) {
            PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << RIGHT_MOTOR_FOR);
        }
        delay_us(1);  // 1us per iteration
    }
    
    // Ensure both motors are OFF at end of PWM cycle
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR) | (1 << RIGHT_MOTOR_FOR);
    
    // Off time between cycles
    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

void run_motors_reverse(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << LEFT_MOTOR_REV) | (1 << RIGHT_MOTOR_REV);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR) | (1 << RIGHT_MOTOR_FOR);
    for (volatile int on_ticks = 0; on_ticks < LEFT_MOTOR_TIME; on_ticks++);
        PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_REV);
    for (volatile int on_ticks = 0; on_ticks < RIGHT_MOTOR_TIME; on_ticks++);
        PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << RIGHT_MOTOR_REV);
    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

void turn_motors_left(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << LEFT_MOTOR_REV) | (1 << RIGHT_MOTOR_FOR);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR) | (1 << RIGHT_MOTOR_REV);
    for (volatile int on_ticks = 0; on_ticks < LEFT_MOTOR_TIME; on_ticks++);
        PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_REV);
    for (volatile int on_ticks = 0; on_ticks < RIGHT_MOTOR_TIME; on_ticks++);
        PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << RIGHT_MOTOR_FOR);
    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}

void turn_motors_right(void) {
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET = (1 << LEFT_MOTOR_FOR) | (1 << RIGHT_MOTOR_REV);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_REV) | (1 << RIGHT_MOTOR_FOR);
    for (volatile int on_ticks = 0; on_ticks < LEFT_MOTOR_TIME; on_ticks++);
        PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR);
    for (volatile int on_ticks = 0; on_ticks < RIGHT_MOTOR_TIME; on_ticks++);
        PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << RIGHT_MOTOR_REV);
    for (volatile int off_ticks = 0; off_ticks < MOTOR_OFF_TIME; off_ticks++);
}
