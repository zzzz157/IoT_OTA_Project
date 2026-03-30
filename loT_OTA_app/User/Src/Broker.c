#include "main.h"
#include "Broker.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
/* 总线主题链表 */
static BusList* TopicHeads[TOPIC_MAX]={NULL};

/* 静态内存池 */
static BusList NodePool[MAX_TOTAL_SUBSCRIBERS];
static uint8_t NodeCount = 0;

static SemaphoreHandle_t xBrokerMutex = NULL;
/* 初始化：创建互斥锁，清空链表头节点 */
void Broker_Init()
{
	if(xBrokerMutex==NULL) 
	{
		xBrokerMutex = xSemaphoreCreateMutex();
	}
	for(uint8_t i=0;i<TOPIC_MAX;i++)
	{
		TopicHeads[i]=NULL;
	}
	NodeCount=0;
}
/* 订阅：分配一个节点，插入链表 */
uint8_t Broker_Subscribe(EventTopic_t topic, QueueHandle_t task_queue)
{
	if(topic>=TOPIC_MAX||task_queue==NULL||NodeCount >= MAX_TOTAL_SUBSCRIBERS) return 0;
	xSemaphoreTake(xBrokerMutex, portMAX_DELAY);
	BusList* pNewNode = &NodePool[NodeCount++];
	pNewNode->quehandle=task_queue;
	pNewNode->Next=TopicHeads[topic];
	TopicHeads[topic]=pNewNode;
	xSemaphoreGive(xBrokerMutex);
	return 1;
}
/* 发布：写对应主题链表下的队列 */
uint8_t Broker_Publish(EventTopic_t topic, void* data)
{
	if(topic>=TOPIC_MAX||data==NULL) return 0;
	xSemaphoreTake(xBrokerMutex, portMAX_DELAY);
	uint8_t sent_count = 0;
	BusList* pCurrent=TopicHeads[topic];
	while(pCurrent!=NULL)
	{
		xQueueSend(pCurrent->quehandle,data,0);
		sent_count++;
		pCurrent=pCurrent->Next;
	}
	xSemaphoreGive(xBrokerMutex);
	return (sent_count>0) ? 1 : 0;
}