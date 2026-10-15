#include "stm32f10x.h" // Device header
#include "delay.h"
#include "OLED.h"
#include "PWM.h"
#include "Servo.h"
#include "key.h"
uint8_t KeyNum;
float Angle;
int main()
{
	OLED_Init();
    Servo_Init ();
	Servo_SetAngle(0);
	while(1)
	{
		KeyNum=key_GetNum();
		if(KeyNum==1)
		{
			Angle+=30;
			if(Angle >180)
			{
				Angle=0;
			}
			
		}
		Servo_SetAngle(Angle);
		OLED_ShowNum(1,7,Angle,3);
	}
	
}