#include "stm32f10x.h"                  // Device header

uint16_t CountSensor_Count;
void CountSensor_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU ;//默认上拉输入，即开始为高电平
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_14 ;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz ;
	GPIO_Init(GPIOB,&GPIO_InitStructure);
	
	GPIO_EXTILineConfig (GPIO_PortSourceGPIOB,GPIO_PinSource14);//AFIO的选择，给EXTI引线
	
	EXTI_InitTypeDef EXTI_InitStructure;
	EXTI_InitStructure.EXTI_Line=EXTI_Line14 ;
	EXTI_InitStructure.EXTI_LineCmd=ENABLE;
	EXTI_InitStructure.EXTI_Mode=EXTI_Mode_Interrupt;//中断模式
	EXTI_InitStructure.EXTI_Trigger= EXTI_Trigger_Falling;//下降沿,上升沿
	EXTI_Init(&EXTI_InitStructure);
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//嵌套向量中断控制器
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel=EXTI15_10_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1;//抢占优先级，可以打断插队，数字越小越靠前
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=1;//子优先级 不能打断

	NVIC_Init(&NVIC_InitStructure);

	
}
uint16_t CountSensor_Get()
{
	return CountSensor_Count ;
}
void EXTI15_10_IRQHandler(void)//中断函数，名称不能改
{
	  if(EXTI_GetFlagStatus(EXTI_Line14)==SET )//判断是否
	  {
		  CountSensor_Count ++;
		  EXTI_ClearITPendingBit(EXTI_Line14 );//清除中断挂起
		  
	  }
}