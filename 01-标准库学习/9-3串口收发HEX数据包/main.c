#include "stm32f10x.h"
#include "delay.h"
#include "OLED.h"
#include "Serial.h"
#include "key.h"

uint8_t keyNum;

int main(void)
{
	OLED_Init();
	Serial_Init();
	key_Init();

	OLED_ShowString(1, 1, "TxPacket");
	OLED_ShowString(3, 1, "RxPacket");

	Serial_TxPacket[0] = 0x01;
	Serial_TxPacket[1] = 0x02;
	Serial_TxPacket[2] = 0x03;
	Serial_TxPacket[3] = 0x04;

	/* 先把初始 4 字节显示出来，不用等第一次按键 */
	OLED_ShowHexNum(2, 1, Serial_TxPacket[0], 2);
	OLED_ShowHexNum(2, 4, Serial_TxPacket[1], 2);
	OLED_ShowHexNum(2, 7, Serial_TxPacket[2], 2);
	OLED_ShowHexNum(2, 10, Serial_TxPacket[3], 2);

	while (1)
	{
		keyNum = key_GetNum();
		if (keyNum == 1)
		{
			/* 每按一次键，4 字节各 +1 再整包发出 */
			Serial_TxPacket[0]++;
			Serial_TxPacket[1]++;
			Serial_TxPacket[2]++;
			Serial_TxPacket[3]++;

			Serial_SendPacket();

			OLED_ShowHexNum(2, 1, Serial_TxPacket[0], 2);
			OLED_ShowHexNum(2, 4, Serial_TxPacket[1], 2);
			OLED_ShowHexNum(2, 7, Serial_TxPacket[2], 2);
			OLED_ShowHexNum(2, 10, Serial_TxPacket[3], 2);
		}

		/* 收到完整一包（包头包尾都正确）才刷新显示 */
		if (Serial_GetRxFlag() == 1)
		{
			OLED_ShowHexNum(4, 1, Serial_RxPacket[0], 2);
			OLED_ShowHexNum(4, 4, Serial_RxPacket[1], 2);
			OLED_ShowHexNum(4, 7, Serial_RxPacket[2], 2);
			OLED_ShowHexNum(4, 10, Serial_RxPacket[3], 2);
		}
	}
}
