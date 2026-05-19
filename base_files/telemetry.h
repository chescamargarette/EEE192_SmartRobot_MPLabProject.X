#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>

void Telemetry_Init(void);
void Telemetry_SendChar(char c);
void Telemetry_SendString(const char* str);
bool Telemetry_IsDataAvailable(void);
char Telemetry_ReadChar(void);

#endif // TELEMETRY_H
