#include "stm32f10x.h" // Device header
#include "delay.h"
#include "OLED.h"
#include "Serial.h"
uint8_t RxData;
int main()
{
	OLED_Init();
	OLED_ShowString (1,1,"RxData:");
	Serial_Init();

	//查询方法
//	while(1)
//	{
//		if(USART_GetFlagStatus (USART1,USART_FLAG_RXNE))==SET)
//		{
//			RxData=USART_ReceiveData (USART1);//自动清除标志位
//			OLED_ShowHexNum(1,8,RxData,2);
//		}
//	}
//NVIC方法
while(1)
	{
		if(Serial_GetRxFlag()==1)
		{
			RxData=Serial_GetRxData(); 
			Serial_SendByte(RxData);
			OLED_ShowHexNum(1,8,RxData,2);
		}
	}



}