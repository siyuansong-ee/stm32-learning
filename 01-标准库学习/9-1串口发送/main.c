#include "stm32f10x.h" // Device header
#include "delay.h"
#include "OLED.h"
#include "Serial.h"
int main()
{
	OLED_Init();
	Serial_Init();
	//Serial_SendByte(0x41);
//	uint8_t MyArray[]={0x42,0x43,0x44};
//	Serial_SendArray(MyArray ,3);
//	Serial_SendString("HelloWorld\r\n");//两个转义字符换行
	
	//Serial_SendNumber(12345,5)
	//printf("Num=&d\r\n",666);
//	char String[100];
//	sprintf(String,"Num=%d\r\n",666);
//	Serial_SendString(String);
	Serial_Printf("Num=%d\r\n",666);






}