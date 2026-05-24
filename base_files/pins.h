// Current version by Chesca (May 25)

// ============================================================
//  pins.h - PIC32CM5164LS00048 Pin Definitions
// ============================================================
#ifndef PINS_H
#define PINS_H

// ============================================================
//  SECTION 1: Raw Pin Bitmasks
//  Use these for PORT_DIRSET, PORT_OUTSET, PORT_IN masking
//  Always specify Group[0] (PORT A) or Group[1] (PORT B)
// ============================================================

// PORT A
#define PA00  (1UL << 0)
#define PA01  (1UL << 1)
#define PA02  (1UL << 2)
#define PA03  (1UL << 3)
#define PA04  (1UL << 4)
#define PA05  (1UL << 5)
#define PA06  (1UL << 6)
#define PA07  (1UL << 7)
#define PA08  (1UL << 8)
#define PA09  (1UL << 9)
#define PA10  (1UL << 10)
#define PA11  (1UL << 11)
#define PA12  (1UL << 12)
#define PA13  (1UL << 13)
#define PA14  (1UL << 14)
#define PA15  (1UL << 15)
#define PA16  (1UL << 16)
#define PA17  (1UL << 17)
#define PA18  (1UL << 18)
#define PA19  (1UL << 19)
#define PA20  (1UL << 20)
#define PA21  (1UL << 21)
#define PA22  (1UL << 22)
#define PA23  (1UL << 23)
#define PA30  (1UL << 30)

// PORT B
#define PB02  (1UL << 2)
#define PB03  (1UL << 3)
#define PB08  (1UL << 8)
#define PB09  (1UL << 9)
#define PB22  (1UL << 22)
#define PB23  (1UL << 23)

// ============================================================
//  SECTION 2: PORT B Local PINCFG/PMUX Index Literals
//
//  NOTE: PIN_PAxx are already defined by the Microchip DFP header
//  (pic32cm5164ls00048.h) as simple pin numbers 0-30 for PORT A.
//  Do NOT redefine them here.
//
//  IMPORTANT: The DFP defines PORT B pins as GLOBAL pin numbers
//  (e.g. PIN_PB08 = 40, not 8). For GROUP[1].PORT_PINCFG[] and
//  GROUP[1].PORT_PMUX[] indexing we need the LOCAL pin number
//  within GROUP[1]. Use PINCFG_PBxx for those operations only.
// ============================================================
#define PINCFG_PB02   2
#define PINCFG_PB03   3
#define PINCFG_PB08   8
#define PINCFG_PB09   9
#define PINCFG_PB22  22
#define PINCFG_PB23  23

// ============================================================
//  SECTION 3: Peripheral Function Mux Values
//  PIC32CM LS00: A=0, B=1, C=2, D=3, E=4, ...
// ============================================================
#define MUX_A  0
#define MUX_B  1
#define MUX_C  2
#define MUX_D  3
#define MUX_E  4
#define MUX_G  6
#define MUX_H  7

// ============================================================
//  SECTION 4: Pin Assignments
// ============================================================

// --- Motor Control Block (PORT A) ---
#define LEFT_MOTOR_EN       PA04    // enA
#define LEFT_MOTOR_BACK     PA05    // IN1
#define LEFT_MOTOR_FOR      PA08    // IN2
#define RIGHT_MOTOR_BACK    PA09    // IN3
#define RIGHT_MOTOR_FOR     PA16    // IN4
#define RIGHT_MOTOR_EN      PA17    // enB

// Pin numbers for motor PINCFG[] use (DFP PIN_PAxx = local PORT A index)
#define PINCFG_LEFT_MOTOR_EN       PIN_PA04
#define PINCFG_LEFT_MOTOR_BACK     PIN_PA05
#define PINCFG_LEFT_MOTOR_FOR      PIN_PA08
#define PINCFG_RIGHT_MOTOR_BACK    PIN_PA09
#define PINCFG_RIGHT_MOTOR_FOR     PIN_PA16
#define PINCFG_RIGHT_MOTOR_EN      PIN_PA17

// --- Line Sensor Block (PORT A) ---
#define LEFT_LINE_1     PA20    // Left   sensor 1
#define LEFT_LINE_2     PA14    // Left   sensor 2
#define MID_LINE_1      PA06    // Middle sensor 1
#define MID_LINE_2      PA03    // Middle sensor 2
#define RIGHT_LINE_1    PA19    // Right  sensor 1
#define RIGHT_LINE_2    PA07    // Right  sensor 2

// Pin numbers for line sensor PINCFG[] use (DFP PIN_PAxx = local PORT A index)
#define PINCFG_LEFT_LINE_1     PIN_PA20
#define PINCFG_LEFT_LINE_2     PIN_PA14
#define PINCFG_MID_LINE_1      PIN_PA06
#define PINCFG_MID_LINE_2      PIN_PA03
#define PINCFG_RIGHT_LINE_1    PIN_PA19
#define PINCFG_RIGHT_LINE_2    PIN_PA07

// --- Wall Sensor Block (PORT A) ---
#define FRONT_WALL_TRIG     PA10    // Output
#define FRONT_WALL_ECHO     PA11    // Input  (3.3V safe via voltage divider)
#define LEFT_WALL_TRIG      PA00    // Output
#define LEFT_WALL_ECHO      PA01    // Input  (3.3V safe via voltage divider)

// --- Bluetooth Block - SERCOM4, PORT B (Phase 4) ---
//     Currently repurposed as Debug UART via SERCOM3
#define BT_RX   PB08    // MCU RX <- HC-05 TX
#define BT_TX   PB09    // MCU TX -> HC-05 RX

// --- Debug UART Block - SERCOM3, PORT B ---
//     Onboard debugger CDC bridge: PB08 = CDC RX (MCU TX), PB09 = CDC TX (MCU RX)
//     PB08 = SERCOM3/PAD[0] = MCU TX (peripheral function D)
//     No external adapter needed - appears as virtual COM port over USB debug cable
//     Baud: 9600, 8N1 @ 24MHz GCLK0  ->  BAUD register = 65117
#define DEBUG_UART_TX           PB08
#define PINCFG_DEBUG_UART_TX    PINCFG_PB08     // Local GROUP[1] index = 8
#define PMUX_DEBUG_UART_TX      (PINCFG_PB08 >> 1)  // PMUX index = 4
#define DEBUG_SERCOM            SERCOM3_REGS
#define DEBUG_BAUD_9600         65117UL         // 9600 baud @ 24MHz GCLK0

// --- LCD I2C Block - SERCOM2, PORT A ---
#define LCD_SDA     PA12    // SDA
#define LCD_SCL     PA13    // SCL

// --- Buzzer (PORT B) ---
#define BUZZER      PB23    // Digital or PWM tone output

// --- Onboard (PORT A) ---
#define ONBOARD_LED     PA15
#define ONBOARD_PB      PA23

#endif // PINS_H
