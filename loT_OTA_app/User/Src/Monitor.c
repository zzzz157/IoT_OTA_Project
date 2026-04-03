#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "main.h"
#include "W25Q64.h"
#include "OTA.h"
#include "Monitor.h"
#include "Broker.h"
#include "MAX30102.h"
#include "lfs.h"

extern IWDG_HandleTypeDef hiwdg;

extern TaskHandle_t xMAX_AcquireTaskHandler,xMAX_CalculateTaskHandler;
extern TaskHandle_t MQTT_Subscribe_TaskHandler,ListenRx_TaskHandler,MQTT_Public_TaskHandler;

extern lfs_t lfs;

extern SemaphoreHandle_t g_OfflineSaveSema;

volatile uint32_t g_mqtt_heartbeat=0;
volatile uint32_t g_max30102_heartbeat=0;

static volatile uint32_t last_tick=0;
static QueueHandle_t xOfflineSaveQueue=NULL;
static HealthData_t offline_health[10];
static uint8_t offline_count=0;
void Monitor_Task(void* arg)
{
	LOG_DEBUG("Monitor Task Started");
	xOfflineSaveQueue=xQueueCreate(5,sizeof(HealthData_t));
	Broker_Subscribe(TOPIC_OFFLINE_HEALTH_DATA,xOfflineSaveQueue);
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
			char stats_buffer[512];
			vTaskList(stats_buffer);
			LOG_DEBUG("Task Name\tState\tPrio\tStack\tNum");
			LOG_DEBUG("\n%s", stats_buffer);
		}
		if(pdPASS==xQueueReceive(xOfflineSaveQueue,&offline_health[offline_count],pdMS_TO_TICKS(1000)))
		{
			offline_count++;
			if(offline_count>=HEALTH_OFFLINE_MAX_COUNT)
			{
				offline_count=0;
				/* write into littlefs */
				lfs_file_t file;
				/* LFS_O_WRONLY: 只写模式 */
				/* LFS_O_CREAT: 如果文件不存在，就创建它 */
				/* LFS_O_TRUNC: 如果文件已存在，先清空旧数据 */
				/* LFS_O_APPEND: 追加模式 */
				int err = lfs_file_open(&lfs, &file,HEALTH_OFFLINE_SAVE_FILE,LFS_O_WRONLY|LFS_O_CREAT
						|LFS_O_APPEND);
				if (err == LFS_ERR_OK)
				{
					lfs_file_write(&lfs, &file,offline_health,sizeof(HealthData_t)*HEALTH_OFFLINE_MAX_COUNT);
					lfs_file_close(&lfs, &file);
					xSemaphoreGive(g_OfflineSaveSema);
				}
			}
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
	}
}