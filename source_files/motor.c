// This source file contains the execution of the motor control block.
// NOTE: Do not use, this is not yet integrated to main.c
// For future purposes

#include <xc.h>
#include "pins.h"
#include "motor.h"

// Initialize Global Variables
int alternate_state = 0;
unsigned long last_alternate_time = 0;

// Function Definitions

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
    int left_time = LEFT_MOTOR_TIME;
    int right_time = RIGHT_MOTOR_TIME;
    if (alternate_state == 1) {
        left_time = LEFT_MOTOR_TIME;
        right_time = RIGHT_MOTOR_TIME + ALT_BOOST;
    }
    for (volatile int on_ticks = 0; on_ticks < left_time; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << LEFT_MOTOR_FOR);
    for (volatile int on_ticks = 0; on_ticks < right_time; on_ticks++);
    PORT_SEC_REGS->GROUP[0].PORT_OUTCLR = (1 << RIGHT_MOTOR_FOR); 
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
