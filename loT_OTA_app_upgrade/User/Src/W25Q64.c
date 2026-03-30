#include "main.h"
#include "SPI_OOP.h"
#include "W25Q64.h"
#include "FreeRTOS.h"
#include "task.h"
typedef struct
{
	const SPI_Device* spi_dev;
}w25q_config;

static void W25Q64_WriteEnable(const W25Q64_t* self)
{
	uint8_t txdata=W25Q64_WRITE_ENABLE;
	w25q_config* cfg=self->W25Q_config;
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_RESET);
	cfg->spi_dev->Transmit(cfg->spi_dev,&txdata,1,10);
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
}
static uint16_t W25Q64GetID(const W25Q64_t* self)
{
	w25q_config* cfg=self->W25Q_config;
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_RESET);
	uint8_t txdata=W25Q64_JEDEC_ID,rxdata[3];
	cfg->spi_dev->Transmit(cfg->spi_dev,&txdata,1,10);
	cfg->spi_dev->Receive(cfg->spi_dev,rxdata,3,100);
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
	return (rxdata[1]<<8|rxdata[2]);
}

static void W25Q64_WaitBusy(const W25Q64_t* self)
{
	uint16_t TimeOut=400;
	w25q_config* cfg=self->W25Q_config;
	uint8_t txdata=W25Q64_READ_STATUS_REGISTER_1,rxdata=0xFF;
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_RESET);
	cfg->spi_dev->Transmit(cfg->spi_dev,&txdata,1,10);
	while(1)
	{
		cfg->spi_dev->Receive(cfg->spi_dev,&rxdata,1,10);
		if((rxdata&0x01)!=0x01)
		{
			break;
		}
		TimeOut--;
		if(TimeOut==0)
		{
			break;
		}
		vTaskDelay(1);
	}
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
}
static void W25Q64_Init(W25Q64_t* self)
{
	w25q_config* cfg=self->W25Q_config;
	cfg->spi_dev->Init(cfg->spi_dev);
	self->DevID=W25Q64GetID(self);
}
static void W25Q64_PagePrograme(const W25Q64_t* self,uint32_t address,uint8_t *data,uint16_t size)
{
	w25q_config* cfg=self->W25Q_config;
	W25Q64_WriteEnable(self);
	uint8_t txdata[4]={W25Q64_PAGE_PROGRAM,address>>16,address>>8,address};
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_RESET);
	cfg->spi_dev->Transmit(cfg->spi_dev,txdata,4,100);
	cfg->spi_dev->Transmit(cfg->spi_dev,data,size,1000);
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
	W25Q64_WaitBusy(self);
}

/* Erase 4KB */
static void W25Q64_SectorErase(const W25Q64_t* self,uint32_t address)
{
	w25q_config* cfg=self->W25Q_config;
	W25Q64_WriteEnable(self);
	uint8_t txdata[4]={W25Q64_SECTOR_ERASE_4KB,address>>16,address>>8,address};
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_RESET);
	cfg->spi_dev->Transmit(cfg->spi_dev,txdata,4,100);
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
	W25Q64_WaitBusy(self);
}
static void W25Q64_ReadDatas(const W25Q64_t* self,uint32_t address,uint8_t *data,uint16_t size)
{
	w25q_config* cfg=self->W25Q_config;
	uint8_t txdata[4]={W25Q64_READ_DATA,address>>16,address>>8,address};
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_RESET);
	cfg->spi_dev->Transmit(cfg->spi_dev,txdata,4,100);
	cfg->spi_dev->Receive(cfg->spi_dev,data,size,1000);
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
}

static w25q_config w25q_cfg={
	.spi_dev=&HardSPI2_Obj,
};

W25Q64_t W25QHandle_t={
	.Init=W25Q64_Init,
	.WritePage=W25Q64_PagePrograme,
	.SectorErase=W25Q64_SectorErase,
	.ReadDatas=W25Q64_ReadDatas,
	.W25Q_config=&w25q_cfg,
};
	
