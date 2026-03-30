#ifndef _SPI_OOP__H
#define _SPI_OOP__H
#include "main.h"
typedef struct SPI_Device_Obj SPI_Device;

typedef struct SPI_Device_Obj
{
	//虚函数表
	void (*Init)(const SPI_Device* self);
	HAL_StatusTypeDef (*Transmit)(const SPI_Device* self, uint8_t *pData, 
		uint16_t Size, uint32_t Timeout);
	HAL_StatusTypeDef (*Receive)(const SPI_Device* self, uint8_t *pData, 
		uint16_t Size, uint32_t Timeout);
	HAL_StatusTypeDef (*TransmitReceive)(const SPI_Device* self, uint8_t *pTxData, 
		uint8_t *pRxData, uint16_t Size,uint32_t Timeout);
	void (*SetCS)(const SPI_Device* self,GPIO_PinState PinState);

	void* spi_config;
}SPI_Device;

extern const SPI_Device HardSPI1_Obj;
extern const SPI_Device HardSPI2_Obj;
extern const SPI_Device HardSPI1_DMA_Obj;

#endif