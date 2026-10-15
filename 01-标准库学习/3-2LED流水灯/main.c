#include "stm32f10x.h" // Device header
#include "delay.h"
int main(void)
{
//    RCC->APB2ENR=0x00000010;
//	GPIOC->CRH=0x00300000;
//	GPIOC->ODR=0x00002000;//0亮，2灭
   RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
   GPIO_InitTypeDef GPIO_InitStruct;//结构体
	GPIO_InitStruct.GPIO_Mode= GPIO_Mode_Out_OD;
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_All;
	GPIO_InitStruct.GPIO_Speed= GPIO_Speed_50MHz;
   GPIO_Init(GPIOA,&GPIO_InitStruct);//初始化
	//GPIO_SetBits(GPIOA,GPIO_Pin_0);//高电平
	//GPIO_ResetBits(GPIOA,GPIO_Pin_0);//低电平
	//范围更大，一个控制两个状态
	//GPIO_WriteBit(GPIOA,GPIO_Pin_0,Bit_RESET);
	while(1)
	{
		GPIO_Write(GPIOA,~0x0001);	
		delay_ms(500);	
		GPIO_Write(GPIOA,~0x0002);	
		delay_ms(500);	
		GPIO_Write(GPIOA,~0x0004);	
		delay_ms(500);	
		GPIO_Write(GPIOA,~0x0008);	
		delay_ms(500);	
		GPIO_Write(GPIOA,~0x0010);	
		delay_ms(500);	
		GPIO_Write(GPIOA,~0x0020);	
		delay_ms(500);	
		GPIO_Write(GPIOA,~0x0040);	
		delay_ms(500);	
		GPIO_Write(GPIOA,~0x0080);	
		delay_ms(500);	
		
	}
}