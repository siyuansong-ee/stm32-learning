#include "stm32f10x.h" // Device header
#include "delay.h"
#include "OLED.h"
#include "Timer.h"
#include "Encode.h"
int16_t Speed;
int main()//旋钮模拟测速
{
	OLED_Init();
	Timer_Init();
	Encoder_Init();
	OLED_ShowString(1,1,"Speed:");
	while(1)
	{
		OLED_ShowSignedNum(1,7,Encoder_Get(),5);
		delay_ms(1000);
		
		
	}
}
void TIM2_IRQHandler(void)
{
	if(TIm_GetITStatus(TIM2,TIM_IT_Update )==SET)
	{
		Speed=Encoder_Get();
		TIM_ClearITPendingBit (TIM2,TIM_IT_Update);
	}
}