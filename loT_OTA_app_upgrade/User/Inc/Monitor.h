#ifndef __MONITOR_H
#define __MONITOR_H

/* less than iwag's reset_tick */
#define MQTT_ALIVE_TICK			pdMS_TO_TICKS(9000)
#define MAX30102_ALIVE_TICK		pdMS_TO_TICKS(9000)

void Monitor_Task(void* arg);

#endif