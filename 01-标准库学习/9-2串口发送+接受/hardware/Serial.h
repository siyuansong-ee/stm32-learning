#ifndef __SERIAL_H
#define __SERIAL_H
#include "stdio.h"
void Serial_Init(void);
void Serial_SendByte(uint8_t Byte);
void Serial_SendArray(uint8_t *Array,uint16_t Length);
void SendString(char *String);
void Serial_Printf(char *format,...);
uint8_t Serial_GetRxFlag(void);
uint8_t Serial_GetRxData(void);
void USART1_IRQHandler(void);


#endif