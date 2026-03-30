#include "main.h"
#include "spi.h"
#include "SPI_OOP.h"
#include "FreeRTOS.h"
#include "Task.h"
#include "semphr.h"
//硬件SPI轮询
void HardSPI_Init(const SPI_Device* self);
static void HardSPI_SetCS(const SPI_Device* self,GPIO_PinState PinState);
static HAL_StatusTypeDef HardSPI_Transmit(const SPI_Device* self, uint8_t *pData, 
		uint16_t Size, uint32_t Timeout);
static HAL_StatusTypeDef HardSPI_Receive(const SPI_Device* self, uint8_t *pData, 
		uint16_t Size, uint32_t Timeout);
static HAL_StatusTypeDef HardSPI_TransmitReceive(const SPI_Device* self, uint8_t *pTxData, 
		uint8_t *pRxData, uint16_t Size,uint32_t Timeout);
//硬件SPI_DMA
static void HardSPI_Init_DMA(const SPI_Device* self);
static HAL_StatusTypeDef HardSPI_Transmit_DMA(const SPI_Device* self, uint8_t *pData, 
		uint16_t Size, uint32_t Timeout);
static HAL_StatusTypeDef HardSPI_Receive_DMA(const SPI_Device* self, uint8_t *pData, 
		uint16_t Size, uint32_t Timeout);
static HAL_StatusTypeDef HardSPI_TransmitReceive_DMA(const SPI_Device* self, uint8_t *pTxData, 
		uint8_t *pRxData, uint16_t Size,uint32_t Timeout);
typedef struct {
	//共享资源
    SPI_HandleTypeDef* spi_handle;      // 硬件句柄
    SemaphoreHandle_t mutex;             // 互斥量（所有模式共用）
    SemaphoreHandle_t txCpltSem;         // 发送完成信号量（中断/DMA 用）
    SemaphoreHandle_t rxCpltSem;         // 接收完成信号量（中断/DMA 用）
	SemaphoreHandle_t txrxCpltSem;         // 发送接收完成信号量（中断/DMA 用）
	GPIO_TypeDef* GPIO_Port_CS1;
	uint16_t Pin_CS1;
	//软件I2C
	uint16_t Pin_SCL;
	uint16_t Pin_MOSI;
	uint16_t Pin_MISO;
	uint16_t Soft_Delay_Count;
}SPI_config;

//HardSPI1
static SPI_config SPI1_Hardware={
	.spi_handle=&hspi1,
	.mutex=NULL,
	.txCpltSem = NULL,
    .rxCpltSem = NULL,
	.txrxCpltSem=NULL,
	.GPIO_Port_CS1=GPIOA,
	.Pin_CS1=GPIO_PIN_4,	
};
//HardSPI2
static SPI_config SPI2_Hardware={
	.spi_handle=&hspi2,
	.mutex=NULL,
	.txCpltSem = NULL,
    .rxCpltSem = NULL,
	.txrxCpltSem=NULL,
	.GPIO_Port_CS1=GPIOC,
	.Pin_CS1=GPIO_PIN_0,	
};
						//======Device_Management=====//
typedef struct {
    SPI_TypeDef* instance;
    SPI_config*  config;
} SPI_MapEntry;
// 映射表 以NULL结尾
static const SPI_MapEntry spi_map[] = {
    {SPI1, &SPI1_Hardware},
    {SPI2, &SPI2_Hardware},
    {NULL, NULL}// 终止标志
};
static SPI_config* get_spi_config(SPI_HandleTypeDef* hspi)
{
	for(uint8_t i=0;spi_map[i].instance!= NULL; i++)
	{
		if (spi_map[i].instance == hspi->Instance)
		{
            return spi_map[i].config;
        }
	}
	return NULL;
}
//HardSPI1
const SPI_Device HardSPI1_Obj={
	.Init=HardSPI_Init,
	.Transmit=HardSPI_Transmit,
	.Receive=HardSPI_Receive,
	.TransmitReceive=HardSPI_TransmitReceive,
	.SetCS=HardSPI_SetCS,
	.spi_config=&SPI1_Hardware,
};
//HardSPI2
const SPI_Device HardSPI2_Obj={
	.Init=HardSPI_Init,
	.Transmit=HardSPI_Transmit,
	.Receive=HardSPI_Receive,
	.TransmitReceive=HardSPI_TransmitReceive,
	.SetCS=HardSPI_SetCS,
	.spi_config=&SPI2_Hardware,
};
//HardSPI1_DMA
const SPI_Device HardSPI1_DMA_Obj={
	.Init=HardSPI_Init_DMA,
	.Transmit=HardSPI_Transmit_DMA,
	.Receive=HardSPI_Receive_DMA,
	.TransmitReceive=HardSPI_TransmitReceive_DMA,
	.SetCS=HardSPI_SetCS,
	.spi_config=&SPI1_Hardware,
};
						//========Hardware_SPI=====//
static void HardSPI_SetCS(const SPI_Device* self,GPIO_PinState PinState)
{
	SPI_config* cfg=self->spi_config;
	HAL_GPIO_WritePin(cfg->GPIO_Port_CS1, cfg->Pin_CS1, PinState);
}
static void HardSPI_Init(const SPI_Device* self)
{
	SPI_config* cfg=self->spi_config;
	if (cfg->mutex == NULL) cfg->mutex = xSemaphoreCreateMutex();
}
static void HardSPI_Init_DMA(const SPI_Device* self)
{
	SPI_config* cfg=self->spi_config;
	if (cfg->txCpltSem == NULL) cfg->txCpltSem = xSemaphoreCreateBinary();
	if (cfg->rxCpltSem == NULL) cfg->rxCpltSem = xSemaphoreCreateBinary();
	if (cfg->txrxCpltSem == NULL) cfg->txrxCpltSem = xSemaphoreCreateBinary();
	if (cfg->mutex == NULL) cfg->mutex = xSemaphoreCreateMutex();
}
static HAL_StatusTypeDef HardSPI_Transmit(const SPI_Device* self, uint8_t *pData, 
		uint16_t Size, uint32_t Timeout)
{
	SPI_config* cfg=self->spi_config;
	if(pdTRUE!=xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))) return HAL_TIMEOUT;
	HAL_StatusTypeDef State_Tx=HAL_SPI_Transmit(cfg->spi_handle,pData,Size,Timeout);
	xSemaphoreGive(cfg->mutex);
	return State_Tx;
}
static HAL_StatusTypeDef HardSPI_Receive(const SPI_Device* self, uint8_t *pData, 
		uint16_t Size, uint32_t Timeout)
{
	SPI_config* cfg=self->spi_config;
	if(pdTRUE!=xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))) return HAL_TIMEOUT;
	HAL_StatusTypeDef State_Tx=HAL_SPI_Receive(cfg->spi_handle,pData,Size,Timeout);
	xSemaphoreGive(cfg->mutex);
	return State_Tx;
}
static HAL_StatusTypeDef HardSPI_TransmitReceive(const SPI_Device* self, uint8_t *pTxData, 
		uint8_t *pRxData, uint16_t Size,uint32_t Timeout)
{
	SPI_config* cfg=self->spi_config;
	if(pdTRUE!=xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))) return HAL_TIMEOUT;
	//HardSPI_SetCS(self,GPIO_PIN_RESET);
	HAL_StatusTypeDef State_Tx=HAL_SPI_TransmitReceive(cfg->spi_handle,pTxData,pRxData,Size,Timeout);
	//HardSPI_SetCS(self,GPIO_PIN_SET);
	xSemaphoreGive(cfg->mutex);
	return State_Tx;
}
////HardSPI1_DMA
static HAL_StatusTypeDef HardSPI_Transmit_DMA(const SPI_Device* self, uint8_t *pData, 
		uint16_t Size, uint32_t Timeout)
{
	SPI_config* cfg=self->spi_config;
	if(pdTRUE!=xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))) return HAL_TIMEOUT;
	HAL_StatusTypeDef SPI_TxState=HAL_SPI_Transmit_DMA(cfg->spi_handle,pData,Size);
	if(SPI_TxState!=HAL_OK)
	{
		xSemaphoreGive(cfg->mutex);
		return SPI_TxState;
	}
	if(cfg->txCpltSem==NULL) while(1);
	BaseType_t xResult=xSemaphoreTake(cfg->txCpltSem,pdMS_TO_TICKS(Timeout));
	if(xResult!=pdTRUE)
	{
		xSemaphoreGive(cfg->mutex);
		return HAL_TIMEOUT;
	}
	xSemaphoreGive(cfg->mutex);
	return HAL_OK;
}
static HAL_StatusTypeDef HardSPI_Receive_DMA(const SPI_Device* self, uint8_t *pData, 
		uint16_t Size, uint32_t Timeout)
{
	SPI_config* cfg=self->spi_config;
	if(pdTRUE!=xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))) return HAL_TIMEOUT;
	HAL_StatusTypeDef SPI_TxState=HAL_SPI_Receive_DMA(cfg->spi_handle,pData,Size);
	if(SPI_TxState!=HAL_OK)
	{
		xSemaphoreGive(cfg->mutex);
		return SPI_TxState;
	}		
	BaseType_t xResult=xSemaphoreTake(cfg->rxCpltSem,pdMS_TO_TICKS(Timeout));
	if(xResult!=pdTRUE)
	{
		xSemaphoreGive(cfg->mutex);
		return HAL_TIMEOUT;
	}
	xSemaphoreGive(cfg->mutex);
	return HAL_OK;
}
static HAL_StatusTypeDef HardSPI_TransmitReceive_DMA(const SPI_Device* self, uint8_t *pTxData, 
		uint8_t *pRxData, uint16_t Size,uint32_t Timeout)
{
	SPI_config* cfg=self->spi_config;
	if(pdTRUE!=xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))) return HAL_TIMEOUT;
	HAL_StatusTypeDef SPI_State=HAL_SPI_TransmitReceive_DMA(cfg->spi_handle,pTxData,pRxData,Size);
	if(SPI_State!=HAL_OK)
	{
		xSemaphoreGive(cfg->mutex);
		return SPI_State;
	}		
	BaseType_t txrxResult=xSemaphoreTake(cfg->txrxCpltSem,pdMS_TO_TICKS(Timeout));
	if(txrxResult!=pdTRUE)
	{
		xSemaphoreGive(cfg->mutex);
		return HAL_TIMEOUT;
	}
	xSemaphoreGive(cfg->mutex);
	return HAL_OK;
}
//Callback
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	SPI_config *cfg = get_spi_config(hspi);
	if (cfg != NULL && cfg->txCpltSem != NULL) 
	{
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(cfg->txCpltSem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
	SPI_config *cfg = get_spi_config(hspi);
	if (cfg != NULL && cfg->rxCpltSem != NULL) 
	{
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(cfg->rxCpltSem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	SPI_config *cfg = get_spi_config(hspi);
	if (cfg != NULL && cfg->txrxCpltSem != NULL) 
	{
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(cfg->txrxCpltSem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}