#ifndef __MYI2C_H
#define __MYI2C_H

#include "stm32f10x.h"

/* 软件 I2C：PB10 = SCL，PB11 = SDA，开漏输出 */
void MyI2C_Init(void);
void MyI2C_Start(void);
void MyI2C_Stop(void);
void MyI2C_SendByte(uint8_t Byte);
uint8_t MyI2C_ReceiveByte(void);
void MyI2C_SendAck(uint8_t AckBit);   /* 0 = ACK(应答)，1 = NACK(非应答) */
uint8_t MyI2C_ReceiveAck(void);       /* 返回 0 = 从机应答，1 = 非应答 */

#endif
