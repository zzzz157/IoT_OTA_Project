#include "AT_Command.h"
#include "ESP8266.h"
#include "Socket.h"
#include "MQTT_Client.h"
#include "OLED.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "main.h"
#include "Broker.h"
#include "OTA.h"
#include "MAX30102.h"
#include "Broker.h"
#include "cJSON.h"
#include "SysParam.h"
#include "Monitor.h"
#include "lfs.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

extern volatile uint8_t g_mqtt_ping_waiting;
extern volatile uint32_t g_mqtt_heartbeat;

extern lfs_t lfs;

volatile int g_mqtt_sockfd = -1;
static QueueHandle_t xMQTTQue_HealthData;
SemaphoreHandle_t g_OfflineSaveSema=NULL;
/* 发布 */
void MQTT_Public_Task(void* arg)
{
	g_OfflineSaveSema=xSemaphoreCreateBinary();
	xMQTTQue_HealthData=xQueueCreate(5,sizeof(HealthData_t));
	Broker_Subscribe(TOPIC_HEALTH_DATA,xMQTTQue_HealthData);
	HealthData_t mqtt_HrSpo2;
	while(1)
	{
		BaseType_t has_new_data = pdFALSE;
		while(xQueueReceive(xMQTTQue_HealthData, &mqtt_HrSpo2, 0) == pdTRUE)
		{
			if(mqtt_HrSpo2.confidence==1)
			{
				has_new_data = pdTRUE;
			}
		}
		if(g_mqtt_sockfd>=0&&socket_status(g_mqtt_sockfd)==1)
		{
			/* reconnect upload */
			if(pdPASS==xSemaphoreTake(g_OfflineSaveSema,0))
			{
				lfs_file_t file;
				int err = lfs_file_open(&lfs, &file,HEALTH_OFFLINE_SAVE_FILE,LFS_O_RDONLY);
				if (err == LFS_ERR_OK)
				{
					HealthData_t history_data;
					while(lfs_file_read(&lfs,&file,&history_data,sizeof(HealthData_t))==
							sizeof(HealthData_t))
					{
						cJSON* cjson_health=cJSON_CreateObject();
						cJSON_AddStringToObject(cjson_health, "device", "stm32f407");
						cJSON* cjson_data=cJSON_CreateObject();
						cJSON_AddNumberToObject(cjson_data,"hr",history_data.HeartRate_Value);
						cJSON_AddNumberToObject(cjson_data,"spo2",history_data.Spo2_Value);
						cJSON_AddNumberToObject(cjson_data,"valid",history_data.confidence);
						cJSON_AddItemToObject(cjson_health,"sensor",cjson_data);
						cJSON_AddNumberToObject(cjson_health,"timestamp",history_data.timestamp_ms);
						cJSON_AddNumberToObject(cjson_health,"history",1);
						char* telemetry=cJSON_PrintUnformatted(cjson_health);
						if(telemetry!=NULL)
						{
							MQTT_Publish(g_mqtt_sockfd,"qrszx/telemetry",telemetry);
							cJSON_free(telemetry);
						}
						cJSON_Delete(cjson_health);
						g_mqtt_heartbeat = xTaskGetTickCount();
						vTaskDelay(50);
					}
					lfs_remove(&lfs,HEALTH_OFFLINE_SAVE_FILE);
					lfs_file_close(&lfs, &file);
				}
			}
			/* vaild data */
			if(has_new_data ==pdTRUE)
			{
				cJSON* cjson_health=cJSON_CreateObject();
				
				cJSON_AddStringToObject(cjson_health, "device", "stm32f407");
				
				cJSON* cjson_data=cJSON_CreateObject();
				cJSON_AddNumberToObject(cjson_data,"hr",mqtt_HrSpo2.HeartRate_Value);
				cJSON_AddNumberToObject(cjson_data,"spo2",mqtt_HrSpo2.Spo2_Value);
				cJSON_AddNumberToObject(cjson_data,"valid",mqtt_HrSpo2.confidence);
				cJSON_AddItemToObject(cjson_health,"sensor",cjson_data);
		
				cJSON_AddNumberToObject(cjson_health,"timestamp",mqtt_HrSpo2.timestamp_ms);
				
				char* telemetry=cJSON_PrintUnformatted(cjson_health);
				if(telemetry!=NULL)
				{
					MQTT_Publish(g_mqtt_sockfd,"qrszx/telemetry",telemetry);
					cJSON_free(telemetry);
				}
				cJSON_Delete(cjson_health);
				
			}
		}
		/* offline save */
		else
		{
			Broker_Publish(TOPIC_OFFLINE_HEALTH_DATA,&mqtt_HrSpo2);
		}
		vTaskDelay(pdMS_TO_TICKS(3000));
	}
}
/* 指定主题回调函数 */
static void my_mqtt_cmd_handler(const char* payload, uint16_t len)
{
	static char message[256] = {0};
    uint16_t copy_len = (len >= sizeof(message)) ? sizeof(message)-1 : len;
    memcpy(message, payload, copy_len);
    message[copy_len] = '\0';
	LOG_DEBUG("%s",message);
	cJSON* cjson_head = cJSON_Parse(message);
	if(cjson_head == NULL)
	{
		LOG_DEBUG("parse fail");
		return;
	}
	cJSON* cjson_cmd=cJSON_GetObjectItem(cjson_head,"cmd");
	if (cjson_cmd != NULL && cJSON_IsString(cjson_cmd))
	{
		/* cmd Dispatcher */
		if (strcmp(cjson_cmd->valuestring, "OTA") == 0)
		{
			/* deal ota command */
			LOG_DEBUG("Trigger OTA process");
			cJSON* cjson_ip=cJSON_GetObjectItem(cjson_head,"ip");
			cJSON* cjson_port=cJSON_GetObjectItem(cjson_head,"port");
			cJSON* cjson_path=cJSON_GetObjectItem(cjson_head,"path");
			if (cjson_ip && cJSON_IsString(cjson_ip) && cjson_port && cJSON_IsNumber(cjson_port)
                && cjson_path && cJSON_IsString(cjson_path))
			{
				OTA_Config_t ota_cmd;
                int ip[4];
				if (sscanf(cjson_ip->valuestring,"%d.%d.%d.%d",&ip[0],&ip[1],&ip[2],&ip[3])==4)
				{
					ota_cmd.ip[0]=ip[0];
					ota_cmd.ip[1]=ip[1];
					ota_cmd.ip[2]=ip[2];
					ota_cmd.ip[3]=ip[3];
					ota_cmd.port=cjson_port->valueint;
					strcpy(ota_cmd.path, cjson_path->valuestring);
					Broker_Publish(TOPIC_OTA_DATA,&ota_cmd);
				}
			}
			else
            {
                LOG_DEBUG("OTA IP format error!");
            }
		}
		/* else if */
		else
		{
			LOG_DEBUG("JSON command not find");
		}
	}
	else
	{
		LOG_DEBUG("JSON missing 'cmd' or 'cmd' is not string");
	}
	cJSON_Delete(cjson_head);
}
/* 订阅 */
void MQTT_Subscribe_Task(void *pvParameters)
{
	int fd_mqtt;
	while(1)
    {
		g_mqtt_heartbeat = xTaskGetTickCount();
		int result=init(&at_esp8266,g_SysParam.wifi_ssid,g_SysParam.wifi_pwd);	/* 联网 */
		if(result != 0)
		{
			g_mqtt_heartbeat = xTaskGetTickCount();
			LOG_DEBUG("ESP8266 Init Failed! Rebooting task...");
			vTaskDelay(pdMS_TO_TICKS(3000));
			continue;
		}
		LOG_DEBUG("init OK");
        fd_mqtt = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_mqtt < 0)
		{
			g_mqtt_heartbeat = xTaskGetTickCount();
			LOG_DEBUG("socket ERR");
            vTaskDelay(pdMS_TO_TICKS(3000)); 
            continue;
        }
		g_mqtt_sockfd=fd_mqtt;
		LOG_DEBUG("socket OK");
		
		g_mqtt_heartbeat = xTaskGetTickCount();
		
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(g_SysParam.mqtt_port);
        server_addr.sin_addr.s_addr = IP4_ADDR(g_SysParam.mqtt_ip[0],g_SysParam.mqtt_ip[1],
			g_SysParam.mqtt_ip[2],g_SysParam.mqtt_ip[3]);

        if (connect(fd_mqtt, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
        {
			LOG_DEBUG("connect OK");
			vTaskDelay(pdMS_TO_TICKS(100));
            if (MQTT_Connect(fd_mqtt, "STM32_Client_01") > 0)
			{
				g_mqtt_heartbeat = xTaskGetTickCount();
				LOG_DEBUG("MQTT Connect Packet Sent!");
                vTaskDelay(100);
				MQTT_Subscribe("qrszx/command", my_mqtt_cmd_handler, fd_mqtt);
                LOG_DEBUG("Subscribed to stm32");
				uint8_t rx_buf[512];
				int rx_idx=0;
				g_mqtt_ping_waiting = 0;
				uint32_t last_tick = xTaskGetTickCount();
				while(1)
				{
					int len = recv(fd_mqtt, &rx_buf[rx_idx], sizeof(rx_buf)-rx_idx, 500);
					g_mqtt_heartbeat = xTaskGetTickCount();
					if(len>0)
					{
						rx_idx += len;
						while(rx_idx>=2)
						{
							uint32_t total_packet_len =Get_TotalPacket_Len(rx_buf,len);
							if(total_packet_len>0&&total_packet_len<=rx_idx)
							{
								/* 订阅消息 */
								LOG_DEBUG("%s",rx_buf);
								Subscribe_Callback(rx_buf, total_packet_len);
								if((rx_idx-total_packet_len) > 0)
								{
									memmove(rx_buf,&rx_buf[total_packet_len],rx_idx-total_packet_len);
								}
								rx_idx-=total_packet_len;
							}
							else break;
						}
					}
					/* 断连 */
					else if (socket_status(fd_mqtt)==0)
					{
						LOG_DEBUG("Detect Disconnected (CLOSED URC)! Reconnecting...");
						break;
					}
					uint32_t current_tick = xTaskGetTickCount();
					if(g_mqtt_ping_waiting==1)
					{
						/* 心跳超时检测 */
						if((current_tick-last_tick)>=pdMS_TO_TICKS(10000))
						{
							LOG_DEBUG("Ping Timeout! Network Dead. Forcing reconnect...");
							break;
						}
					}
					else
					{
						/* ping */
						if((current_tick-last_tick)>=pdMS_TO_TICKS(35000))
						{
							MQTT_PingReq(fd_mqtt);
							g_mqtt_ping_waiting=1;
							last_tick = current_tick;
							LOG_DEBUG("Send PINGREQ");
						}
					}
				}
			}
        }
        else 
        {
            LOG_DEBUG("connect ERR, Wait to retry...");
        }
		g_mqtt_heartbeat = xTaskGetTickCount();
		g_mqtt_sockfd=-1;
        close(fd_mqtt);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
TaskHandle_t MQTT_Subscribe_TaskHandler;
TaskHandle_t ListenRx_TaskHandler;
TaskHandle_t MQTT_Public_TaskHandler;
void MQTT_Task(void* arg)
{
	LOG_DEBUG("MQTT Task");
	socket_register_device(&esp8266_net_device);  /* 注册socket */
	xTaskCreate(AT_Recv_Task,"AT",512,&at_esp8266,7,&ListenRx_TaskHandler);/* 监听Rx任务 */
	xTaskCreate(MQTT_Subscribe_Task,"MQTT_Task",512,&at_esp8266,5,&MQTT_Subscribe_TaskHandler);
	xTaskCreate(MQTT_Public_Task,"mqtt_public",512,NULL,3,&MQTT_Public_TaskHandler);
	vTaskDelete(NULL);
}