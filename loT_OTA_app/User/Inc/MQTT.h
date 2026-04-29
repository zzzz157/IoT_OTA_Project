#ifndef __MQTT__H
#define __MQTT__H

#include "FreeRTOS.h"
#include "event_groups.h"

#define EVENT_WIFI_CONNECTED   (1 << 0)
extern EventGroupHandle_t xWiFiMQTTEventGroup;

void MQTT_Task(void* arg);

#endif