#ifndef __MQTT_CLIENT_H
#define __MQTT_CLIENT_H
#define MQTT_MAX_LIST_NODE 10

typedef void (*MQTT_Callback_t)(const char* topic,const char* payload, uint16_t len);

typedef struct
{
    char topic_name[128];
    MQTT_Callback_t handler;
	int sockfy;
}MQTT_Topic_Handle_t;

typedef struct  _MQTT_List MQTT_List;
typedef struct _MQTT_List
{
	MQTT_Topic_Handle_t mqtt_sub;
	MQTT_List* Next;
}MQTT_List;

int MQTT_Connect(int fd, const char* client_id,const char* username,const char* password);
int MQTT_Publish(int fd, const char* topic, const char* payload);
int MQTT_Subscribe(const char* topic_name,MQTT_Callback_t handler,int sockfy);
int Subscribe_Callback(void* rx_buf, uint16_t size);
int MQTT_PingReq(int fd);

uint32_t Get_TotalPacket_Len(void* rx_buf,uint16_t len);

#endif