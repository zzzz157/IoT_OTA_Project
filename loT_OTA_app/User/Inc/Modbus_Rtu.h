#ifndef __MODBUS_RTU_H
#define __MODBUS_RTU_H

#include "UART_OOP.h"

#define MODBUS_MAX_ADU 256

#define ILLEGAL_FUNCTION 0x01
#define ILLEGAL_ADDRESS  0x02
#define ILLEGAL_DATA 	 0x03

typedef enum
{
	Both_Write_Read=0,
	Only_Read
}Modbus_RegType;

typedef struct {
    uint8_t  (*ReadBit)(uint16_t addr);
    uint8_t  (*ReadInputBit)(uint16_t addr);
    uint16_t (*ReadRegister)(uint16_t addr);
    uint16_t (*ReadInputRegister)(uint16_t addr);
    void     (*WriteBit)(uint16_t addr, uint8_t data);
    void     (*WriteRegister)(uint16_t addr, uint16_t data);
}Modbus_DataHooks;

typedef struct _Modbus_RTU Modbus_RTU;

typedef struct _Modbus_RTU
{
    uint8_t slave_id;       		// 本机从机地址
	void (*Init)(Modbus_RTU* self,uint8_t slave_id,char* uart_name);
	void (*Receive)(Modbus_RTU* self);
	
	Modbus_DataHooks* data_hooks;
	
	void* pData;
}Modbus_RTU;

void Modbus_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

extern Modbus_RTU modbus_rtu_dev1;


#endif