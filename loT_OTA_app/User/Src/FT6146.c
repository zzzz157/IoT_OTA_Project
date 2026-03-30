#include "main.h"
#include "I2C_OOP.h"
#include "FT6146.h"
#include "ST7789V.h"
#include "FreeRTOS.h"
#include "Task.h"
#include "semphr.h"
//#include "OLED.h"
#include <stdbool.h>

static const I2C_Device* FT6146_I2C=NULL;
//SemaphoreHandle_t g_xTouchSem=NULL;

static void FT66146_SetRST(GPIO_PinState PinState)
{
	HAL_GPIO_WritePin(GPIOC,GPIO_PIN_5,PinState);
}

void FT6146_Init(const I2C_Device* i2c)
{
	if(i2c==NULL) return ;
	FT6146_I2C=i2c;
	FT6146_I2C->Init(FT6146_I2C);
	//g_xTouchSem=xSemaphoreCreateBinary();
	//复位
	FT66146_SetRST(GPIO_PIN_RESET);
	HAL_Delay(20);
	FT66146_SetRST(GPIO_PIN_SET);
	HAL_Delay(150);
}

HAL_StatusTypeDef FT6146_WriteReg(uint8_t reg, uint8_t data,uint32_t timeout)
{
	if(FT6146_I2C==NULL) return HAL_ERROR;
	HAL_StatusTypeDef hal_state=FT6146_I2C->WriteReg(FT6146_I2C,FT6146_ADDRESS_7BIT,reg,data,timeout);
	return hal_state;
}
HAL_StatusTypeDef FT6146_ReadRegs(uint8_t start_reg, uint8_t *buf, uint8_t len,uint32_t timeout)
{
	if(FT6146_I2C==NULL) return HAL_ERROR;
	HAL_StatusTypeDef hal_state=FT6146_I2C->ReadRegs(FT6146_I2C,FT6146_ADDRESS_7BIT,start_reg,
		buf,len,timeout);
	return hal_state;
}
static void FT6146_Process(uint8_t data[5],Touch_t* touch)
{
	touch->Touch_Num=data[0]&0x0F;
	touch->Touch_X=((data[1]&0x0F)<<8)|data[2];
	touch->Touch_Y=((data[3]&0x0F)<<8)|data[4];
}
static uint8_t Touch_Data[5];
Touch_t touch;
//void vFT6146_Task(void* arg)
//{
//	FT6146_Init(&HardI2C1_DMA_Obj);
//	while(1)
//	{
//		//xSemaphoreTake(g_xTouchSem,portMAX_DELAY);
//		if(FT6146_ReadRegs(TD_STATUS_ADDRESS,Touch_Data,5,10)==HAL_OK)
//		{
//			FT6146_Process(Touch_Data,&touch);
//			if(touch.Touch_Num > 0)
//			{
//				OLED_ShowNum(1, 1, touch.Touch_Num, 2);
//				OLED_ShowNum(2, 1, touch.Touch_X, 4);
//				OLED_ShowNum(3, 1, touch.Touch_Y, 4);
//			}
//		}
//	}
//}