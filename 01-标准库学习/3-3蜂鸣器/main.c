#include "stm32f10x.h" // Device header
#include "delay.h"
int main(void)
{
//    RCC->APB2ENR=0x00000010;
//	GPIOC->CRH=0x00300000;
//	GPIOC->ODR=0x00002000;//0亮，2灭
   RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
   GPIO_InitTypeDef GPIO_InitStruct;//结构体
	GPIO_InitStruct.GPIO_Mode= GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_12;
	GPIO_InitStruct.GPIO_Speed= GPIO_Speed_50MHz;
   GPIO_Init(GPIOB,&GPIO_InitStruct);//初始化
	//GPIO_SetBits(GPIOA,GPIO_Pin_0);//高电平
	//c//低电平
	//范围更大，一个控制两个状态
	//GPIO_WriteBit(GPIOA,GPIO_Pin_0,Bit_RESET);
	while(1)
	{
		GPIO_ResetBits(GPIOB,GPIO_Pin_12);
		delay_ms (100);
		GPIO_SetBits(GPIOB,GPIO_Pin_12);
		delay_ms (100);
		GPIO_ResetBits(GPIOB,GPIO_Pin_12);
		delay_ms (100);
		GPIO_SetBits(GPIOB,GPIO_Pin_12);
		delay_ms (700);
		
	}
}