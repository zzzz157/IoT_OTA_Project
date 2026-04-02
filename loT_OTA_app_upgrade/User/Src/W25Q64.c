#include "main.h"
#include "string.h"
#include "SPI_OOP.h"
#include "W25Q64.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static SemaphoreHandle_t w25q_mutex=NULL;

typedef struct
{
	const SPI_Device* spi_dev;
}w25q_config;

static int W25Q64_WriteEnable(const W25Q64_t* self)
{
	uint8_t txdata=W25Q64_WRITE_ENABLE;
	w25q_config* cfg=self->W25Q_config;
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_RESET);
	if(cfg->spi_dev->Transmit(cfg->spi_dev,&txdata,1,10)!=HAL_OK) return 0;
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
	return 1;
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

static int W25Q64_WaitBusy(const W25Q64_t* self)
{
	uint16_t TimeOut=3000;
	w25q_config* cfg=self->W25Q_config;
	uint8_t txdata=W25Q64_READ_STATUS_REGISTER_1,rxdata=0xFF;
	while(1)
	{
		cfg->spi_dev->SetCS(cfg->spi_dev, GPIO_PIN_RESET);
		cfg->spi_dev->Transmit(cfg->spi_dev, &txdata, 1, 10);
		if(cfg->spi_dev->Receive(cfg->spi_dev, &rxdata, 1, 10) != HAL_OK) 
		{
			cfg->spi_dev->SetCS(cfg->spi_dev, GPIO_PIN_SET);
			return 0;
		}
		cfg->spi_dev->SetCS(cfg->spi_dev, GPIO_PIN_SET); // 每次读完拉高CS
		if((rxdata & 0x01) != 0x01) return 1;
		TimeOut--;
		if(TimeOut == 0) return 0;
		vTaskDelay(1);
	}
}
static void W25Q64_Init(W25Q64_t* self)
{
	w25q_config* cfg=self->W25Q_config;
	if(w25q_mutex==NULL) w25q_mutex=xSemaphoreCreateMutex();
	cfg->spi_dev->Init(cfg->spi_dev);
	self->DevID=W25Q64GetID(self);
}
static int W25Q64_PagePrograme(const W25Q64_t* self,uint32_t address,uint8_t *data,uint16_t size)
{
	uint8_t state=1;
	w25q_config* cfg=self->W25Q_config;
	xSemaphoreTake(w25q_mutex,portMAX_DELAY);
	if(W25Q64_WriteEnable(self)==0)  
	{
		state=0;
		goto exit;
	}
	static uint8_t tx_buf[W25Q64_PAGE_LEN+4];
	tx_buf[0] = W25Q64_PAGE_PROGRAM;
	tx_buf[1] = address >> 16;
	tx_buf[2] = address >> 8;
	tx_buf[3] = address & 0xFF;
	memcpy(&tx_buf[4], data, size);
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_RESET);
	if(cfg->spi_dev->Transmit(cfg->spi_dev,tx_buf,size+4,1000)!=HAL_OK)
	{
		cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
		state=0;
		goto exit;
	}
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
	if(W25Q64_WaitBusy(self)==0)
	{
		state=0;
		goto exit;
	}
exit:
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
	xSemaphoreGive(w25q_mutex);
	return state;
}
/* Erase 4KB */
static int W25Q64_SectorErase(const W25Q64_t* self,uint32_t address)
{
	uint8_t state=1;
	w25q_config* cfg=self->W25Q_config;
	xSemaphoreTake(w25q_mutex,portMAX_DELAY);
	if(W25Q64_WriteEnable(self)==0)
	{
		state=0;
		goto exit;
	}
	uint8_t txdata[4]={W25Q64_SECTOR_ERASE_4KB,address>>16,address>>8,address};
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_RESET);
	if(cfg->spi_dev->Transmit(cfg->spi_dev,txdata,4,100)!=HAL_OK)
	{
		state=0;
		goto exit;
	}
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
	if(W25Q64_WaitBusy(self)==0) 
	{
		state=0;
		goto exit;
	}
exit:
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
	xSemaphoreGive(w25q_mutex);
	return state;
}
static int W25Q64_ReadDatas(const W25Q64_t* self,uint32_t address,uint8_t *data,uint16_t size)
{
	uint8_t state=1;
	w25q_config* cfg=self->W25Q_config;
	xSemaphoreTake(w25q_mutex,portMAX_DELAY);
	uint8_t txdata[4]={W25Q64_READ_DATA,address>>16,address>>8,address};
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_RESET);
	if(cfg->spi_dev->Transmit(cfg->spi_dev,txdata,4,100)!=HAL_OK)
	{
		state=0;
		goto exit;
	}
	if(cfg->spi_dev->Receive(cfg->spi_dev,data,size,1000)!=HAL_OK)
	{
		state=0;
		goto exit;
	}
exit:
	cfg->spi_dev->SetCS(cfg->spi_dev,GPIO_PIN_SET);
	xSemaphoreGive(w25q_mutex);
	return state;
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
	
