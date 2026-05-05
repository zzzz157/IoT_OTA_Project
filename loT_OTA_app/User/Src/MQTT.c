#include "AT_Command.h"
#include "ESP8266.h"
#include "Socket.h"
#include "MQTT_Client.h"
#include "OLED.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"
#include "main.h"
#include "Broker.h"
#include "OTA.h"
#include "MQTT.h"
#include "MAX30102.h"
#include "Broker.h"
#include "cJSON.h"
#include "cJSON_config.h"
#include "SysParam.h"
#include "Monitor.h"
#include "lfs.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>


//$oc/devices/{device_id}/sys/messages/up
//$oc/devices/69f82fa318855b39c515bd1c_qrszx_01/sys/messages/up
//$oc/devices/69f82fa318855b39c515bd1c_qrszx_01/sys/properties/report
extern volatile uint8_t g_mqtt_ping_waiting;
extern volatile uint32_t g_mqtt_heartbeat;

extern lfs_t lfs;

volatile int g_mqtt_sockfd = -1;
static QueueHandle_t xMQTTQue_HealthData;
SemaphoreHandle_t g_OfflineSaveSema=NULL;
SemaphoreHandle_t g_cjson_mutex = NULL;
EventGroupHandle_t xGlobalEventGroup;
/* Publish */
void MQTT_Publish_Task(void* arg)
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
					xSemaphoreTake(g_cjson_mutex,portMAX_DELAY);
					while(lfs_file_read(&lfs,&file,&history_data,sizeof(HealthData_t))==
							sizeof(HealthData_t))
					{
						reset_cjson_pool();
						cJSON* cjson_health=cJSON_CreateObject();
				
						cJSON* services = cJSON_CreateArray();
						cJSON_AddItemToObject(cjson_health,"services",services);
						
						cJSON* service_obj = cJSON_CreateObject();
						cJSON_AddItemToArray(services, service_obj);
						
						cJSON_AddStringToObject(service_obj, "service_id", "HealthData");
						
						cJSON* properties = cJSON_CreateObject();
						cJSON_AddNumberToObject(properties,"heart_rate",history_data.HeartRate_Value);
						cJSON_AddNumberToObject(properties,"spo2",history_data.Spo2_Value);
						cJSON_AddNumberToObject(properties,"timestamp",mqtt_HrSpo2.timestamp_ms);
						cJSON_AddNumberToObject(properties,"valid",history_data.confidence);
						cJSON_AddNumberToObject(properties,"history",1);
						cJSON_AddItemToObject(service_obj,"properties",properties);
						
						char* telemetry=cJSON_PrintUnformatted(cjson_health);
						if(telemetry!=NULL)
						{
							MQTT_Publish(g_mqtt_sockfd
								,"$oc/devices/69f82fa318855b39c515bd1c_qrszx_01/sys/properties/report"
								,telemetry);
							cJSON_free(telemetry);
						}
						cJSON_Delete(cjson_health);
						g_mqtt_heartbeat = xTaskGetTickCount();
						vTaskDelay(50);
					}
					lfs_remove(&lfs,HEALTH_OFFLINE_SAVE_FILE);
					lfs_file_close(&lfs, &file);
					xSemaphoreGive(g_cjson_mutex);
				}
			}
			/* vaild data */
			if(has_new_data ==pdTRUE)
			{
				xSemaphoreTake(g_cjson_mutex,portMAX_DELAY);
				reset_cjson_pool();
				
				cJSON* cjson_health=cJSON_CreateObject();
				
				cJSON* services = cJSON_CreateArray();
				cJSON_AddItemToObject(cjson_health,"services",services);
				
				cJSON* service_obj = cJSON_CreateObject();
				cJSON_AddItemToArray(services, service_obj);
				
				cJSON_AddStringToObject(service_obj, "service_id", "HealthData");
				
				cJSON* properties = cJSON_CreateObject();
				cJSON_AddNumberToObject(properties,"heart_rate",mqtt_HrSpo2.HeartRate_Value);
				cJSON_AddNumberToObject(properties,"spo2",mqtt_HrSpo2.Spo2_Value);
				cJSON_AddNumberToObject(properties,"timestamp",mqtt_HrSpo2.timestamp_ms);
				cJSON_AddNumberToObject(properties,"valid",mqtt_HrSpo2.confidence);
				cJSON_AddItemToObject(service_obj,"properties",properties);
				
				char* telemetry=cJSON_PrintUnformatted(cjson_health);
				if(telemetry!=NULL)
				{
					MQTT_Publish(g_mqtt_sockfd
						,"$oc/devices/69f82fa318855b39c515bd1c_qrszx_01/sys/properties/report"
						,telemetry);
					cJSON_free(telemetry);
				}
				cJSON_Delete(cjson_health);
				xSemaphoreGive(g_cjson_mutex);
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
/* Sub callback */
static void my_mqtt_cmd_handler(const char* topic,const char* payload, uint16_t len)
{
	static char message[256] = {0};
    uint16_t copy_len = (len >= sizeof(message)) ? sizeof(message)-1 : len;
    memcpy(message, payload, copy_len);
    message[copy_len] = '\0';
	LOG_DEBUG("%s",message);
	char* req_id_ptr = strstr(topic, "request_id=");
	if(req_id_ptr!=NULL)
	{
		req_id_ptr+=11;
		static char response_topic[256];
		snprintf(response_topic, sizeof(response_topic),
			"$oc/devices/%s/sys/commands/response/request_id=%s"
			,g_SysParam.mqtt_username,req_id_ptr);
		char response_payload[] = "{\"result_code\": 0, \"response_name\": \"OTA_RESPONSE\"}";
		MQTT_Publish(g_mqtt_sockfd, response_topic, response_payload);
		LOG_DEBUG("Reply Sent to: %s", response_topic);
	}
	xSemaphoreTake(g_cjson_mutex,portMAX_DELAY);
	reset_cjson_pool();
	cJSON* cjson_head = cJSON_Parse(message);
	if(cjson_head == NULL)
	{
		LOG_DEBUG("parse fail");
		goto exit;
	}
	cJSON* cjson_cmd=cJSON_GetObjectItem(cjson_head,"command_name");
	if (cjson_cmd != NULL && cJSON_IsString(cjson_cmd))
	{
		/* cmd Dispatcher */
		if (strcmp(cjson_cmd->valuestring, "OTA") == 0)
		{
			/* deal ota command */
			cJSON* paras = cJSON_GetObjectItem(cjson_head,"paras");
			cJSON* cjson_ip=cJSON_GetObjectItem(paras,"ip");
			cJSON* cjson_port=cJSON_GetObjectItem(paras,"port");
			cJSON* cjson_path=cJSON_GetObjectItem(paras,"path");
			if (cjson_ip && cJSON_IsString(cjson_ip) && cjson_port && cJSON_IsNumber(cjson_port)
                && cjson_path && cJSON_IsString(cjson_path))
			{
				OTA_Config ota_cmd;
                int ip[4];
				if (sscanf(cjson_ip->valuestring,"%d.%d.%d.%d",&ip[0],&ip[1],&ip[2],&ip[3])==4)
				{
					LOG_DEBUG("Trigger OTA process");
					ota_cmd.ip[0]=ip[0];
					ota_cmd.ip[1]=ip[1];
					ota_cmd.ip[2]=ip[2];
					ota_cmd.ip[3]=ip[3];
					ota_cmd.port=cjson_port->valueint;
					ota_cmd.is_new=1;
					strcpy(ota_cmd.path, cjson_path->valuestring);
					Broker_Publish(TOPIC_OTA_REQ,&ota_cmd);
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
exit:
	cJSON_Delete(cjson_head);
	xSemaphoreGive(g_cjson_mutex);
}
/* Subscribe */
void MQTT_Subscribe_Task(void *pvParameters)
{
	int fd_mqtt;
	static uint8_t tcp_fail_count=0;
	while(1)
    {
		xEventGroupClearBits(xGlobalEventGroup, EVENT_MQTT_WIFICONNECTED);
		g_mqtt_heartbeat = xTaskGetTickCount();
		int result;
		do{
			result=init(&at_esp8266,g_SysParam.wifi_ssid,g_SysParam.wifi_pwd);	/* 联网 */
			g_mqtt_heartbeat = xTaskGetTickCount();
			if(result!=0)
			{
				LOG_DEBUG("WiFi Init Fail, wait 3s...");
				vTaskDelay(pdMS_TO_TICKS(3000));
			}
		}while(result!=0);
		LOG_DEBUG("init OK");
		xEventGroupSetBits(xGlobalEventGroup, EVENT_MQTT_WIFICONNECTED);
		while(1)
		{
			fd_mqtt = socket(AF_INET, SOCK_STREAM, 0);
			if(fd_mqtt < 0)
			{
				g_mqtt_heartbeat = xTaskGetTickCount();
				LOG_DEBUG("socket ERR");
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
				tcp_fail_count=0;
				LOG_DEBUG("connect OK");
				vTaskDelay(pdMS_TO_TICKS(100));
				
				if (MQTT_Connect(fd_mqtt,g_SysParam.mqtt_client_id,g_SysParam.mqtt_username
						,g_SysParam.mqtt_password) > 0)
				{
					g_mqtt_heartbeat = xTaskGetTickCount();
					LOG_DEBUG("MQTT Connect Packet Sent!");
					vTaskDelay(100);
					MQTT_Subscribe("$oc/devices/69f82fa318855b39c515bd1c_qrszx_01/sys/commands/#"
						, my_mqtt_cmd_handler, fd_mqtt);
					LOG_DEBUG("Subscribed to stm32");
					static uint8_t rx_buf[512];
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
								uint32_t total_packet_len =Get_TotalPacket_Len(rx_buf,rx_idx);
								if(total_packet_len>0&&total_packet_len<=rx_idx)
								{
									/* 订阅消息 */
									//LOG_DEBUG("%s",rx_buf);
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
				tcp_fail_count++;
			}
			g_mqtt_heartbeat = xTaskGetTickCount();
			g_mqtt_sockfd=-1;
			close(fd_mqtt);
			vTaskDelay(pdMS_TO_TICKS(500));
			if(tcp_fail_count>=5) 
			{
				tcp_fail_count=0;
				break;
			}
		}
	}
}
TaskHandle_t MQTT_Subscribe_TaskHandler;
TaskHandle_t ListenRx_TaskHandler;
TaskHandle_t MQTT_Publish_TaskHandler;
void MQTT_Task(void* arg)
{
	LOG_DEBUG("MQTT Task");
	socket_register_device(&esp8266_net_device);  /* 注册socket */
	My_cJSON_Hook_Init(); 	/* init cJSON hook */
	if(g_cjson_mutex==NULL) g_cjson_mutex=xSemaphoreCreateMutex(); /* create cjson mutex */
	xGlobalEventGroup = xEventGroupCreate();/* create global eventgroup */
	
	xTaskCreate(AT_Recv_Task,"AT",512,&at_esp8266,7,&ListenRx_TaskHandler);/* 监听Rx任务 */
	xTaskCreate(MQTT_Subscribe_Task,"MQTT_Task",2048,&at_esp8266,5,&MQTT_Subscribe_TaskHandler);
	xTaskCreate(MQTT_Publish_Task,"mqtt_publish",512,NULL,3,&MQTT_Publish_TaskHandler);
	vTaskDelete(NULL);
}