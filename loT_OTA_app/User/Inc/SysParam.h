#ifndef __SYSPARAM_H
#define __SYSPARAM_H

#include <stdint.h>
#define SYS_CFG_MAGIC   0x55AA1122
#define SYS_CFG_FILE    "/sys_cfg.bin"
#pragma pack(push, 1)
typedef struct {
	uint32_t magic;
	
	char wifi_ssid[32];     /* WiFi 名称 */ 
    char wifi_pwd[64];      /* WiFi 密码 */ 
	
	uint8_t mqtt_ip[4];     /* MQTT 服务器 IP */ 
    uint16_t mqtt_port;     /* MQTT 服务器 端口 */ 
	
	uint8_t modbus_id;      /* Modbus 从机地址 */ 
	
	uint32_t boot_count;    /* 开机次数统计 */ 
	
	uint32_t crc32;         /* CRC32校验 */ 
}SysParam_t;
#pragma pack(pop)

extern SysParam_t g_SysParam;

void lfs_init();

int SysParam_Save();
void SysParam_Init();

#endif