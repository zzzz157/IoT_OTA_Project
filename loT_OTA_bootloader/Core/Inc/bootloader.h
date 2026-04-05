#ifndef _BOOTLOADER__H
#define _BOOTLOADER__H

#define OTA_HAED_LENGTH   	 0x200
#define OTA_FLAG_ADDR        BOOT_FLAGS_ADDR
#define OTA_DOWN_HEAD_ADDR   OTA_DOWNLOAD
#define OTA_APP_DOWN_ADDR 	 (OTA_DOWNLOAD+OTA_HAED_LENGTH)
#define OTA_BACK_HEAD_ADDR   OTA_BACKUP
#define OTA_APP_BACK_ADDR 	 (OTA_BACKUP+OTA_HAED_LENGTH)

#define IH_MAGIC 			0x27051956
#define IH_NMLEN 			32

#define ONCE_RELOCATE_MAX_LEN 512

typedef enum
{
	BOOT_FLAG_NONE=0xFFFFFFFF,
	BOOT_CORRUPTED_FIRMWARE=0x11111111,
	BOOT_FLAG_NEED_UPDATE=0x22222222,
	BOOT_FLAG_UPDATEING=0x33333333,
	BOOT_FLAG_TESTING=0x44444444,
	BOOT_FLAG_ROLLED_BACK=0x55555555,
}Boot_Flag;

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

uint32_t bootloader_init(W25Q64_t* ex_handle);
void OTA_Jump_TO_Normal_APP(uint32_t app_ep_addr);
void Relocate_Internal_To_Backup(uint32_t app_head_addr,uint32_t app_head_len);
void Relocate_NewApp_To_Internal(uint32_t app_head_addr,uint32_t app_head_len);
void OTA_Jump_TO_BACK_APP(uint32_t app_head_addr,uint32_t app_head_len);

#endif