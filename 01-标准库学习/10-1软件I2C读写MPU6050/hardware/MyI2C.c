#include "stm32f10x.h"
#include "delay.h"
#include "MyI2C.h"

/* ---------------- 底层引脚操作 ---------------- */

void MyI2C_W_SCL(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOB, GPIO_Pin_10, BitValue ? Bit_SET : Bit_RESET);
	delay_us(10);          /* 等电平稳定，决定 SCL 频率（约 20kHz） */
}

void MyI2C_W_SDA(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOB, GPIO_Pin_11, BitValue ? Bit_SET : Bit_RESET);
	delay_us(10);
}

uint8_t MyI2C_R_SDA(void)
{
	return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11);
}

/* ---------------- 初始化 ---------------- */

void MyI2C_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	/* 开漏输出：既能主动拉低，又能释放（靠外部上拉回到高电平），
	   这是 I2C 总线"线与"的前提 */
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_SetBits(GPIOB, GPIO_Pin_10 | GPIO_Pin_11);   /* 释放总线，空闲态为高 */
}

/* ---------------- 时序 ---------------- */

/* 起始条件：SCL 高电平期间，SDA 由高变低 */
void MyI2C_Start(void)
{
	MyI2C_W_SDA(1);        /* 先确保总线空闲（兼容重复起始条件） */
	MyI2C_W_SCL(1);
	MyI2C_W_SDA(0);
	MyI2C_W_SCL(0);
}

/* 停止条件：SCL 高电平期间，SDA 由低变高 */
void MyI2C_Stop(void)
{
	MyI2C_W_SDA(0);
	MyI2C_W_SCL(1);
	MyI2C_W_SDA(1);
}

/* 发送一个字节，高位在前；SCL 高电平期间从机采样 */
void MyI2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		MyI2C_W_SDA((Byte & (0x80 >> i)) ? 1 : 0);
		MyI2C_W_SCL(1);
		MyI2C_W_SCL(0);
	}
}

/* 接收一个字节：主机先释放 SDA，从机在 SCL 高电平期间给出数据 */
uint8_t MyI2C_ReceiveByte(void)
{
	uint8_t i, Byte = 0x00;

	MyI2C_W_SDA(1);
	for (i = 0; i < 8; i++)
	{
		MyI2C_W_SCL(1);
		if (MyI2C_R_SDA() == 1)
		{
			Byte |= (0x80 >> i);
		}
		MyI2C_W_SCL(0);
	}
	return Byte;
}

/* 主机发送应答：第 9 个时钟，SDA 低 = ACK，高 = NACK */
void MyI2C_SendAck(uint8_t AckBit)
{
	MyI2C_W_SDA(AckBit);
	MyI2C_W_SCL(1);
	MyI2C_W_SCL(0);
}

/* 接收从机的应答：主机释放 SDA，读第 9 个时钟的电平 */
uint8_t MyI2C_ReceiveAck(void)
{
	uint8_t AckBit;

	MyI2C_W_SDA(1);        /* 主机释放 SDA，交给从机 */
	delay_us(1);
	MyI2C_W_SCL(1);
	AckBit = MyI2C_R_SDA();/* 0 = 从机把 SDA 拉低 = 应答 */
	MyI2C_W_SCL(0);
	return AckBit;
}
