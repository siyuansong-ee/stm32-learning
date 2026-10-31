#include "stm32f10x.h"                  // Device header
#include "MySPI.h"
#include "W25Q64_Ins.h"
void W25Q64_Init(void)
{
	MySPI_Init();
	
}

void W25Q64_ReadID(uint8_t *MID ,uint16_t *DID)
{
	MySPI_Start();
	MySPI_SwapByte(W25Q64_JEDEC_ID);
	*MID=MySPI_SwapByte(W25Q64_DUMMY_BYTE);//得到厂商ID
	*DID=MySPI_SwapByte(W25Q64_DUMMY_BYTE);//高八位
	*DID<<=8;
	*DID|=MySPI_SwapByte(W25Q64_DUMMY_BYTE);//用或把低八位并进去
	MySPI_Stop();
}
	
void W25Q64_WriteEnable(void)
{
	MySPI_Start();
	MySPI_SwapByte(W25Q64_WRITE_ENABLE);
	MySPI_Stop();
}

void W25Q64_WaitBusy(void)//等待忙标志，1正在擦除
{
	uint32_t Timeout;
	MySPI_Start();//连续读出状态寄存器
	MySPI_SwapByte(W25Q64_READ_STATUS_REGISTER_1);
	Timeout=100000;
	while((MySPI_SwapByte(W25Q64_DUMMY_BYTE)&0x01)==0x01)//判断最后一位标志位
	{
		Timeout--;
		if(Timeout==0)
		{
			break;
		}
	}
	MySPI_Stop();
}

void W25Q64_PageProgram(uint32_t Address,uint8_t *DataArray,uint16_t count)//指针
{
	W25Q64_WriteEnable();
	uint16_t i;
	MySPI_Start();//24 位地址，**先发高位字节，再发低位字节**
	MySPI_SwapByte(W25Q64_PAGE_PROGRAM);
	MySPI_SwapByte(Address>>16);//把后16位直接挤掉
	MySPI_SwapByte(Address>>8);
	MySPI_SwapByte(Address);
	for(i=0;i<count;i++)
	{
		MySPI_SwapByte(DataArray[i]);
	}
	MySPI_Stop();
	W25Q64_WaitBusy();
	
}
void W25Q64_SectorErase(uint32_t Address)//所在扇区被擦除
{
	W25Q64_WriteEnable();
	MySPI_Start();
	MySPI_SwapByte(W25Q64_SECTOR_ERASE_4KB);
	MySPI_SwapByte(Address>>16);//把后16位直接挤掉
	MySPI_SwapByte(Address>>8);
	MySPI_SwapByte(Address);
	MySPI_Stop();
	W25Q64_WaitBusy();
}

void W25Q64_ReadData(uint32_t Address,uint8_t *DataArray,uint32_t count)
{
	uint32_t i;
	MySPI_Start();
	MySPI_SwapByte(W25Q64_READ_DATA);
	MySPI_SwapByte(Address>>16);
	MySPI_SwapByte(Address>>8);
	MySPI_SwapByte(Address);
	for(i=0;i<count;i++)//主机读取数据
	{
		DataArray[i]=MySPI_SwapByte(W25Q64_DUMMY_BYTE);
	}
	MySPI_Stop();

}