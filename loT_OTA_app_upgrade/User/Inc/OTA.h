#ifndef __OTA_H
#define __OTA_H
#include <stdint.h>

#define OTA_FLAG_ADDR        (0x7FFFFF-4)
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
#pragma pack(push, 1)
typedef struct image_header 
{
    uint32_t    ih_magic;       /* Image Header Magic Number    */
    uint32_t    ih_hcrc;        /* Image Header CRC Checksum    */
    uint32_t    ih_time;        /* Image Creation Timestamp     */
    uint32_t    ih_size;        /* Image Data Size              */
    uint32_t    ih_load;        /* Data Load Address            */
    uint32_t    ih_ep;          /* Entry Point Address          */
    uint32_t    ih_dcrc;        /* Image Data CRC Checksum      */
    uint8_t     ih_os;          /* Operating System             */
    uint8_t     ih_arch;        /* CPU architecture             */
    uint8_t     ih_type;        /* Image Type                   */
    uint8_t     ih_comp;        /* Compression Type             */
    uint8_t     ih_name[IH_NMLEN];  /* Image Name               */
} header_t;
#pragma pack(pop)

typedef struct {
    uint8_t ip[4];
    uint16_t port;
    char path[64];
}OTA_Config_t;

void OTA_Task(void* arg);

#endif