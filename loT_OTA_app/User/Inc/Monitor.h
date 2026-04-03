#ifndef __MONITOR_H
#define __MONITOR_H

/* less than iwag's reset_tick */
#define MQTT_ALIVE_TICK				pdMS_TO_TICKS(18000)
#define MAX30102_ALIVE_TICK			pdMS_TO_TICKS(18000)
#define STACK_MARK_TICK				pdMS_TO_TICKS(30000)

/* offline save file */
#define HEALTH_OFFLINE_SAVE_FILE	"/OfflineSave.bin"
#define HEALTH_OFFLINE_MAX_COUNT	10


void Monitor_Task(void* arg);

#endif