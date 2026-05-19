// This header file contains the initialization of the motor control block.
// NOTE: Do not use, this is not yet integrated to main.c
// For future purposes

#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>
#include <stdbool.h>

// Global Definitions
#define LEFT_MOTOR_SPEED    80    // Adjust since right motor is faster (L > R)
#define RIGHT_MOTOR_SPEED   10    // Adjust since right motor is faster (L > R)
#define MOTOR_OFF_TIME      85
#define PWM_CYCLE           2000  // 2ms PWM cycle

// Function Declarations
void init_motors(void);
void enable_motors(void);
void delay_motors(void);
void stop_motors(void);
void run_motors_forward(void);
void run_motors_reverse(void);
void turn_motors_left(void);
void turn_motors_right(void);

#endif // MOTOR_H
