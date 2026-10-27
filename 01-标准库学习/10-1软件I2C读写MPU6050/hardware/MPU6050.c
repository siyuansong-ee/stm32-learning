#include "stm32f10x.h"
#include "MyI2C.h"
#include "MPU6050.h"
#include "MPU6050_Reg.h"

/* 7 位从机地址 0x68，左移一位、最低位补 0 = 写方向 */
#define MPU6050_ADDRESS  0xD0

/* ---------------- 寄存器读写 ---------------- */

void MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data)
{
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS);        /* 写地址 0xD0 */
	MyI2C_ReceiveAck();
	MyI2C_SendByte(RegAddress);             /* 要写的寄存器 */
	MyI2C_ReceiveAck();
	MyI2C_SendByte(Data);                   /* 要写的数据 */
	MyI2C_ReceiveAck();
	MyI2C_Stop();
}

uint8_t MPU6050_ReadReg(uint8_t RegAddress)
{
	uint8_t Data;

	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS);        /* 先以写方向告诉它读哪个寄存器 */
	MyI2C_ReceiveAck();
	MyI2C_SendByte(RegAddress);
	MyI2C_ReceiveAck();

	MyI2C_Start();                          /* 重复起始条件，不清总线 */
	MyI2C_SendByte(MPU6050_ADDRESS | 0x01); /* 换读地址 */
	MyI2C_ReceiveAck();

	Data = MyI2C_ReceiveByte();
	MyI2C_SendAck(1);                       /* 只读 1 字节，回 NACK */
	MyI2C_Stop();
	return Data;
}

/* ---------------- 初始化 ---------------- */

void MPU6050_Init(void)
{
	MyI2C_Init();

	MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x01);    /* 解除睡眠，时钟源选 X 轴陀螺 PLL */
	MPU6050_WriteReg(MPU6050_PWR_MGMT_2, 0x00);    /* 六轴全部打开 */
	MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x09);    /* 采样率 = 1kHz / (1+9) = 100Hz */
	MPU6050_WriteReg(MPU6050_CONFIG, 0x06);        /* 数字低通滤波 5Hz，抑制抖动 */
	MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x18);   /* 陀螺量程 ±2000°/s（FS_SEL=3） */
	MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x18);  /* 加速度量程 ±16g（AFS_SEL=3） */
}

uint8_t MPU6050_GetID(void)
{
	return MPU6050_ReadReg(MPU6050_WHO_AM_I);       /* 正常应返回 0x68 */
}

/* ---------------- 一次读全六轴 ---------------- */

/* 从 0x3B 开始连续读 14 字节：
 * [0..1] AX  [2..3] AY  [4..5] AZ  [6..7] 温度  [8..9] GX  [10..11] GY  [12..13] GZ
 * 好处：只走一趟总线，比逐个寄存器读快约 10 倍，而且六轴是同一时刻的样本 */
void MPU6050_GetData(int16_t *AccX, int16_t *AccY, int16_t *AccZ,
                     int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ)
{
	uint8_t Data[14];
	uint8_t i;

	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(MPU6050_ACCEL_XOUT_H);   /* 首地址 0x3B */
	MyI2C_ReceiveAck();

	MyI2C_Start();                          /* 重复起始，转读 */
	MyI2C_SendByte(MPU6050_ADDRESS | 0x01);
	MyI2C_ReceiveAck();

	for (i = 0; i < 14; i++)
	{
		Data[i] = MyI2C_ReceiveByte();
		MyI2C_SendAck(i < 13 ? 0 : 1);      /* 前 13 字节 ACK，最后一字节 NACK */
	}
	MyI2C_Stop();

	/* 16 位有符号数 = 高字节在前，拼起来后再转成 int16_t */
	*AccX  = (int16_t)(((uint16_t)Data[0]  << 8) | Data[1]);
	*AccY  = (int16_t)(((uint16_t)Data[2]  << 8) | Data[3]);
	*AccZ  = (int16_t)(((uint16_t)Data[4]  << 8) | Data[5]);
	*GyroX = (int16_t)(((uint16_t)Data[8]  << 8) | Data[9]);
	*GyroY = (int16_t)(((uint16_t)Data[10] << 8) | Data[11]);
	*GyroZ = (int16_t)(((uint16_t)Data[12] << 8) | Data[13]);
}
