#include "stm32f10x.h"                  // Device header
uint16_t AD_Value[4];
void AD_Init(void)
{
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_ADC1,ENABLE );
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA,ENABLE);
	RCC_ADCCLKConfig(RCC_PCLK2_Div6 );//72MHZ/6=12<14
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1 ,ENABLE );
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AIN;//模拟输入,断开PA0,ADC采集原始电压
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Speed =GPIO_Speed_50MHz ;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	ADC_RegularChannelConfig(ADC1,ADC_Channel_0 ,1,ADC_SampleTime_55Cycles5);
	ADC_RegularChannelConfig(ADC1,ADC_Channel_1 ,2,ADC_SampleTime_55Cycles5);
	ADC_RegularChannelConfig(ADC1,ADC_Channel_2 ,3,ADC_SampleTime_55Cycles5);
	ADC_RegularChannelConfig(ADC1,ADC_Channel_3 ,4,ADC_SampleTime_55Cycles5);

	
	//1：规则组rank，采样电容填充周期
	ADC_InitTypeDef ADC_InitStruct;
	ADC_InitStruct.ADC_ContinuousConvMode=ENABLE ;//连续
	ADC_InitStruct.ADC_DataAlign=ADC_DataAlign_Right;//右对齐
	ADC_InitStruct.ADC_ExternalTrigConv=ADC_ExternalTrigConv_None;//软件触发
	ADC_InitStruct.ADC_Mode=ADC_Mode_Independent;//ADC12独立
	ADC_InitStruct.ADC_NbrOfChannel=4;	//Nbr=Scan+1单通道，加N多通道
	ADC_InitStruct.ADC_ScanConvMode=ENABLE ;
	ADC_Init(ADC1,&ADC_InitStruct);
	//上电校准四件套
	
	DMA_InitTypeDef DMA_InitStructure;
	DMA_InitStructure.DMA_BufferSize=4;//几次
	DMA_InitStructure.DMA_DIR=DMA_DIR_PeripheralSRC;//外设做数据源
	DMA_InitStructure.DMA_M2M=DMA_M2M_Disable;//非软件
	DMA_InitStructure.DMA_MemoryBaseAddr=(uint32_t)AD_Value;
	DMA_InitStructure.DMA_MemoryDataSize=DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryInc=DMA_MemoryInc_Enable; 
	DMA_InitStructure.DMA_Mode=DMA_Mode_Circular;//循环
	DMA_InitStructure.DMA_PeripheralBaseAddr=(uint32_t)&ADC1->DR ;
	DMA_InitStructure.DMA_PeripheralDataSize=DMA_PeripheralDataSize_HalfWord;//低16位
	DMA_InitStructure.DMA_PeripheralInc=DMA_PeripheralInc_Disable;//不自增
	DMA_InitStructure.DMA_Priority=DMA_Priority_Medium;
	DMA_Init(DMA1_Channel1,&DMA_InitStructure);
	DMA_Cmd(DMA1_Channel1,ENABLE);
	ADC_DMACmd(ADC1,ENABLE);
	ADC_Cmd(ADC1,ENABLE );
	ADC_ResetCalibration(ADC1);//复位校准
	while(ADC_GetResetCalibrationStatus (ADC1)==SET);
	ADC_StartCalibration(ADC1);//开始校准
	while(ADC_GetCalibrationStatus(ADC1)==SET);
	ADC_SoftwareStartConvCmd(ADC1,ENABLE);//软件触发，ADC转换

}
