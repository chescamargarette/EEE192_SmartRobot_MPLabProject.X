#include "telemetry.h"
#include <xc.h>

void Telemetry_Init(void) {
    // THE FIX: Route GCLK0 (4MHz Default Clock) to prevent freezing
    GCLK_REGS->GCLK_PCHCTRL[20] = 0x00000040; 
    while ((GCLK_REGS->GCLK_PCHCTRL[20] & (1 << 6)) == 0);

    // Configure PB08 (TX) and PB09 (RX)
    PORT_SEC_REGS->GROUP[1].PORT_PMUX[4] = 0x33; 
    PORT_SEC_REGS->GROUP[1].PORT_PINCFG[8] = 0x41; // TX: PMUXEN + Strong Drive
    PORT_SEC_REGS->GROUP[1].PORT_PINCFG[9] = 0x03; // RX: PMUXEN + Input Enable

    // Reset SERCOM3
    SERCOM3_REGS->USART_INT.SERCOM_CTRLA = 0x01;
    while (SERCOM3_REGS->USART_INT.SERCOM_SYNCBUSY & 0x01);

    // Configure Protocol (8N1)
    SERCOM3_REGS->USART_INT.SERCOM_CTRLA = (1 << 30) | (1 << 20) | (0 << 16) | (0x01 << 2);
    SERCOM3_REGS->USART_INT.SERCOM_CTRLB = (1 << 17) | (1 << 16);

    // Hardcode Baud Rate for 9600
    SERCOM3_REGS->USART_INT.SERCOM_BAUD = 63019; 

    // Enable SERCOM3
    SERCOM3_REGS->USART_INT.SERCOM_CTRLA |= 0x02;
    while (SERCOM3_REGS->USART_INT.SERCOM_SYNCBUSY & 0x02);
}

// Send a single character
void Telemetry_SendChar(char c) {
    while(!(SERCOM3_REGS->USART_INT.SERCOM_INTFLAG & 0x01)); // Wait for DRE
    SERCOM3_REGS->USART_INT.SERCOM_DATA = c;
}

// Send a string
void Telemetry_SendString(const char* str) {
    while (*str) {
        Telemetry_SendChar(*str++);
    }
}

// Check if data is available 
bool Telemetry_IsDataAvailable(void) {
    if (SERCOM3_REGS->USART_INT.SERCOM_INTFLAG & 0x04) {
        return true;
    }
    return false;
}

// Read received character
char Telemetry_ReadChar(void) {
    return (char)SERCOM3_REGS->USART_INT.SERCOM_DATA;
}
