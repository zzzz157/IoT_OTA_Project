#include "myuart.h"
#include "W25Q64.h"
#include "MCU_Flash.h"
#include "bootloader.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static W25Q64_t* Flash_Device=NULL;
/* init flash_handle and read ota flag */
uint32_t bootloader_init(W25Q64_t* ex_handle)
{
	uint32_t flag=0xFFFFFFFF;
	Flash_Device=ex_handle;
	Flash_Device->Init(Flash_Device);
	Flash_Device->ReadDatas(Flash_Device,OTA_FLAG_ADDR,(uint8_t*)&flag,4);
	return flag;
}
/* OTA upgrade flag manage */
static int OTA_Write_Flash_Flag(Boot_Flag flag)
{
	if(Flash_Device->SectorErase(Flash_Device,OTA_FLAG_ADDR & 0xFFFFF000)!=1)
	{
		put_str("OTA Flag Erase ERROR\r\n");
		return 0;
	}
	if(Flash_Device->WritePage(Flash_Device,OTA_FLAG_ADDR,(uint8_t*)&flag,
		sizeof(flag))!=1)
	{
		put_str("OTA Flag Write ERROR");
		return 0;
	}
	return 1;
}
static uint32_t last_erased_sector = 0xFFFFFFFF;
/* write flash */
static int OTA_Write_Flash(uint32_t start_addr, uint8_t *data, uint32_t len)
{
	uint32_t write_addr=start_addr;
	uint32_t remain = len;
    uint8_t* ptr = data;
	while(remain>0)
	{
		/* erase current sector */
		uint32_t current_sector = write_addr & 0xFFFFF000;
		if(current_sector!=last_erased_sector)
		{
			if(Flash_Device->SectorErase(Flash_Device,current_sector)==0) return 0;
			last_erased_sector=current_sector;
		}
		/* write flash page */
		uint32_t page_remain = W25Q64_PAGE_LEN-(write_addr%W25Q64_PAGE_LEN);
        uint32_t write_len = (remain <= page_remain) ? remain : page_remain;
		
		if(Flash_Device->WritePage(Flash_Device,write_addr,ptr,write_len)==0) return 0;
		
		write_addr+=write_len;
		ptr+=write_len;
		remain-=write_len;
	}
	return 1;
}

/* jump to normal app */
extern void start_app(uint32_t addr);
void OTA_Jump_TO_Normal_APP(uint32_t app_ep_addr)
{
	/* jump to app */
	__disable_irq();
	HAL_RCC_DeInit();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
	HAL_DeInit();
	start_app(app_ep_addr);
}

/* jump to upgrade app */
void OTA_Jump_TO_Upgrade_APP(uint32_t app_head_addr,uint32_t app_head_len)
{
	put_str("need to upgrade\r\n");
	header_t head;
	static uint8_t data[ONCE_RELOCATE_MAX_LEN];
	uint32_t relocated_len=0;
	/* read internal head */
	MCU_Flash_Read(app_head_addr,(uint8_t*)&head,sizeof(header_t));
	put_str("read internal head ok\r\n");
	/* simple check internal head */
	if(head.ih_magic!=IH_MAGIC)
	{
		put_str("Jump_TO_Upgrade's internal app 's magic error");
	}
	put_str("simple check internal head ok\r\n");
	/* deal internal head */
	uint32_t content_length1=head.ih_size;
	/* relocate internal app to external flash_B*/
	while(relocated_len<content_length1+app_head_len)
	{
		uint32_t remain_len=content_length1+app_head_len-relocated_len;
		uint32_t push_len=(remain_len>ONCE_RELOCATE_MAX_LEN)?ONCE_RELOCATE_MAX_LEN:remain_len;
		MCU_Flash_Read(app_head_addr+relocated_len,data,push_len);
		OTA_Write_Flash(OTA_BACKUP+relocated_len,data,push_len);
		relocated_len+=push_len;
	}
	put_str("relocate internal app to external flash_B ok\r\n");
	/* read external head */
	Flash_Device->ReadDatas(Flash_Device,OTA_DOWN_HEAD_ADDR,(uint8_t*)&head,sizeof(header_t));
	put_str("read external head\r\n");
	/* simple check external head */
	if(head.ih_magic!=IH_MAGIC)
	{
		put_str("Jump_TO_Upgrade's external app 's magic error\r\n");
	}
	put_str("simple check external head ok\r\n");
	/* deal external head */
	relocated_len=0;
	uint32_t content_length2=head.ih_size;
	/* relocate external flash_A to internal Flash*/
	MCU_Flash_Erase(app_head_addr,content_length2+app_head_len);
	while(relocated_len<content_length2+app_head_len)
	{
		uint32_t remain_len=content_length2+app_head_len-relocated_len;
		uint32_t push_len=(remain_len>ONCE_RELOCATE_MAX_LEN)?ONCE_RELOCATE_MAX_LEN:remain_len;
		if(Flash_Device->ReadDatas(Flash_Device,OTA_DOWN_HEAD_ADDR+relocated_len,data,push_len)!=1)
		{
			put_str("read spi flash error\r\n");
		}
		if (MCU_Flash_Write(app_head_addr+relocated_len, data, push_len) != 1)
		{
		    put_str("Flash Write Error!\r\n");
		}
		relocated_len+=push_len;
	}
	put_str("relocate external flash_A to internal Flash ok\r\n");
	/* updata flag */
	OTA_Write_Flash_Flag(BOOT_FLAG_TESTING);
	put_str("updata flag ok\r\n");
	/* jump new app */
	OTA_Jump_TO_Normal_APP(app_head_addr+app_head_len);
}
/* jump to back app */
void OTA_Jump_TO_BACK_APP(uint32_t app_head_addr,uint32_t app_head_len)
{
	put_str("jump to back app\r\n");
	header_t head;
	static uint8_t data[ONCE_RELOCATE_MAX_LEN];
	uint32_t relocated_len=0;
	/* read external head */
	Flash_Device->ReadDatas(Flash_Device,OTA_BACK_HEAD_ADDR,(uint8_t*)&head,sizeof(header_t));
	/* simple check external head */
	if(head.ih_magic!=IH_MAGIC)
	{
		put_str("Jump_TO_BACK's magic error");
	}
	/* deal external head */
	uint32_t content_length=head.ih_size;
	/* relocate external flash_B to internal Flash*/
	MCU_Flash_Erase(app_head_addr,content_length+app_head_len);
	while(relocated_len<content_length+app_head_len)
	{
		uint32_t remain_len=content_length+app_head_len-relocated_len;
		uint32_t push_len=(remain_len>ONCE_RELOCATE_MAX_LEN)?ONCE_RELOCATE_MAX_LEN:remain_len;
		if(Flash_Device->ReadDatas(Flash_Device,OTA_BACK_HEAD_ADDR+relocated_len,data,push_len)!=1)
		{
			put_str("read spi flash error\r\n");
		}
		if (MCU_Flash_Write(app_head_addr+relocated_len, data, push_len) != 1)
		{
		    put_str("Flash Write Error!\r\n");
		}
		relocated_len+=push_len;
	}
	/* updata flag */
	OTA_Write_Flash_Flag(BOOT_FLAG_ROLLED_BACK);
	put_str("updata flag ok\r\n");
	/* jump old app */
	put_str("jump to back app ok\r\n");
	OTA_Jump_TO_Normal_APP(app_head_addr+app_head_len);
}
