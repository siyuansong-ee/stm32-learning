#include "stm32f10x.h" // Device header
#include "delay.h"
#include "OLED.h"
#include "MPU6050.h"
int16_t AX,AY,AZ,GX,GY,GZ;
int main()
{
	uint8_t ID;
	OLED_Init();
	MPU6050_Init();
//	MPU6050_Init();
//	MOU6050_WriteReg(0X6B,0x00);//解除睡眠
//	MOU6050_WriteReg(0x19,0xAA);
//    uint8_t ID=MPU6050_ReadReg(0x75)
//	OLED_ShowHexNum(1,1,ID,2);
//    MOU6050_WriteReg
	OLED_ShowString(1,1,"ID:");
	ID=MPU6050_GetID();
	OLED_ShowHexNum (1,4,ID,2);
	while(1)
	{
		MPU6050_GetData(&AX,&AY,&AZ,&GX,&GY,&GZ);
		OLED_ShowSignedNum(2,1,AX,5);
		OLED_ShowSignedNum(3,1,AY,5);
		OLED_ShowSignedNum(4,1,AZ,5);
		OLED_ShowSignedNum(2,8,GX,5);
		OLED_ShowSignedNum(3,8,GY,5);
		OLED_ShowSignedNum(4,8,GZ,5);
		
	}
}