#include "stm32f10x.h" // Device header
#include "delay.h"
#include "OLED.h"
#include "motor.h"
#include "key.h"
uint8_t Speed;
uint8_t KeyNum;

int main()
{
	OLED_Init();
    motor_Init ();
	motor_SetSpeed (50);
	while(1)
	{
		KeyNum=key_GetNum ();
		if(KeyNum==1)
		{
			Speed+=20;
			if(Speed>100)
			{
				Speed=-100;
			}
		}
		motor_SetSpeed (Speed);
		OLED_ShowSignedNum(1,7,Speed,3);
	}
	
}