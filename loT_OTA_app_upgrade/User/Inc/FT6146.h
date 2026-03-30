#ifndef _TOUCH__H
#define _TOUCH__H
//#include "FreeRTOS.h"
//#include "semphr.h"
#include "I2C_OOP.h"
#include <stdbool.h>
#include "main.h"
#define FT6146_ADDRESS_7BIT 0x38

#define TD_STATUS_ADDRESS 0x02
#define P1_XH_ADDRESS 0x03
#define P1_XL_ADDRESS 0x04
#define P1_YH_ADDRESS 0x05
#define P1_YL_ADDRESS 0x06

typedef struct
{
	uint8_t Touch_Num;
	uint16_t Touch_X;
	uint16_t Touch_Y;
}Touch_t;

//extern SemaphoreHandle_t g_xTouchSem;

void FT6146_Init(const I2C_Device* i2c);
HAL_StatusTypeDef FT6146_WriteReg(uint8_t reg, uint8_t data,uint32_t timeout);
HAL_StatusTypeDef FT6146_ReadRegs(uint8_t start_reg, uint8_t *buf, uint8_t len,uint32_t timeout);
bool FT6146_IsTouch();
void FT6146_GetXY(Touch_t* touch);

HAL_StatusTypeDef FT6146_ReadRegs_Poll(uint8_t start_reg, uint8_t *buf, uint8_t len,uint32_t timeout);
void vFT6146_Task(void* arg);

#endif