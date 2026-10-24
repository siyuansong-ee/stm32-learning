#ifndef __SERIAL_H
#define __SERIAL_H

#include "stm32f10x.h"

/* 文本帧格式：'@' + 正文 + "\r\n"，正文最长 99 字符（留 1 字节放 '\0'） */
#define SERIAL_RX_BUFFER_SIZE   100

extern char Serial_RxPacket[SERIAL_RX_BUFFER_SIZE];   /* 收到的一行文本，已补 '\0' */
extern volatile uint8_t Serial_RxFlag;                /* 1 = 收到一行完整文本 */

void Serial_Init(void);
void Serial_SendByte(uint8_t Byte);
void Serial_SendArray(uint8_t *Array, uint16_t Length);
void Serial_SendString(char *String);
uint8_t Serial_GetRxFlag(void);

#endif
