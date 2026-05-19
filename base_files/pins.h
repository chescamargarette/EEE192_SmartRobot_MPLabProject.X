// Current version by Chesca

#ifndef PINS_H
#define PINS_H

// Motor Control Block
#define LEFT_MOTOR_REV   8       // PA08 - IN1
#define LEFT_MOTOR_FOR   11      // PA11 - IN2
#define RIGHT_MOTOR_REV  9       // PA09 - IN3
#define RIGHT_MOTOR_FOR  10      // PA10 - IN4
#define LEFT_MOTOR_EN    16      // PA16 - ENA
#define RIGHT_MOTOR_EN   17      // PA17 - ENB

// Line Sensor Block
#define LEFT_LINE_1      20      // PA20 - OUT1
#define LEFT_LINE_2      14      // PA14 - OUT2
#define MID_LINE_1       6       // PA06 - OUT3
#define MID_LINE_2       3       // PA03 - OUT4
#define RIGHT_LINE_1     19      // PA19 - OUT5
#define RIGHT_LINE_2     7       // PA07 - OUT6

// Wall Sensor Block 
#define MID_WALL_TRIG    3       // PB03 - TRIG1
#define MID_WALL_ECHO    2       // PB02 - ECHO1 (has TC2 peripheral)
#define LEFT_WALL_TRIG   21      // PA21 - TRIG2
#define LEFT_WALL_ECHO   22      // PA22 - ECHO2 (has TC0 peripheral)

// LCD Block
#define LCD_SDA          12      // PA12 - SDA (has I2C peripheral)
#define LCD_SCL          13      // PA13 - SCL (has I2C peripheral)

// Bluetooth Block
#define BT_RX            8       // PB08 - RX (has CDC RX)
#define BT_TX            9       // PB09 - TX (has CDC TX)

// Onboard Block
#define ONBOARD_LED      15      // PA15
#define ONBOARD_PB       23      // PA23

#endif // PINS_H
