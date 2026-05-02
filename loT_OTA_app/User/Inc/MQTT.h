#ifndef __MQTT__H
#define __MQTT__H

#include "FreeRTOS.h"
#include "event_groups.h"

#define EVENT_MQTT_WIFICONNECTED   (1 << 0)
#define EVENT_OTA_SYSTEMRESET  	   (1 << 1)
#define EVENT_OTA_NORMALCONTROL    (1 << 2)
#define EVENT_OTA_RESUMEDOWNLOAD   (1 << 3)

extern EventGroupHandle_t xWiFiMQTTEventGroup;

void MQTT_Task(void* arg);

#endif