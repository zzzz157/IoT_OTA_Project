#include "main.h"
#include "FreeRTOS.h"
#include "Task.h"
#include "Modbus_Data.h"
#include <stdbool.h>

static uint8_t bits[NUM_BITS]={0x00};
static uint8_t input_bits[NUM_INPUT_BITS]={0x00};
static uint16_t registers[NUM_REGISTERS]={0x00};
static uint16_t input_registers[NUM_INPUT_REGISTERS]={0x00};


void DataCenter_UpdateHealth(uint16_t hr, uint16_t spo2)
{
    taskENTER_CRITICAL();
    input_registers[0] = hr;    // input_registers[0]约定为心率
    input_registers[1] = spo2;  // input_registers[1]约定为血氧
    taskEXIT_CRITICAL();
}
void DataCenter_UpdateSensorError(bool sensor_state)
{
	taskENTER_CRITICAL();
	input_bits[0]=sensor_state; //input_bits[0]约定位传感器采集错误
	taskEXIT_CRITICAL();
}
void DataCenter_UpdateDataLegal(bool is_legal)
{
	taskENTER_CRITICAL();
	input_bits[1]=is_legal; //buf_input_bits[1]约定为数据合法
	taskEXIT_CRITICAL();
}

uint8_t DataCenter_ReadBit(uint16_t addr)
{
    uint16_t val = 0;
    taskENTER_CRITICAL();
    if(addr < NUM_BITS) val = bits[addr];
    taskEXIT_CRITICAL();
    return val;
}
void DataCenter_WriteBit(uint16_t addr,uint8_t data)
{
    taskENTER_CRITICAL();
    bits[addr]=data;
    taskEXIT_CRITICAL();
}
uint8_t DataCenter_ReadInputBit(uint16_t addr)
{
    uint8_t val = 0;
    taskENTER_CRITICAL();
    if(addr < NUM_INPUT_BITS) val = input_bits[addr];
    taskEXIT_CRITICAL();
    return val;
}
uint16_t DataCenter_ReadRegister(uint16_t addr)
{
    uint16_t val = 0;
    taskENTER_CRITICAL();
    if(addr < NUM_REGISTERS) val = registers[addr];
    taskEXIT_CRITICAL();
    return val;
}
void DataCenter_WriteRegister(uint16_t addr,uint16_t data)
{
    taskENTER_CRITICAL();
    registers[addr]=data;
    taskEXIT_CRITICAL();
}
uint16_t DataCenter_ReadInputRegister(uint16_t addr)
{
    uint16_t val = 0;
    taskENTER_CRITICAL();
    if(addr < NUM_INPUT_REGISTERS) val = input_registers[addr];
    taskEXIT_CRITICAL();
    return val;
}