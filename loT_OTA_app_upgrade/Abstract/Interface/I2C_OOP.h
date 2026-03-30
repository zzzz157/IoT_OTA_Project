#ifndef _I2C_OOP__H
#define _I2C_OOP__H

#include "main.h"
typedef struct I2C_Device_Obj I2C_Device;

typedef struct I2C_Device_Obj
{
	//虚函数表
	void (*Init)(const I2C_Device* self);
	HAL_StatusTypeDef (*WriteReg)(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t Data,uint32_t Timeout);
	HAL_StatusTypeDef (*WriteRegs)(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		const uint8_t *Data, uint16_t Size, uint32_t Timeout);
	HAL_StatusTypeDef (*ReadReg)(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint32_t Timeout);
	HAL_StatusTypeDef (*ReadRegs)(const I2C_Device* self,uint16_t DevAddress,uint8_t RegAddr,
		uint8_t *ReceiveData,uint16_t Size,uint32_t Timeout);
	
	void* i2c_config;
}I2C_Device;

I2C_HandleTypeDef* I2C_GetHandle(I2C_Device* dev);

extern const I2C_Device HardI2C1_Obj;
extern const I2C_Device HardI2C2_Obj;
extern const I2C_Device HardI2C1_IT_Obj;
extern const I2C_Device HardI2C1_DMA_Obj;
extern const I2C_Device SoftI2C1_Obj;

#endif