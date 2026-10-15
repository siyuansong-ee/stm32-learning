#include "stm32f10x.h" // Device header
#include "delay.h"
#include "LED.h"
#include "Key.h"
uint8_t KeyNum;
int main(void)
{
	LED_Init();
	key_Init();
	while(1)
	{
	  KeyNum=key_GetNum();
		if(KeyNum==1)
		{
			LED1_Turn();
		}
		if(KeyNum==2)
		{
			LED2_Turn();
		}
	}
} 