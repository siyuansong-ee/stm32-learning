#include "stm32f10x.h"                  // Device header

void MySPI_W_SS(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOA,GPIO_Pin_4,(BitAction)BitValue);
}

void MySPI_W_SCK(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOA,GPIO_Pin_5,(BitAction)BitValue);
}

void MySPI_W_MOSI(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOA,GPIO_Pin_7,(BitAction)BitValue);
}

uint8_t MySPI_R_MISO(void)
{
	return GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_6);
}

void MySPI_Init()
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_PP;//推挽输出
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;//推挽输出
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_6 ;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);

	MySPI_W_SS (1);
	MySPI_W_SCK (0);
	

}

void MySPI_Start(void)
{
	MySPI_W_SS (0);
}

void MySPI_Stop(void)
{
	MySPI_W_SS (1);
}
//模式0
uint8_t MySPI_SwapByte(uint8_t Bytesend)
{
	uint8_t i;
	for(i=0;i<8;i++)
	{
		MySPI_W_MOSI(Bytesend&0x80);//按位与
		Bytesend<<=1;//将数据移到MOSI
		MySPI_W_SCK (1);
		if(MySPI_R_MISO()==1)
		{Bytesend |=0x01;}//按位或
		 MySPI_W_SCK(0);
	}

	return Bytesend;
}