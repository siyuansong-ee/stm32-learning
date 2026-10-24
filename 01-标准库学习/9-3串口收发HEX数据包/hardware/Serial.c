#include "stm32f10x.h"
#include "Serial.h"

uint8_t Serial_TxPacket[SERIAL_PACKET_SIZE];
uint8_t Serial_RxPacket[SERIAL_PACKET_SIZE];
volatile uint8_t Serial_RxFlag;

void Serial_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	/* PA9 = USART1_TX，复用推挽输出 */
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* PA10 = USART1_RX，上拉输入：空闲时保持高电平，避免悬空噪声误触发 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 9600-8-N-1，收发都用 */
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART1, &USART_InitStructure);

	/* 接收非空中断（每收到 1 字节进一次中断） */
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	/* NVIC：抢占 1 / 子优先级 1 */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStructure);          /* ★ 漏掉这一行，中断永远进不去 */

	USART_Cmd(USART1, ENABLE);
}

/* 发送 1 字节：写入 TDR 后等 TXE=1，表示 TDR 已腾空、可以装下一字节 */
void Serial_SendByte(uint8_t Byte)
{
	USART_SendData(USART1, Byte);
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}

void Serial_SendArray(uint8_t *Array, uint16_t Length)
{
	uint16_t i;
	for (i = 0; i < Length; i++)
	{
		Serial_SendByte(Array[i]);
	}
}

/* 打包发送：0xFF + 4 字节 + 0xFE */
void Serial_SendPacket(void)
{
	Serial_SendByte(SERIAL_PACKET_HEAD);
	Serial_SendArray(Serial_TxPacket, SERIAL_PACKET_SIZE);
	Serial_SendByte(SERIAL_PACKET_TAIL);
}

/* 读标志并清除：读一次只返回一次 1，主循环不会重复处理同一包 */
uint8_t Serial_GetRxFlag(void)
{
	if (Serial_RxFlag == 1)
	{
		Serial_RxFlag = 0;
		return 1;
	}
	return 0;
}

/* 三状态接收状态机：0=找包头 1=收数据 2=等包尾 */
void USART1_IRQHandler(void)
{
	static uint8_t RxState = 0;
	static uint8_t pRxPacket = 0;

	if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
	{
		uint8_t RxData = USART_ReceiveData(USART1);   /* 读 DR 会自动清 RXNE */

		switch (RxState)
		{
		case 0:                                   /* 等包头 */
			if (RxData == SERIAL_PACKET_HEAD)
			{
				pRxPacket = 0;                    /* ★ 每包都从 0 开始，否则会写越界 */
				RxState = 1;
			}
			break;

		case 1:                                   /* 收 4 字节数据 */
			Serial_RxPacket[pRxPacket] = RxData;
			pRxPacket++;
			if (pRxPacket >= SERIAL_PACKET_SIZE)
			{
				RxState = 2;
			}
			break;

		case 2:                                   /* 等包尾 */
			if (RxData == SERIAL_PACKET_TAIL)
			{
				Serial_RxFlag = 1;
				RxState = 0;
			}
			else if (RxData == SERIAL_PACKET_HEAD)
			{
				/* 包尾丢了：把这个字节当新包头，就地重新同步 */
				pRxPacket = 0;
				RxState = 1;
			}
			else
			{
				/* 既不是包尾也不是包头：整包作废，回状态 0 重新找 */
				RxState = 0;
			}
			break;

		default:
			RxState = 0;
			break;
		}
	}
}
