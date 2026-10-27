#include "stm32f10x.h"
#include "OLED.h"
#include "MPU6050.h"

int16_t AX, AY, AZ, GX, GY, GZ;

int main(void)
{
	uint8_t ID;

	OLED_Init();
	MPU6050_Init();

	/* 先确认芯片在线：WHO_AM_I 正常返回 0x68 */
	ID = MPU6050_GetID();
	OLED_ShowString(1, 1, "ID:");
	OLED_ShowHexNum(1, 4, ID, 2);
	OLED_ShowString(1, 8, "A|G");     /* 左列加速度，右列陀螺仪 */

	if (ID != 0x68)
	{
		OLED_ShowString(3, 1, "MPU6050 ERROR");
		while (1);                     /* 没接好就别继续跑，免得看到一堆假数据 */
	}

	while (1)
	{
		MPU6050_GetData(&AX, &AY, &AZ, &GX, &GY, &GZ);

		OLED_ShowSignedNum(2, 1, AX, 5);
		OLED_ShowSignedNum(3, 1, AY, 5);
		OLED_ShowSignedNum(4, 1, AZ, 5);
		OLED_ShowSignedNum(2, 8, GX, 5);
		OLED_ShowSignedNum(3, 8, GY, 5);
		OLED_ShowSignedNum(4, 8, GZ, 5);
	}
}
