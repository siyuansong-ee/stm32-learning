#include "stm32f10x.h"                  // Device header
extern uint16_t Num;
void Timer_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE );
	TIM_InternalClockConfig (TIM2);//内部时钟
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period=10000-1;//ARR
	TIM_TimeBaseInitStructure.TIM_Prescaler=7200-1;//PSC
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter=0;
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseInitStructure);
	TIM_ClearFlag(TIM2,TIM_FLAG_Update);//清除事件标志位 防止上电误触发
	TIM_ITConfig (TIM2,TIM_IT_Update,ENABLE);//定时器内部的中断总闸  向 NVIC 提交中断请求
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel=TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE ;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=1;
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_Cmd(TIM2,ENABLE);//启动定时器
	
}
void TIM2_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM2,TIM_IT_Update )==SET)
	{
		Num++;
		TIM_ClearITPendingBit (TIM2,TIM_IT_Update); 
	}
}