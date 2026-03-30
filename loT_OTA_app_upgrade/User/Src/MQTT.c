#include "AT_Command.h"
#include "ESP8266.h"
#include "Socket.h"
#include "MQTT_Client.h"
#include "OLED.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "Broker.h"
#include "OTA.h"
#include "MAX30102.h"
#include "Broker.h"
#include <string.h>
extern volatile uint8_t g_mqtt_ping_waiting;
volatile int g_mqtt_sockfd = -1;
static QueueHandle_t xMQTTQue_HealthData;
/* 发布 */
void MQTT_Public_Task(void* arg)
{
	xMQTTQue_HealthData=xQueueCreate(5,sizeof(HealthData_t));
	Broker_Subscribe(TOPIC_HEALTH_DATA,xMQTTQue_HealthData);
	HealthData_t mqtt_HrSpo2;
	while(1)
	{
		if(g_mqtt_sockfd>=0&&socket_status(g_mqtt_sockfd)==1)
		{
			BaseType_t has_new_data = pdFALSE;
			while(xQueueReceive(xMQTTQue_HealthData, &mqtt_HrSpo2, 0) == pdTRUE) 
			{
				if(mqtt_HrSpo2.confidence==1)
				{
					has_new_data = pdTRUE;
				}
			}
			if(has_new_data ==pdTRUE)
			{
				char cmd[64];
				snprintf(cmd, sizeof(cmd), "HR=%d,SPO2=%d\r\n",mqtt_HrSpo2.HeartRate_Value,
						mqtt_HrSpo2.Spo2_Value);
				MQTT_Publish(g_mqtt_sockfd,"qrszx/down",cmd);
				
			}
		}
		vTaskDelay(pdMS_TO_TICKS(3000));
	}
}
/* 指定标题回调函数 */
void my_mqtt_msg_handler(const char* payload, uint16_t len)
{
    char msg[64] = {0};
    uint16_t copy_len = (len > sizeof(msg)) ? sizeof(msg)-1 : len;
    memcpy(msg, payload, copy_len);
    msg[copy_len] = '\0';
    
    OLED_ShowString(1, 1, "MQTT Recv:");
    OLED_ShowString(2, 1, msg);
    LOG_DEBUG("MQTT Received: %s", msg);
}
OTA_Config_t ota_cmd;
void my_mqtt_ota_handler(const char* payload, uint16_t len)
{
	char msg[64] = {0};
    uint16_t copy_len = (len >= sizeof(msg)) ? sizeof(msg)-1 : len;
    memcpy(msg, payload, copy_len);
    msg[copy_len] = '\0';
	/* deal ota command */
	if (strncmp(msg, "OTA:", 4) == 0)
	{
		int ip[4], port;
        char path[64];
		if (sscanf(msg,"OTA:%d.%d.%d.%d:%d:%63s",&ip[0],&ip[1],&ip[2],&ip[3],&port, path)==6)
		{
			ota_cmd.ip[0]=ip[0];
			ota_cmd.ip[1]=ip[1];
			ota_cmd.ip[2]=ip[2];
			ota_cmd.ip[3]=ip[3];
			ota_cmd.port=port;
			strcpy(ota_cmd.path, path);
			Broker_Publish(TOPIC_OTA_DATA,&ota_cmd);
		}
	}
}
/* 订阅 */
void MQTT_Subscribe_Task(void *pvParameters)
{
	int result=init(&at_esp8266,"www","123456789");	/* 联网 */
	assert_param(result==0);
	LOG_DEBUG("init OK");
	int fd_mqtt;
	while(1)
    {
        fd_mqtt = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_mqtt < 0)
		{
			LOG_DEBUG("socket ERR");
            vTaskDelay(pdMS_TO_TICKS(3000)); 
            continue;
        }
		g_mqtt_sockfd=fd_mqtt;
		LOG_DEBUG("socket OK");

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(1883);
        server_addr.sin_addr.s_addr = IP4_ADDR(44,232,241,40);

        if (connect(fd_mqtt, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
        {
			LOG_DEBUG("connect OK");
			vTaskDelay(pdMS_TO_TICKS(100));
            if (MQTT_Connect(fd_mqtt, "STM32_Client_01") > 0)
			{
				LOG_DEBUG("MQTT Connect Packet Sent!");
                vTaskDelay(100);
				MQTT_Subscribe("qrszx/up", my_mqtt_msg_handler, fd_mqtt);
				MQTT_Subscribe("qrszx/ota", my_mqtt_ota_handler, fd_mqtt);
                LOG_DEBUG("Subscribed to stm32");
				uint8_t rx_buf[512];
				int rx_idx=0;
				g_mqtt_ping_waiting = 0;
				uint32_t last_tick = xTaskGetTickCount();
				while(1)
				{
					int len = recv(fd_mqtt, &rx_buf[rx_idx], sizeof(rx_buf)-rx_idx, 500);

					if(len>0)
					{
						rx_idx += len;
						while(rx_idx>=2)
						{
							uint32_t total_packet_len =Get_TotalPacket_Len(rx_buf,len);
							if(total_packet_len>0&&total_packet_len<=rx_idx)
							{
								/* 订阅消息 */
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
		g_mqtt_sockfd=-1;
        close(fd_mqtt);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
static TaskHandle_t MQTT_Subscribe_TaskHandler;
static TaskHandle_t listenRx_TaskHandler2;
static TaskHandle_t MQTT_Public_TaskHandler;
void MQTT_Task(void* arg)
{
	LOG_DEBUG("MQTT Task");
	socket_register_device(&esp8266_net_device);  /* 注册socket */
	xTaskCreate(AT_Recv_Task,"AT",512,&at_esp8266,7,&listenRx_TaskHandler2);/* 监听Rx任务 */
	xTaskCreate(MQTT_Subscribe_Task,"MQTT_Task",512,&at_esp8266,5,&MQTT_Subscribe_TaskHandler);
	xTaskCreate(MQTT_Public_Task,"mqtt_public",512,NULL,3,&MQTT_Public_TaskHandler);
	vTaskDelete(NULL);
	while(1);
}