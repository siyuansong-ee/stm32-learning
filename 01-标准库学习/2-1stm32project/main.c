#include "stm32f10x.h" // Device header
#include "delay.h"
int main(void)
{
//    RCC->APB2ENR=0x00000010;
//	GPIOC->CRH=0x00300000;
//	GPIOC->ODR=0x00002000;//0亮，2灭
   RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
   GPIO_InitTypeDef GPIO_InitStruct;//结构体
	GPIO_InitStruct.GPIO_Mode= GPIO_Mode_Out_PP;//推挽模式高低都可驱动，开漏只有低电平
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_0;
	GPIO_InitStruct.GPIO_Speed= GPIO_Speed_50MHz;
   GPIO_Init(GPIOA,&GPIO_InitStruct);//初始化
	//GPIO_SetBits(GPIOA,GPIO_Pin_0);//高电平
	//GPIO_ResetBits(GPIOA,GPIO_Pin_0);//低电平
	;//范围更大，一个控制两个状态
	
	while(1)
	{
			GPIO_WriteBit(GPIOA,GPIO_Pin_0,Bit_RESET);
			delay_ms(100);
			GPIO_WriteBit(GPIOA,GPIO_Pin_0,Bit_SET);	
			delay_ms(100);
	}
}