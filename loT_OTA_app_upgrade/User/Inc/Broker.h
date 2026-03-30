#ifndef __BROKER__H
#define __BROKER__H

#include "FreeRTOS.h"
#include "queue.h"

#define MAX_TOTAL_SUBSCRIBERS 	10
typedef enum 
{
    TOPIC_HEALTH_DATA = 0,
	TOPIC_OTA_DATA,
    TOPIC_MAX
}EventTopic_t;

typedef struct  _BusList BusList;

typedef struct _BusList
{
	QueueHandle_t quehandle;
	BusList* Next;
}BusList;

void Broker_Init();
uint8_t Broker_Subscribe(EventTopic_t topic, QueueHandle_t task_queue);
uint8_t Broker_Publish(EventTopic_t topic, void* data);


#endif