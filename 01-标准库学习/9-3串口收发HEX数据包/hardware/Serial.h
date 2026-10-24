#ifndef __SERIAL_H
#define __SERIAL_H

#include "stm32f10x.h"

/* 数据包格式：包头 0xFF + 4 字节数据 + 包尾 0xFE，共 6 字节 */
#define SERIAL_PACKET_HEAD   0xFF
#define SERIAL_PACKET_TAIL   0xFE
#define SERIAL_PACKET_SIZE   4

extern uint8_t Serial_TxPacket[SERIAL_PACKET_SIZE];   /* 待发送的 4 字节数据 */
extern uint8_t Serial_RxPacket[SERIAL_PACKET_SIZE];   /* 已收到的 4 字节数据 */
extern volatile uint8_t Serial_RxFlag;                /* 1 = 收到一包完整数据 */

void Serial_Init(void);
void Serial_SendByte(uint8_t Byte);
void Serial_SendArray(uint8_t *Array, uint16_t Length);
void Serial_SendPacket(void);
uint8_t Serial_GetRxFlag(void);

#endif
