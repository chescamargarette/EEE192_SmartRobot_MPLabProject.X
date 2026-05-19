// This header file contains the initialization of the motor control block.
// NOTE: Do not use, this is not integrated yet to main.c
// For future purposes

#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>
#include <stdbool.h>

// Global Definitions
#define LEFT_MOTOR_TIME    80
#define RIGHT_MOTOR_TIME   10
#define MOTOR_OFF_TIME     85
#define ALT_INTERVAL_MS    1
#define ALT_BOOST          20

// Global Variables
extern int alternate_state;
extern unsigned long last_alternate_time;

// Function Declarations
void enable_motors(void);
void stop_motors(void);
void run_motors_forward(void);
void run_motors_reverse(void);
void turn_motors_left(void);
void turn_motors_right(void);
void delay_motors(void);

#endif // MOTOR_H
