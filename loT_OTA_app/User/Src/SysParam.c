#include "lfs.h"
#include "SysParam.h"
#include "OTA.h"
#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

char* 	 HW_CLIENT_ID = "69f82fa318855b39c515bd1c_qrszx_01_0_0_2026050405";
char* 	 HW_USERNAME  = "69f82fa318855b39c515bd1c_qrszx_01";
uint8_t  HW_IP[4]     =  {124,70,218,131};
uint16_t HW_PORT      =  1883;

lfs_t lfs;
SysParam_t g_SysParam;
extern SemaphoreHandle_t lfs_mutex;

static void Restore_Default_Params()
{
	memset(&g_SysParam, 0, sizeof(SysParam_t));
	g_SysParam.magic=SYS_CFG_MAGIC;
	
	strcpy(g_SysParam.wifi_ssid,"www");
	strcpy(g_SysParam.wifi_pwd,"123456789");
	
	g_SysParam.mqtt_ip[0]=HW_IP[0];
	g_SysParam.mqtt_ip[1]=HW_IP[1];
	g_SysParam.mqtt_ip[2]=HW_IP[2];
	g_SysParam.mqtt_ip[3]=HW_IP[3];
	g_SysParam.mqtt_port=HW_PORT;
	strcpy(g_SysParam.mqtt_client_id, HW_CLIENT_ID);
    strcpy(g_SysParam.mqtt_username,  HW_USERNAME);
    strcpy(g_SysParam.mqtt_password,  HW_PASSWORD);
	
	g_SysParam.modbus_id=0x01;
	g_SysParam.boot_count=0;
}

void lfs_init()
{
	if(lfs_mutex == NULL) lfs_mutex = xSemaphoreCreateMutex();
	int err = lfs_mount(&lfs, &lfs_cfg);
	if (err) 
	{
        LOG_DEBUG("LFS Mount Fail, Formatting...");
        lfs_format(&lfs, &lfs_cfg);
        err = lfs_mount(&lfs, &lfs_cfg);
        if(err) 
		{
            LOG_DEBUG("LFS Format Fail! Flash Hardware Error!");
            while(1); // 硬件坏了
        }
    }
    LOG_DEBUG("LittleFS Mount Success!");
}
int SysParam_Save()
{
	lfs_file_t file;
	g_SysParam.crc32 = crc32_calculate(0, (uint8_t*)&g_SysParam, sizeof(SysParam_t) - 4);
	int err = lfs_file_open(&lfs, &file, SYS_CFG_FILE, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (err == LFS_ERR_OK)
    {
        lfs_file_write(&lfs, &file, &g_SysParam, sizeof(SysParam_t));
        lfs_file_close(&lfs, &file);
        return 1;
    }
    return 0;
}
void SysParam_Init()
{
	lfs_file_t file;
    uint8_t need_reset = 1;
	int err = lfs_file_open(&lfs, &file, SYS_CFG_FILE, LFS_O_RDONLY);
	if (err == LFS_ERR_OK)
	{
		lfs_ssize_t read_len =lfs_file_read(&lfs,&file,&g_SysParam,sizeof(SysParam_t));
		lfs_file_close(&lfs, &file);
		if(read_len==sizeof(SysParam_t))
		{
			if(g_SysParam.magic==SYS_CFG_MAGIC)
			{
				uint32_t calc_crc = crc32_calculate(0, (uint8_t*)&g_SysParam, sizeof(SysParam_t) - 4);
				if(calc_crc==g_SysParam.crc32)
				{
					need_reset=0;
				}
			}
		}
	}
	if (need_reset)
	{
		Restore_Default_Params();
		SysParam_Save();
	}
	else
	{
		g_SysParam.boot_count++;
        SysParam_Save();
	}
}

