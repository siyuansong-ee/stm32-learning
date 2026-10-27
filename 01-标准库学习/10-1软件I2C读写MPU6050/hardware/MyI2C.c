#include "stm32f10x.h"                  // Device header
#include "delay.h"
//封装函数
//软件模拟
void MyI2C_W_SCL(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOB,GPIO_Pin_10,(BitAction)BitValue);//Bitaction是强制类型转换
	delay_us(10);
}

void MyI2C_W_SDA(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOB,GPIO_Pin_11,(BitAction)BitValue);
	delay_us(10);//等电平稳定
}

//读取 PB11（SDA）引脚电平，返回 0 或者 1。
uint8_t MyI2C_R_SDA(void)
{
	return GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_11);
}

//配置
void MyI2C_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_OD;//开漏输出
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_10|GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&GPIO_InitStructure);
	
	GPIO_SetBits(GPIOB,GPIO_Pin_10|GPIO_Pin_11); //置高电平 
}

void MyI2C_Start(void)
{	
	MyI2C_W_SDA(1);//兼容起始条件和重复起始条件
	MyI2C_W_SCL(1);//先释放
	MyI2C_W_SDA(0);//start
	MyI2C_W_SCL(0);
}
//起始是拉低，结束是抬高
void MyI2C_Stop(void)
{
	MyI2C_W_SDA(0);//确保他是低的
	MyI2C_W_SCL(1);
	MyI2C_W_SDA(1);
}

void MyI2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for(i=0;i<8;i++)
	{
		MyI2C_W_SDA(Byte &(0x80>>i));//取最高位，逐个写入字节
		MyI2C_W_SCL(1);//拉高，从机采样
		MyI2C_W_SCL(0);//从机拉低换下一位
	}
}

//从机发送
uint8_t MyI2C_ReceiveByte(void)
{
	uint8_t i,Byte=0x00;
	MyI2C_W_SDA(1);//主机释放
	for(i=0;i<8;i++)
	{
		MyI2C_W_SCL(1);
		if(MyI2C_R_SDA()==1)
		{
			Byte|=(0x80>>i);//按位或
		}
		MyI2C_W_SCL(0);
	}
	return Byte;
}

//主机发送应答
void MyI2C_SendAck(uint8_t AckBit)
{
	MyI2C_W_SDA(AckBit);
	MyI2C_W_SCL(1);
	MyI2C_W_SCL(0);
}

uint8_t MyI2C_ReceiveAck(void)
{
	uint8_t AckBit;
	MyI2C_W_SDA(1);//主机释放
	delay_us(1);
	MyI2C_W_SCL(1);
	AckBit=MyI2C_R_SDA();//看从机是否接受
	MyI2C_W_SCL(0);//0是应答
	return AckBit;
}