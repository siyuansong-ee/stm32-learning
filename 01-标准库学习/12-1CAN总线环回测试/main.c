#include "stm32f10x.h" // Device header
#include "delay.h"
#include "OLED.h"
#include "MyCAN.h"
#include "key.h"
uint8_t KeyNum;
int main()
{
	OLED_Init();
	MyCAN_Init();
	key_Init();
	OLED_ShowString(2,1,"RxID:");
	OLED_ShowString(3,1,"Leng:");
	OLED_ShowString(4,1,"Data:");
	
	while(1)
	{
		KeyNum=key_GetNum();
		if(KeyNum==1)
		{
			uint8_t TxData[]={0x66,0x88};
			MyCAN_Transmit(0x123,2,TxData);
		}
		if(MyCAN_ReceiveFlag())
		{
			uint32_t RxID;
			uint8_t RxLength;
			uint8_t RxData[8];
			MyCAN_Receive(&RxID,&RxLength,RxData);
			
			OLED_ShowHexNum(2,6,RxID,3);
			OLED_ShowHexNum(3,6,RxLength,1);
			OLED_ShowHexNum(4,6,RxData[0],2);
			OLED_ShowHexNum(4,9,RxData[1],2);
	
		}
		
	}
}