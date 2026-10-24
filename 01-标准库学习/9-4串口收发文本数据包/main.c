#include "stm32f10x.h"
#include "OLED.h"
#include "Serial.h"
#include "LED.h"
#include <string.h>

/* 把第 2 行清空后显示回复内容（8 个空格足够盖住本课的短回复） */
static void ShowReply(char *Reply)
{
	OLED_ShowString(2, 1, "                ");
	OLED_ShowString(2, 1, Reply);
}

/* 只取前 16 个字符显示，补空格盖住上一次更长的内容，避免换行错位 */
static void ShowRxText(void)
{
	char Line[17];
	uint8_t i;

	for (i = 0; i < 16 && Serial_RxPacket[i] != '\0'; i++)
	{
		Line[i] = Serial_RxPacket[i];
	}
	for (; i < 16; i++)
	{
		Line[i] = ' ';
	}
	Line[16] = '\0';

	OLED_ShowString(4, 1, Line);
}

int main(void)
{
	OLED_Init();
	LED_Init();
	Serial_Init();

	OLED_ShowString(1, 1, "ReplyPacket");
	OLED_ShowString(3, 1, "RxPacket");

	while (1)
	{
		if (Serial_GetRxFlag() == 1)      /* 读标志时已自动清零，不用再清一次 */
		{
			ShowRxText();

			if (strcmp(Serial_RxPacket, "LED_ON") == 0)
			{
				LED1_ON();
				Serial_SendString("LED_ON_OK\r\n");
				ShowReply("LED_ON_OK");
			}
			else if (strcmp(Serial_RxPacket, "LED_OFF") == 0)
			{
				LED1_OFF();
				Serial_SendString("LED_OFF_OK\r\n");
				ShowReply("LED_OFF_OK");
			}
			else
			{
				Serial_SendString("ERROR_COMMAND\r\n");
				ShowReply("ERROR_COMMAND");
			}
		}
	}
}
