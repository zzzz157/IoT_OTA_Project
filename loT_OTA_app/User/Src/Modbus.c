#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "MAX30102.h"
#include "Modbus_Rtu.h"
#include "Broker.h"
#include "DataCenter.h"
#include "SysParam.h"
static Modbus_DataHooks my_app_hooks = {
    .ReadBit           = DataCenter_ReadBit,
    .ReadInputBit      = DataCenter_ReadInputBit,
    .ReadRegister      = DataCenter_ReadRegister,
    .ReadInputRegister = DataCenter_ReadInputRegister,
    .WriteBit          = DataCenter_WriteBit,
    .WriteRegister     = DataCenter_WriteRegister
};
Modbus_RTU* mosbus_dev=NULL;
static QueueHandle_t xModbusQue_Health=NULL;
static void Modbus_Data_Timer_Callback(TimerHandle_t xTimer)
{
	if(xModbusQue_Health==NULL) return;
	static HealthData_t rx_healthdata={.confidence=0};
	BaseType_t has_new_data = pdFALSE;
	while(xQueueReceive(xModbusQue_Health, &rx_healthdata, 0) == pdTRUE)
	{
		has_new_data=pdTRUE;
	}
	if(has_new_data==pdTRUE)
	{
		DataCenter_UpdateHealth(rx_healthdata.HeartRate_Value,rx_healthdata.Spo2_Value);
	}
	else
	{
		DataCenter_UpdateDataLegal(rx_healthdata.confidence);
	}
}
void Modbus_Task(void* arg)
{
	LOG_DEBUG("Modbus Task");
	xModbusQue_Health = xQueueCreate(2, sizeof(HealthData_t));
    Broker_Subscribe(TOPIC_HEALTH_DATA, xModbusQue_Health);
	TimerHandle_t xTimer = xTimerCreate("MbTimer", pdMS_TO_TICKS(100), pdTRUE, NULL, 
		Modbus_Data_Timer_Callback);
    xTimerStart(xTimer, 0);
	mosbus_dev=&modbus_rtu_dev1;
	mosbus_dev->data_hooks = &my_app_hooks;
	mosbus_dev->Init(mosbus_dev,g_SysParam.modbus_id,"uart1_it");
	LOG_DEBUG("Modbus init");
	while(1)
	{
		mosbus_dev->Receive(mosbus_dev);
	}
}