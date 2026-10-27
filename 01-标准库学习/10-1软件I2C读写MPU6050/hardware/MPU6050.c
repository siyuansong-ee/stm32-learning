#include "stm32f10x.h"
#include "MyI2C.h"
#define MPU6050_ADDRESS  0xD0
#include "MPU6050_Reg.h"

//加速度、角速度是**16 位有符号数据**拆成【高字节 _H】、【低字节 _L】两个寄存器存放
//三轴加速度 + 三轴陀螺仪
void MPU6050_WriteReg(uint8_t RegAddress,uint8_t Data)
{
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS);//发送从机【写地址 0xD0】
	MyI2C_ReceiveAck();
	MyI2C_SendByte(RegAddress);//发送要操作的寄存器地址
	MyI2C_ReceiveAck();
	MyI2C_SendByte(Data);
	MyI2C_ReceiveAck();
	MyI2C_Stop();
}

uint8_t MPU6050_ReadReg(uint8_t RegAddress)
{
	uint8_t Data;
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(RegAddress);
	MyI2C_ReceiveAck();
	MyI2C_Start();			//重复起始 Restart
	MyI2C_SendByte(MPU6050_ADDRESS | 0x01);//换成读地址
	MyI2C_ReceiveAck();
	Data = MyI2C_ReceiveByte();
	MyI2C_SendAck(1);		//主机读完发NACK，停止
	MyI2C_Stop();
	return Data;
}

//按照传感器数据手册里的寄存器说明，通过 I2C 往指定寄存器地址写入配置参数
void MPU6050_Init(void)
{
	MyI2C_Init();
	MPU6050_WriteReg(MPU6050_PWR_MGMT_1,0x01);//唤醒 MPU6050
	MPU6050_WriteReg(MPU6050_PWR_MGMT_2,0x00);//开启轴
	MPU6050_WriteReg(MPU6050_SMPLRT_DIV,0x09);
	MPU6050_WriteReg(MPU6050_CONFIG,0x06);
	MPU6050_WriteReg(MPU6050_GYRO_CONFIG,0x18);
	MPU6050_WriteReg(MPU6050_ACCEL_CONFIG,0x18);
}

uint8_t MPU6050_GetID(void)//身份寄存器
{
	return MPU6050_ReadReg(MPU6050_WHO_AM_I);//用来调试
}

//读取加速度+陀螺仪
void MPU6050_GetData(int16_t *AccX,int16_t *AccY,int16_t *AccZ,
					 int16_t *GyroX,int16_t *GyroY,int16_t *GyroZ)
{
	uint8_t DataH,DataL;
	
	//加速度X
	DataH=MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H);
	DataL=MPU6050_ReadReg(MPU6050_ACCEL_XOUT_L);
	*AccX=(DataH<<8)|DataL;
	
	//加速度Y
	DataH=MPU6050_ReadReg(MPU6050_ACCEL_YOUT_H);
	DataL=MPU6050_ReadReg(MPU6050_ACCEL_YOUT_L);
	*AccY=(DataH<<8)|DataL;
	
	//加速度Z
	DataH=MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_H);
	DataL=MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_L);
	*AccZ=(DataH<<8)|DataL;
	
	//陀螺仪X
	DataH=MPU6050_ReadReg(MPU6050_GYRO_XOUT_H);
	DataL=MPU6050_ReadReg(MPU6050_GYRO_XOUT_L);
	*GyroX =(DataH<<8)|DataL;
	
	//陀螺仪Y
	DataH=MPU6050_ReadReg(MPU6050_GYRO_YOUT_H);
	DataL=MPU6050_ReadReg(MPU6050_GYRO_YOUT_L);
	*GyroY =(DataH<<8)|DataL;
	
	//陀螺仪Z
	DataH=MPU6050_ReadReg(MPU6050_GYRO_ZOUT_H);
	DataL=MPU6050_ReadReg(MPU6050_GYRO_ZOUT_L);
	*GyroZ =(DataH<<8)|DataL;//高 8 位，左移 8 位，放到 16 位数据的高字节位置
}
