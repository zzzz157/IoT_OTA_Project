#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "W25Q64.h"
#include "OTA.h"
#include "Monitor.h"

extern IWDG_HandleTypeDef hiwdg;

volatile uint32_t g_mqtt_heartbeat=0;
volatile uint32_t g_max30102_heartbeat=0;

void Monitor_Task(void* arg)
{
	LOG_DEBUG("Monitor Task Started");
	uint32_t boot_flag=0;
	W25QHandle_t.ReadDatas(&W25QHandle_t,BOOT_FLAGS_ADDR,(uint8_t*)&boot_flag,sizeof(boot_flag));
	vTaskDelay(pdMS_TO_TICKS(7000));
	while(1)
	{
		uint32_t current_tick=xTaskGetTickCount();
		uint8_t mqtt_alive=((current_tick-g_mqtt_heartbeat)<MQTT_ALIVE_TICK)?1:0;
		uint8_t max30102_alive=((current_tick-g_max30102_heartbeat)<MAX30102_ALIVE_TICK)?1:0;
		if(mqtt_alive&&max30102_alive)
		{
			HAL_IWDG_Refresh(&hiwdg);
			if(BOOT_FLAG_TESTING==boot_flag)
			{
				/* test success */
				OTA_Write_Flash_Flag(BOOT_FLAG_NONE);
				boot_flag=BOOT_FLAG_NONE;
				LOG_DEBUG("OTA Firmware Self-Test Passed! Flag cleared.");
			}
		}
		else
		{
			LOG_DEBUG("System Hung! MQTT:%d,Sensor:%d. Waiting for Watchdog Reset...", 
				mqtt_alive, max30102_alive);
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}