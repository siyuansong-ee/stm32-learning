#include "stm32f10x.h"                  // Device header

void AD_Init(void)
{
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_ADC1,ENABLE );
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA,ENABLE);
	RCC_ADCCLKConfig(RCC_PCLK2_Div6 );//72MHZ/6=12<14
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AIN;//模拟输入,断开PA0,ADC采集原始电压
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Speed =GPIO_Speed_50MHz ;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	ADC_RegularChannelConfig(ADC1,ADC_Channel_0 ,1,ADC_SampleTime_55Cycles5);
	//1：规则组rank，采样电容填充周期
	ADC_InitTypeDef ADC_InitStruct;
	ADC_InitStruct.ADC_ContinuousConvMode=DISABLE ;//单次转换，非连续
	ADC_InitStruct.ADC_DataAlign=ADC_DataAlign_Right;//右对齐
	ADC_InitStruct.ADC_ExternalTrigConv=ADC_ExternalTrigConv_None;//软件触发
	ADC_InitStruct.ADC_Mode=ADC_Mode_Independent;//ADC12独立
	ADC_InitStruct.ADC_NbrOfChannel=1;	//Nbr=Scan+1单通道，加N多通道
	ADC_InitStruct.ADC_ScanConvMode=DISABLE ;
	ADC_Init(ADC1,&ADC_InitStruct);
	//上电校准四件套
	ADC_Cmd(ADC1,ENABLE );
	ADC_ResetCalibration(ADC1);//复位校准
	while(ADC_GetResetCalibrationStatus (ADC1)==SET);
	ADC_StartCalibration(ADC1);//开始校准
	while(ADC_GetCalibrationStatus(ADC1)==SET);
}
uint16_t AD_GetValue(uint8_t ADC_Channel)
{
	ADC_RegularChannelConfig(ADC1,ADC_Channel ,1,ADC_SampleTime_55Cycles5);

	ADC_SoftwareStartConvCmd(ADC1,ENABLE);//软件触发
	while(ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC)==RESET);
	return ADC_GetConversionValue(ADC1);
}