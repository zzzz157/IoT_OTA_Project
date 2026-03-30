#include "main.h"
#include "I2C_OOP.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "Task.h"
#include "semphr.h"
static void HardI2C_Init(const I2C_Device* self);
//硬件I2C轮询
static HAL_StatusTypeDef HardI2C_WriteReg(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t Data,uint32_t Timeout);
static HAL_StatusTypeDef HardI2C_WriteRegs(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		const uint8_t *Data, uint16_t Size, uint32_t Timeout);
static HAL_StatusTypeDef HardI2C_ReadReg(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint32_t Timeout);
static HAL_StatusTypeDef HardI2C_ReadRegs(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint16_t Size,uint32_t Timeout);
//硬件I2C_IT
static HAL_StatusTypeDef HardI2C_WriteReg_IT(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t Data,uint32_t Timeout);
static HAL_StatusTypeDef HardI2C_WriteRegs_IT(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		const uint8_t *Data, uint16_t Size, uint32_t Timeout);
static HAL_StatusTypeDef HardI2C_ReadReg_IT(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint32_t Timeout);
static HAL_StatusTypeDef HardI2C_ReadRegs_IT(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint16_t Size,uint32_t Timeout);
//硬件I2C_DMA
static HAL_StatusTypeDef HardI2C_WriteReg_DMA(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t Data,uint32_t Timeout);
static HAL_StatusTypeDef HardI2C_WriteRegs_DMA(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		const uint8_t *Data, uint16_t Size, uint32_t Timeout);
static HAL_StatusTypeDef HardI2C_ReadReg_DMA(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint32_t Timeout);
static HAL_StatusTypeDef HardI2C_ReadRegs_DMA(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint16_t Size,uint32_t Timeout);
//软件I2C
static void SoftI2C_Init(const I2C_Device* self);
static HAL_StatusTypeDef SoftI2C_WriteReg(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t Data,uint32_t Timeout);
static HAL_StatusTypeDef SoftI2C_WriteRegs(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		const uint8_t *Data, uint16_t Size, uint32_t Timeout);
static HAL_StatusTypeDef SoftI2C_ReadReg(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint32_t Timeout);
static HAL_StatusTypeDef SoftI2C_ReadRegs(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint16_t Size,uint32_t Timeout);
typedef struct {
	//共享资源
    I2C_HandleTypeDef* i2c_handle;      // 硬件句柄
    SemaphoreHandle_t mutex;             // 互斥量（所有模式共用）
    SemaphoreHandle_t txCpltSem;         // 发送完成信号量（中断/DMA 用）
    SemaphoreHandle_t rxCpltSem;         // 接收完成信号量（中断/DMA 用）
	//软件I2C
	GPIO_TypeDef* GPIO_Port;
	uint16_t Pin_SCL;
	uint16_t Pin_SDA;
	uint16_t Soft_Delay_Count;
}I2C_config;

//HardI2C1
static I2C_config I2C1_Hardware = {
    .i2c_handle = &hi2c1,
    .mutex = NULL,
    .txCpltSem = NULL,
    .rxCpltSem = NULL,
};
//HardI2C2
static I2C_config I2C2_Hardware = {
    .i2c_handle = &hi2c2,
    .mutex = NULL,
    .txCpltSem = NULL,
    .rxCpltSem = NULL,
};
//SoftI2C1
static I2C_config I2C1_Software = {
	.GPIO_Port=GPIOE,
    .Pin_SCL=GPIO_PIN_2,
	.Pin_SDA=GPIO_PIN_3,
	.Soft_Delay_Count=100,
};
						//======Device_Management=====//
typedef struct {
    I2C_TypeDef* instance;
    I2C_config*  config;
} I2C_MapEntry;
// 映射表 以NULL结尾
static const I2C_MapEntry i2c_map[] = {
    {I2C1, &I2C1_Hardware},
    {I2C2, &I2C2_Hardware},
    {NULL, NULL}// 终止标志
};
static I2C_config* get_i2c_config(I2C_HandleTypeDef* hi2c)
{
	for(uint8_t i=0;i2c_map[i].instance!= NULL; i++)
	{
		if (i2c_map[i].instance == hi2c->Instance)
		{
            return i2c_map[i].config;
        }
	}
	return NULL;
}
//HardI2C1
const I2C_Device HardI2C1_Obj={
	.Init=HardI2C_Init,
	.WriteReg=HardI2C_WriteReg,
	.WriteRegs=HardI2C_WriteRegs,
	.ReadReg=HardI2C_ReadReg,
	.ReadRegs=HardI2C_ReadRegs,
	.i2c_config=&I2C1_Hardware,
};
//HardI2C2
const I2C_Device HardI2C2_Obj={
	.Init=HardI2C_Init,
	.WriteReg=HardI2C_WriteReg,
	.WriteRegs=HardI2C_WriteRegs,
	.ReadReg=HardI2C_ReadReg,
	.ReadRegs=HardI2C_ReadRegs,
	.i2c_config=&I2C2_Hardware,
};
//HardI2C1_IT
const I2C_Device HardI2C1_IT_Obj={
	.Init=HardI2C_Init,
	.WriteReg=HardI2C_WriteReg_IT,
	.WriteRegs=HardI2C_WriteRegs_IT,
	.ReadReg=HardI2C_ReadReg_IT,
	.ReadRegs=HardI2C_ReadRegs_IT,
	.i2c_config=&I2C1_Hardware,
};
//HardI2C1_DMA
const I2C_Device HardI2C1_DMA_Obj={
	.Init=HardI2C_Init,
	.WriteReg=HardI2C_WriteReg_DMA,
	.WriteRegs=HardI2C_WriteRegs_DMA,
	.ReadReg=HardI2C_ReadReg_DMA,
	.ReadRegs=HardI2C_ReadRegs_DMA,
	.i2c_config=&I2C1_Hardware,
};
//SoftI2C1
const I2C_Device SoftI2C1_Obj={
	.Init=SoftI2C_Init,
	.WriteReg=SoftI2C_WriteReg,
	.WriteRegs=SoftI2C_WriteRegs,
	.ReadReg=SoftI2C_ReadReg,
	.ReadRegs=SoftI2C_ReadRegs,
	.i2c_config=&I2C1_Software,
};
						//========Hardware_I2C=====//
static void HardI2C_ErrorHandler(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->hdmatx != NULL)
    {
        HAL_DMA_Abort(hi2c->hdmatx);
    }
    if (hi2c->hdmarx != NULL) 
    {
        HAL_DMA_Abort(hi2c->hdmarx);
    }

    __HAL_I2C_DISABLE(hi2c); // 强行关闭 I2C 外设

    volatile uint32_t delay = 1000;
    while(delay--);

//    HAL_I2C_DeInit(hi2c);
//    HAL_I2C_Init(hi2c);

    hi2c->State = HAL_I2C_STATE_READY;
    hi2c->Mode  = HAL_I2C_MODE_NONE;
}
static void HardI2C_Init(const I2C_Device* self)
{
	I2C_config *cfg=self->i2c_config;
	if(cfg->txCpltSem==NULL)
	{
		cfg->txCpltSem=xSemaphoreCreateBinary();
		xSemaphoreTake(cfg->txCpltSem, 0);
	}
	if(cfg->rxCpltSem==NULL)
	{
		cfg->rxCpltSem=xSemaphoreCreateBinary();
		xSemaphoreTake(cfg->rxCpltSem, 0);
	}
	if(cfg->mutex==NULL)
	{
		cfg->mutex=xSemaphoreCreateMutex();
	}
}
static HAL_StatusTypeDef HardI2C_WriteReg(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t Data,uint32_t Timeout)
{
	I2C_config *cfg=self->i2c_config;
	if(xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))!=pdTRUE) return HAL_TIMEOUT;
	HAL_StatusTypeDef HardI2C_State=HAL_I2C_Mem_Write(cfg->i2c_handle,DevAddress<<1,
		RegAddr,I2C_MEMADD_SIZE_8BIT ,&Data,1,Timeout);
	if(HardI2C_State != HAL_OK)
    {
        HardI2C_ErrorHandler(cfg->i2c_handle);
    }
	xSemaphoreGive(cfg->mutex);
	return HardI2C_State;
}
static HAL_StatusTypeDef HardI2C_WriteRegs(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		const uint8_t *Data, uint16_t Size, uint32_t Timeout)
{
	I2C_config *cfg=self->i2c_config;
	if(xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))!=pdTRUE) return HAL_TIMEOUT;
	HAL_StatusTypeDef HardI2C_State=HAL_I2C_Mem_Write(cfg->i2c_handle,DevAddress<<1,
		RegAddr,I2C_MEMADD_SIZE_8BIT,(uint8_t *)Data,Size,Timeout);
	if(HardI2C_State != HAL_OK)
    {
        HardI2C_ErrorHandler(cfg->i2c_handle);
    }
	xSemaphoreGive(cfg->mutex);
	return HardI2C_State;
}
static HAL_StatusTypeDef HardI2C_ReadReg(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint32_t Timeout)
{
	I2C_config *cfg=self->i2c_config;
	if(xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))!=pdTRUE) return HAL_TIMEOUT;
	HAL_StatusTypeDef HardI2C_State=HAL_I2C_Mem_Read(cfg->i2c_handle,DevAddress<<1,
		RegAddr,I2C_MEMADD_SIZE_8BIT,ReceiveData,1,Timeout);\
	if(HardI2C_State != HAL_OK)
    {
        HardI2C_ErrorHandler(cfg->i2c_handle);
    }
	xSemaphoreGive(cfg->mutex);
	return HardI2C_State;
}
static HAL_StatusTypeDef HardI2C_ReadRegs(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint16_t Size,uint32_t Timeout)
{
	I2C_config *cfg=self->i2c_config;
	if(xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))!=pdTRUE) return HAL_TIMEOUT;
	HAL_StatusTypeDef HardI2C_State=HAL_I2C_Mem_Read(cfg->i2c_handle,DevAddress<<1,
		RegAddr,I2C_MEMADD_SIZE_8BIT,ReceiveData,Size,Timeout);
	if(HardI2C_State != HAL_OK)
    {
        HardI2C_ErrorHandler(cfg->i2c_handle);
    }
	xSemaphoreGive(cfg->mutex);
	return HardI2C_State;
}
						//========Hardware_I2C_IT=====//
static HAL_StatusTypeDef HardI2C_WriteReg_IT(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t Data,uint32_t Timeout)
{
	I2C_config *cfg=self->i2c_config;
	if(xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))!=pdTRUE) return HAL_TIMEOUT;
	HAL_StatusTypeDef HardI2C_State=HAL_I2C_Mem_Write_IT(cfg->i2c_handle,
		DevAddress<<1,RegAddr,I2C_MEMADD_SIZE_8BIT ,&Data,1);
	if(HardI2C_State == HAL_OK)
    {
        BaseType_t xResult = xSemaphoreTake(cfg->txCpltSem,pdMS_TO_TICKS(Timeout));
        if(xResult != pdTRUE)
        {
            HardI2C_ErrorHandler(cfg->i2c_handle);
            HardI2C_State = HAL_TIMEOUT;
        }
    }
    else
    {
        HardI2C_ErrorHandler(cfg->i2c_handle);
    }
	xSemaphoreGive(cfg->mutex);
	return HardI2C_State;
}
static HAL_StatusTypeDef HardI2C_WriteRegs_IT(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		const uint8_t *Data, uint16_t Size, uint32_t Timeout)
{
	I2C_config *cfg=self->i2c_config;
	if(xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))!=pdTRUE) return HAL_TIMEOUT;
	HAL_StatusTypeDef HardI2C_State=HAL_I2C_Mem_Write_IT(cfg->i2c_handle,DevAddress<<1,
		RegAddr,I2C_MEMADD_SIZE_8BIT,(uint8_t *)Data,Size);
	if(HardI2C_State == HAL_OK)
    {
        BaseType_t xResult = xSemaphoreTake(cfg->txCpltSem,pdMS_TO_TICKS(Timeout));
        if(xResult != pdTRUE)
        {
            HardI2C_ErrorHandler(cfg->i2c_handle);
            HardI2C_State = HAL_TIMEOUT;
        }
    }
    else
    {
        HardI2C_ErrorHandler(cfg->i2c_handle);
    }
	xSemaphoreGive(cfg->mutex);
	return HardI2C_State;
}
static HAL_StatusTypeDef HardI2C_ReadReg_IT(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint32_t Timeout)
{
	I2C_config *cfg=self->i2c_config;
	if(xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))!=pdTRUE) return HAL_TIMEOUT;
	HAL_StatusTypeDef HardI2C_State=HAL_I2C_Mem_Read_IT(cfg->i2c_handle,DevAddress<<1,
		RegAddr,I2C_MEMADD_SIZE_8BIT,ReceiveData,1);
	if(HardI2C_State == HAL_OK)
    {
        BaseType_t xResult = xSemaphoreTake(cfg->rxCpltSem,pdMS_TO_TICKS(Timeout));
        if(xResult != pdTRUE)
        {
            HardI2C_ErrorHandler(cfg->i2c_handle);
            HardI2C_State = HAL_TIMEOUT;
        }
    }
    else
    {
        HardI2C_ErrorHandler(cfg->i2c_handle);
    }
	xSemaphoreGive(cfg->mutex);
	return HardI2C_State;
}
static HAL_StatusTypeDef HardI2C_ReadRegs_IT(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint16_t Size,uint32_t Timeout)
{
	I2C_config *cfg=self->i2c_config;
	if(xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))!=pdTRUE) return HAL_TIMEOUT;
	HAL_StatusTypeDef HardI2C_State=HAL_I2C_Mem_Read_IT(cfg->i2c_handle,DevAddress<<1,
		RegAddr,I2C_MEMADD_SIZE_8BIT,ReceiveData,Size);
	if(HardI2C_State == HAL_OK)
    {
        BaseType_t xResult = xSemaphoreTake(cfg->rxCpltSem,pdMS_TO_TICKS(Timeout));
        if(xResult != pdTRUE)
        {
            HardI2C_ErrorHandler(cfg->i2c_handle);
            HardI2C_State = HAL_TIMEOUT;
        }
    }
    else
    {
        HardI2C_ErrorHandler(cfg->i2c_handle);
    }
	xSemaphoreGive(cfg->mutex);
	return HardI2C_State;
}
						//========Hardware_I2C_DMA=====//
static HAL_StatusTypeDef HardI2C_WriteReg_DMA(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t Data,uint32_t Timeout)
{
	I2C_config *cfg=self->i2c_config;
	if(xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))!=pdTRUE) return HAL_TIMEOUT;
	HAL_StatusTypeDef I2C_TxState=HAL_I2C_Mem_Write_DMA(cfg->i2c_handle,
		DevAddress<<1,RegAddr,I2C_MEMADD_SIZE_8BIT ,&Data,1);
	if(I2C_TxState == HAL_OK)
    {
        BaseType_t xResult = xSemaphoreTake(cfg->txCpltSem,pdMS_TO_TICKS(Timeout));
        if(xResult != pdTRUE)
        {
            HardI2C_ErrorHandler(cfg->i2c_handle);
            I2C_TxState = HAL_TIMEOUT;
        }
    }
    else
    {
        HardI2C_ErrorHandler(cfg->i2c_handle);
    }
	xSemaphoreGive(cfg->mutex);
	return I2C_TxState;
}
static HAL_StatusTypeDef HardI2C_WriteRegs_DMA(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		const uint8_t *Data, uint16_t Size, uint32_t Timeout)
{
	I2C_config *cfg=self->i2c_config;
	if(xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))!=pdTRUE) return HAL_TIMEOUT;
	HAL_StatusTypeDef I2C_TxState=HAL_I2C_Mem_Write_DMA(cfg->i2c_handle,DevAddress<<1,
		RegAddr,I2C_MEMADD_SIZE_8BIT,(uint8_t *)Data,Size);
	if(I2C_TxState == HAL_OK)
    {
        BaseType_t xResult = xSemaphoreTake(cfg->txCpltSem,pdMS_TO_TICKS(Timeout));
        if(xResult != pdTRUE)
        {
            HardI2C_ErrorHandler(cfg->i2c_handle);
            I2C_TxState = HAL_TIMEOUT;
        }
    }
    else
    {
        HardI2C_ErrorHandler(cfg->i2c_handle);
    }
	xSemaphoreGive(cfg->mutex);
	return I2C_TxState;
}
static HAL_StatusTypeDef HardI2C_ReadReg_DMA(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint32_t Timeout)
{
	I2C_config *cfg=self->i2c_config;
	if(xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))!=pdTRUE) return HAL_TIMEOUT;
	HAL_StatusTypeDef HardI2C_State=HAL_I2C_Mem_Read_DMA(cfg->i2c_handle,DevAddress<<1,
		RegAddr,I2C_MEMADD_SIZE_8BIT,ReceiveData,1);
	if(HardI2C_State == HAL_OK)
    {
        BaseType_t xResult = xSemaphoreTake(cfg->rxCpltSem,pdMS_TO_TICKS(Timeout));
        if(xResult != pdTRUE)
        {
            HardI2C_ErrorHandler(cfg->i2c_handle);
            HardI2C_State = HAL_TIMEOUT;
        }
    }
    else
    {
        HardI2C_ErrorHandler(cfg->i2c_handle);
    }
	xSemaphoreGive(cfg->mutex);
	return HardI2C_State;
}
static HAL_StatusTypeDef HardI2C_ReadRegs_DMA(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint16_t Size,uint32_t Timeout)
{
	I2C_config *cfg=self->i2c_config;
	if(xSemaphoreTake(cfg->mutex,pdMS_TO_TICKS(Timeout))!=pdTRUE) return HAL_TIMEOUT;
	xSemaphoreTake(cfg->rxCpltSem, 0);
	HAL_StatusTypeDef HardI2C_State=HAL_I2C_Mem_Read_DMA(cfg->i2c_handle,DevAddress<<1,
		RegAddr,I2C_MEMADD_SIZE_8BIT,ReceiveData,Size);
	if(HardI2C_State == HAL_OK)
    {
        BaseType_t xResult = xSemaphoreTake(cfg->rxCpltSem,pdMS_TO_TICKS(Timeout));
        if(xResult != pdTRUE)
        {
            HardI2C_ErrorHandler(cfg->i2c_handle);
            HardI2C_State = HAL_TIMEOUT;
        }
    }
    else
    {
        HardI2C_ErrorHandler(cfg->i2c_handle);
    }
	xSemaphoreGive(cfg->mutex);
	return HardI2C_State;
}
//Callback
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	I2C_config *cfg = get_i2c_config(hi2c);
	if (cfg != NULL && cfg->txCpltSem != NULL) 
	{
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(cfg->txCpltSem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	I2C_config *cfg = get_i2c_config(hi2c);
	if (cfg != NULL && cfg->rxCpltSem != NULL) 
	{
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(cfg->rxCpltSem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
	
}
						//====Software-emulated I2C====
static void I2C_Delay(const I2C_Device* self)
{
    I2C_config* cfg = self->i2c_config;
    uint32_t i = cfg->Soft_Delay_Count; 
    while(i--) { __NOP(); }
}
static void SoftI2C_Write_SCL(const I2C_Device* self,GPIO_PinState PinState)
{
	I2C_config* cfg=self->i2c_config;
	HAL_GPIO_WritePin(cfg->GPIO_Port,cfg->Pin_SCL,PinState);
}

static void SoftI2C_Write_SDA(const I2C_Device* self,GPIO_PinState PinState)
{
	I2C_config* cfg=self->i2c_config;
	HAL_GPIO_WritePin(cfg->GPIO_Port,cfg->Pin_SDA,PinState);
}

static uint8_t SoftI2C_Read_SDA(const I2C_Device* self)
{
	I2C_config* cfg=self->i2c_config;
	return HAL_GPIO_ReadPin(cfg->GPIO_Port,cfg->Pin_SDA);
}
//
static void SoftI2C_Start(const I2C_Device* self)
{
	SoftI2C_Write_SDA(self,1);
	I2C_Delay(self);
	SoftI2C_Write_SCL(self,1);
	I2C_Delay(self);
	SoftI2C_Write_SDA(self,0);
	I2C_Delay(self);
	SoftI2C_Write_SCL(self,0);
	I2C_Delay(self);
}

static void SoftI2C_Stop(const I2C_Device* self)
{
	SoftI2C_Write_SDA(self,0);
	I2C_Delay(self);
	SoftI2C_Write_SCL(self,1);
	I2C_Delay(self);
	SoftI2C_Write_SDA(self,1);
	I2C_Delay(self);
}

static void SoftI2C_SendByte(const I2C_Device* self,uint8_t Byte)
{
	uint8_t i;
	for(i=0;i<8;i++)
	{
		SoftI2C_Write_SDA(self,!!((0x80>>i) & Byte));
		I2C_Delay(self);
		SoftI2C_Write_SCL(self,1);
		I2C_Delay(self);
		SoftI2C_Write_SCL(self,0);
		I2C_Delay(self);
	}
}

static uint8_t SoftI2C_ReceiveByte(const I2C_Device* self)
{
	uint8_t i,Byte=0x00;
	SoftI2C_Write_SDA(self,1);
	I2C_Delay(self);
	for(i=0;i<8;i++)
	{
		SoftI2C_Write_SCL(self,1);
		I2C_Delay(self);
		if( SoftI2C_Read_SDA(self) )
		{
			Byte|=(0x80>>i);
		}
	    SoftI2C_Write_SCL(self,0);
		I2C_Delay(self);
	}
	return Byte;
}

static void SoftI2C_SendAck(const I2C_Device* self,uint8_t Ackbit)
{
	SoftI2C_Write_SDA(self,Ackbit);
	I2C_Delay(self);
	SoftI2C_Write_SCL(self,1);
	I2C_Delay(self);
	SoftI2C_Write_SCL(self,0);
	I2C_Delay(self);
}

static uint8_t SoftI2C_ReceiveAck(const I2C_Device* self)
{
	uint8_t Ackbit=1;
	SoftI2C_Write_SDA(self,1);
	I2C_Delay(self);
	SoftI2C_Write_SCL(self,1);
	I2C_Delay(self);
	Ackbit=SoftI2C_Read_SDA(self);
	SoftI2C_Write_SCL(self,0);
	I2C_Delay(self);
	return Ackbit;
}
							//======Software_I2C=====//
static void SoftI2C_Init(const I2C_Device* self)
{
	
}
static HAL_StatusTypeDef SoftI2C_WriteReg(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t Data,uint32_t Timeout)
{
	SoftI2C_Start(self);
	SoftI2C_SendByte(self,DevAddress<<1|0);
	if(SoftI2C_ReceiveAck(self)){ SoftI2C_Stop(self); return HAL_ERROR;}
	SoftI2C_SendByte(self,RegAddr);
	if(SoftI2C_ReceiveAck(self)){ SoftI2C_Stop(self); return HAL_ERROR;}
	SoftI2C_SendByte(self,Data);
	if(SoftI2C_ReceiveAck(self)){ SoftI2C_Stop(self); return HAL_ERROR;}
	SoftI2C_Stop(self);
	return HAL_OK;
}
static HAL_StatusTypeDef SoftI2C_WriteRegs(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		const uint8_t *Data, uint16_t Size, uint32_t Timeout)
{
	SoftI2C_Start(self);
	SoftI2C_SendByte(self,DevAddress<<1|0);
	if(SoftI2C_ReceiveAck(self)){ SoftI2C_Stop(self); return HAL_ERROR;}
	SoftI2C_SendByte(self,RegAddr);
	if(SoftI2C_ReceiveAck(self)){ SoftI2C_Stop(self); return HAL_ERROR;}
	for(uint8_t i=0;i<Size;i++)
	{
		SoftI2C_SendByte(self,Data[i]);
		if(SoftI2C_ReceiveAck(self)){ SoftI2C_Stop(self); return HAL_ERROR;}
	}
	SoftI2C_Stop(self);
	return HAL_OK;
}
static HAL_StatusTypeDef SoftI2C_ReadReg(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint32_t Timeout)
{
	SoftI2C_Start(self);
	SoftI2C_SendByte(self,DevAddress<<1|0);
	if(SoftI2C_ReceiveAck(self)){ SoftI2C_Stop(self); return HAL_ERROR;}
	SoftI2C_SendByte(self,RegAddr);
	if(SoftI2C_ReceiveAck(self)){ SoftI2C_Stop(self); return HAL_ERROR;}
	
	SoftI2C_Start(self);
	SoftI2C_SendByte(self,DevAddress<<1|1);
	if(SoftI2C_ReceiveAck(self)){ SoftI2C_Stop(self); return HAL_ERROR;}
	if(ReceiveData!=NULL)
	{
		*ReceiveData=SoftI2C_ReceiveByte(self);
	}
	SoftI2C_SendAck(self,1);
	SoftI2C_Stop(self);
	return HAL_OK;
}
static HAL_StatusTypeDef SoftI2C_ReadRegs(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint16_t Size,uint32_t Timeout)
{
	SoftI2C_Start(self);
	SoftI2C_SendByte(self,DevAddress<<1|0);
	if(SoftI2C_ReceiveAck(self)){ SoftI2C_Stop(self); return HAL_ERROR;}
	SoftI2C_SendByte(self,RegAddr);
	if(SoftI2C_ReceiveAck(self)){ SoftI2C_Stop(self); return HAL_ERROR;}
	
	SoftI2C_Start(self);
	SoftI2C_SendByte(self,DevAddress<<1|1);
	if(SoftI2C_ReceiveAck(self)){ SoftI2C_Stop(self); return HAL_ERROR;}
	for(uint8_t i=0;i<Size;i++)
	{
		ReceiveData[i]=SoftI2C_ReceiveByte(self);
		if(i<Size-1)
		{
			SoftI2C_SendAck(self,0);
		}
	}
	SoftI2C_SendAck(self,1);
	SoftI2C_Stop(self);
	return HAL_OK;
}
								//====对外接口====//
//I2C_Handle
I2C_HandleTypeDef* I2C_GetHandle(I2C_Device* dev) 
{
    if (!dev || !dev->i2c_config) return NULL;
    I2C_config* cfg = (I2C_config*)dev->i2c_config;
    return cfg->i2c_handle;
}