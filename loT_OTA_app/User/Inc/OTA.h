#ifndef __OTA_H
#define __OTA_H
#include <stdint.h>

#define OTA_FLAG_ADDR        0x010000
#define OTA_HEAD_START_ADDR  0x000000
#define OTA_APP_START_ADDR 	 0x000200
#define OTA_DOWN_RXBUF_LEN   1024
#define OTA_HAED_LENGTH   	 0x200

#define OTA_UPGRADE_DOING    0xA5A5
#define OTA_UPGRADE_CPLT     0x5A5A

#define APP_LOAD_ADDRESS 	 0x08040000
#define APP_EP_ADDRESS 	 	 0x08040200

#define IH_MAGIC 			 0x27051956
#define IH_NMLEN 			 32

typedef struct {
    uint8_t ip[4];
    uint16_t port;
    char path[64];
}OTA_Config_t;

void OTA_Task(void* arg);

#endif