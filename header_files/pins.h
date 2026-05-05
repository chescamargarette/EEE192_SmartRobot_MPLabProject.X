#ifndef PINS_H
#define PINS_H

// Motor Pin Definitions
#define EN_LEFT_MOTOR   12      // Enable left motor
#define REV_LEFT_MOTOR  13      // reverse left motor
#define FOR_LEFT_MOTOR  8       // forward left motor
#define EN_RIGHT_MOTOR  10      // Enable right motor
#define REV_RIGHT_MOTOR 11      // reverse right motor
#define FOR_RIGHT_MOTOR 9       // forward right motor

// Line Sensor Pin Definitions 
#define LEFT_LINE_1     3       // PA03 - Left sensor 1
#define LEFT_LINE_2     2       // PA02 - Left sensor 2
#define MIDDLE_LINE_1   3       // PB03 - Middle sensor 1
#define MIDDLE_LINE_2   2       // PB02 - Middle sensor 2
#define RIGHT_LINE_1    23      // PB23 - Right sensor 1
#define RIGHT_LINE_2    19      // PA19 - Right sensor 2

// Wall Sensor Pin Definitions
// Left Wall Sensor
#define LEFT_WALL_TRIG  20      // PA20 - Left wall trigger
#define LEFT_WALL_ECHO  14      // PA14 - Left wall echo

// Middle Wall Sensor
#define MIDDLE_WALL_TRIG 16     // PA16 - Middle wall trigger
#define MIDDLE_WALL_ECHO 17     // PA17 - Middle wall echo

// Right Wall Sensor
#define RIGHT_WALL_TRIG 21      // PA21 - Right wall trigger
#define RIGHT_WALL_ECHO 22      // PA22 - Right wall echo

// Onboard LED Pin Definition
#define ONBOARD_LED     15      // PA15

#endif // PINS_H
