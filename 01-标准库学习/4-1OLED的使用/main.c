#include "stm32f10x.h" // Device header
#include "delay.h"
#inciude "OLED.h"
int main()
{
	OLED_Init();
	OLED_ShowChar(1,1,'A');
	OLED_ShowString(1,3,"Helloworld");
	OLED_ShowNum(2,1,12345,5);//小，高位切掉
	OLED_ShowSignedNum(2,7,12345,5);//带符号
	OLED_ShowHexNum(3,1,0xAA55,4);
	OLED_ShowBinNum(4,1,0xAA55,16)//十六进制
	OLED_Clear();
}