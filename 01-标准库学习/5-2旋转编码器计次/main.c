#include "stm32f10x.h" // Device header
#include "delay.h"
#include "OLED.h"
#include "Encode.h"
int16_t Num;
int main()
{
	//NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//嵌套向量中断控制器
	OLED_Init();
	Encoder_Init();
	OLED_ShowString (1,1,"Num:");
	while(1)
	{
		Num +=Encoder_Get();
		OLED_ShowSignedNum(1,5,Num,5);
	}
}