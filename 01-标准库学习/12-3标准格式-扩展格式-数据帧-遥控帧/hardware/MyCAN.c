#include "stm32f10x.h"                  // Device header

void MyCAN_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);

	CAN_InitTypeDef CAN_InitStructure;
	CAN_InitStructure.CAN_ABOM=DISABLE;
	CAN_InitStructure.CAN_AWUM=DISABLE;
	CAN_InitStructure.CAN_BS1=CAN_BS1_2tq;
	CAN_InitStructure.CAN_BS2=CAN_BS2_3tq;
	CAN_InitStructure.CAN_Mode=CAN_Mode_LoopBack;//环回测试模式。正常发送改成nomal
	CAN_InitStructure.CAN_NART=DISABLE;
	CAN_InitStructure.CAN_Prescaler=48;//Tq 位时序
	CAN_InitStructure.CAN_RFLM=DISABLE;
	CAN_InitStructure.CAN_SJW=CAN_SJW_2tq;
	CAN_InitStructure.CAN_TTCM=DISABLE;
	CAN_InitStructure.CAN_TXFP=DISABLE;
	CAN_Init(CAN1,&CAN_InitStructure);
	
	CAN_FilterInitTypeDef CAN_FilterInitStructure;
	CAN_FilterInitStructure.CAN_FilterActivation=ENABLE;
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment=CAN_Filter_FIFO0;
	CAN_FilterInitStructure.CAN_FilterIdHigh=0x0000;
	CAN_FilterInitStructure.CAN_FilterIdLow=0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh=0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdLow=0x0000;
	CAN_FilterInitStructure.CAN_FilterMode=CAN_FilterMode_IdMask;
	CAN_FilterInitStructure.CAN_FilterNumber=0;
	CAN_FilterInitStructure.CAN_FilterScale=CAN_FilterScale_32bit;
	CAN_FilterInit(&CAN_FilterInitStructure);
	
	
}

//发送函数,用结构体
void MyCAN_Transmit(CanTxMsg *TxMessage)
{
	uint8_t TransmitMailbox=CAN_Transmit(CAN1,TxMessage);//获得写入的邮箱号
	if(TransmitMailbox==CAN_TxStatus_NoMailBox) return;  //3 个邮箱都满，直接返回
	
	uint32_t timeout=0;//超时退出
	while(CAN_TransmitStatus(CAN1,TransmitMailbox)!=CAN_TxStatus_Ok)//等待报文发送
	{
		timeout++;
		if(timeout>100000)
		{
			break;
		}
	}
}

uint8_t MyCAN_ReceiveFlag(void)//Flag：标志
{
	if(CAN_MessagePending(CAN1,CAN_FIFO0)>0)//读取FIFO0队伍的长度，判断是否发送
	{
		return 1;
	}
	return 0;
}

//读取函数
void MyCAN_Receive(CanRxMsg *RxMessage)//用指针返回多个值
{
	CAN_Receive(CAN1,CAN_FIFO0,RxMessage);	
}
