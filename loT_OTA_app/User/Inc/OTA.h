#ifndef __OTA_H
#define __OTA_H
#include <stdint.h>
#include "W25Q64.h"

#define OTA_HAED_LENGTH   	 0x200
#define OTA_FLAG_ADDR        BOOT_FLAGS_ADDR
#define OTA_HEAD_START_ADDR  OTA_DOWNLOAD
#define OTA_APP_START_ADDR 	 (OTA_DOWNLOAD+OTA_HAED_LENGTH)
#define OTA_DOWNLOAD_REQ_LEN 1024

#define APP_LOAD_ADDRESS 	 0x08020000
#define APP_EP_ADDRESS 	 	 0x08020200

#define IH_MAGIC 			 0x27051956
#define IH_NMLEN 			 32

#define OTA_CFG_FILE    	 "/ota_cfg.bin"

typedef enum
{
	BOOT_FLAG_NONE=0xFFFFFFFF,
	BOOT_CORRUPTED_FIRMWARE=0x11111111,
	BOOT_FLAG_NEED_UPDATE=0x22222222,
	BOOT_FLAG_UPDATEING=0x33333333,
	BOOT_FLAG_TESTING=0x44444444,
	BOOT_FLAG_ROLLED_BACK=0x55555555,
	BOOT_FLAG_DOWNLOADING=0x66666666,
}Boot_Flag;

typedef enum
{
	OTA_STATUS_IDLE = 0,
	OTA_STATUS_STARTED,
	OTA_STATUS_DOWNLOADING,
	OTA_STATUS_PAUSED,
	OTA_STATUS_SUCCESS,
	OTA_STATUS_FAILED,
    OTA_STATUS_RESUMING,
}OTA_Status;

typedef struct {
    uint8_t ip[4];
    uint16_t port;
    char path[64];
	uint8_t is_new;
}OTA_Config;
#pragma pack(push, 1)
typedef struct image_header{
    uint32_t    ih_magic;       /* Image Header Magic Number    */
    uint32_t    ih_hcrc;        /* Image Header CRC Checksum    */
    uint32_t    ih_time;        /* Image Creation Timestamp     */
    uint32_t    ih_size;        /* Image Data Size              */
    uint32_t    ih_load;        /* Data  Load  Address          */
    uint32_t    ih_ep;          /* Entry Point Address          */
    uint32_t    ih_dcrc;        /* Image Data CRC Checksum      */
    uint8_t     ih_os;          /* Operating System             */
    uint8_t     ih_arch;        /* CPU architecture             */
    uint8_t     ih_type;        /* Image Type                   */
    uint8_t     ih_comp;        /* Compression Type             */
    uint8_t     ih_name[IH_NMLEN];  /* Image Name             */
}header_t;
#pragma pack(pop)

uint32_t crc32_calculate(uint32_t crc, const uint8_t *buf, uint32_t len);
int OTA_Write_Flash_Flag(Boot_Flag flag);
void OTA_Task(void* arg);

#endif