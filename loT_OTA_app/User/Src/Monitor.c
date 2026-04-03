#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "main.h"
#include "W25Q64.h"
#include "OTA.h"
#include "Monitor.h"

extern IWDG_HandleTypeDef hiwdg;

extern TaskHandle_t xMAX_AcquireTaskHandler,xMAX_CalculateTaskHandler;
extern TaskHandle_t MQTT_Subscribe_TaskHandler,ListenRx_TaskHandler,MQTT_Public_TaskHandler;

volatile uint32_t g_mqtt_heartbeat=0;
volatile uint32_t g_max30102_heartbeat=0;
volatile uint32_t last_tick=0;
void Monitor_Task(void* arg)
{
	LOG_DEBUG("Monitor Task Started");
	for(uint8_t i=0;i<5;i++)
	{
		HAL_IWDG_Refresh(&hiwdg);
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
	uint32_t boot_flag=0;
	W25QHandle_t.ReadDatas(&W25QHandle_t,BOOT_FLAGS_ADDR,(uint8_t*)&boot_flag,sizeof(boot_flag));
	while(1)
	{
		uint32_t current_tick=xTaskGetTickCount();
		uint8_t mqtt_alive=((current_tick-g_mqtt_heartbeat)<MQTT_ALIVE_TICK)?1:0;
		uint8_t max30102_alive=((current_tick-g_max30102_heartbeat)<MAX30102_ALIVE_TICK)?1:0;
		uint8_t stack_mark=((current_tick-last_tick)>STACK_MARK_TICK)?1:0;
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
			LOG_DEBUG("Free Heap Size: %d Bytes", xPortGetFreeHeapSize());
		}
		if(stack_mark)
		{
			last_tick=current_tick;
//			UBaseType_t max_acqu_stack  = uxTaskGetStackHighWaterMark(xMAX_AcquireTaskHandler);
//			UBaseType_t max_calu_stack  = uxTaskGetStackHighWaterMark(xMAX_CalculateTaskHandler);
//			UBaseType_t mqtt_pub_stack  = uxTaskGetStackHighWaterMark(MQTT_Public_TaskHandler);
//			UBaseType_t mqtt_sub_stack  = uxTaskGetStackHighWaterMark(MQTT_Subscribe_TaskHandler);
//			UBaseType_t at_listen_stack  = uxTaskGetStackHighWaterMark(ListenRx_TaskHandler);
//			LOG_DEBUG("max calu Stack WaterMark: %lu words", max_calu_stack);
//			LOG_DEBUG("max acq Stack WaterMark: %lu words", max_acqu_stack);
//			LOG_DEBUG("mqtt pub Stack WaterMark: %lu words", mqtt_pub_stack);
//			LOG_DEBUG("mqtt sub Stack WaterMark: %lu words", mqtt_sub_stack);
//			LOG_DEBUG("at_listen Stack WaterMark: %lu words", at_listen_stack);
			char stats_buffer[512];
			vTaskList(stats_buffer);
			LOG_DEBUG("Task Name\tState\tPrio\tStack\tNum");
			LOG_DEBUG("\n%s", stats_buffer);
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}