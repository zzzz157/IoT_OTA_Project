#include "W25Q64.h"
#include "OTA.h"
#include "Socket.h"
#include "lfs.h"
#include "MQTT.h"
#include "main.h"
#include "iwdg.h"
#include "Broker.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "semphr.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static W25Q64_t* Flash_Device=NULL;
/* OTA upgrade flag manage */
int OTA_Write_Flash_Flag(Boot_Flag flag)
{
	if(Flash_Device->SectorErase(Flash_Device,OTA_FLAG_ADDR & 0xFFFFF000)!=1)
	{
		LOG_DEBUG("OTA Flag Erase ERROR");
		return 0;
	}
	if(Flash_Device->WritePage(Flash_Device,OTA_FLAG_ADDR,(uint8_t*)&flag,
		sizeof(flag))!=1)
	{
		LOG_DEBUG("OTA Flag Write ERROR");
		return 0;
	}
	return 1;
}

static uint32_t progress_record_idx = 0;
static uint32_t max_recorded_offset = 0;
/* record download progress */
static int ota_record_download_progress(uint32_t current_offset)
{
	if(current_offset<=max_recorded_offset) return 1;
	if(progress_record_idx >= 1024) 
	{
        Flash_Device->SectorErase(Flash_Device, OTA_PROGRESS_ADDR & 0xFFFFF000);
        progress_record_idx = 0;
    }
	int res= Flash_Device->WritePage(Flash_Device,OTA_PROGRESS_ADDR+4*progress_record_idx
			,(uint8_t*)&current_offset,4);
	if(res==1)
	{
		max_recorded_offset = current_offset;
		progress_record_idx++;
	}
	return res;
}
/* get resumable download address */
static uint32_t ota_get_exact_offset()
{
	uint32_t exact_offset=0;
	for(progress_record_idx=0;;progress_record_idx++)
	{
		uint32_t value=0xFFFFFFFF;
		Flash_Device->ReadDatas(Flash_Device,OTA_PROGRESS_ADDR+4*progress_record_idx
				,(uint8_t*)&value,4);
		if(value==0xFFFFFFFF) break;
		exact_offset=value;
	}
	max_recorded_offset=exact_offset;
	return exact_offset;
}
/* CRC check out */
uint32_t crc32_calculate(uint32_t crc, const uint8_t *buf, uint32_t len) 
{
    crc ^= 0xFFFFFFFF;
    while (len--) {
        crc ^= *buf++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    return crc ^ 0xFFFFFFFF;
}
/* new firmware check out */
static header_t head;
static int OTA_Download_Check(uint32_t ota_head_addr,uint32_t ota_head_len)
{
	/* read head */
	Flash_Device->ReadDatas(Flash_Device,ota_head_addr,(uint8_t*)&head,sizeof(header_t));
	/* deal head */
	if(IH_MAGIC!=head.ih_magic)
	{
		LOG_DEBUG("head's magic error");
		OTA_Write_Flash_Flag(BOOT_CORRUPTED_FIRMWARE);
		return 0;
	}
	/* app head CRC */
	uint32_t correct_crc=head.ih_hcrc;
	head.ih_hcrc=0;
	uint32_t new_crc=crc32_calculate(0, (uint8_t*)&head,sizeof(header_t));
	if(new_crc!=correct_crc)
	{
		LOG_DEBUG("head's crc error");
		OTA_Write_Flash_Flag(BOOT_CORRUPTED_FIRMWARE);
		return 0;
	}
	/* app data CRC */
	uint32_t calc_dcrc = 0;
    uint8_t buf[256];
    uint32_t check_len = 0;
	uint32_t content_length=head.ih_size;
	while(check_len < content_length)
	{
        uint32_t remain = content_length - check_len;
        uint16_t read_size = (remain > 256) ? 256 : remain;
        Flash_Device->ReadDatas(Flash_Device,ota_head_addr+ota_head_len+check_len,buf,read_size);
        calc_dcrc = crc32_calculate(calc_dcrc, buf, read_size);
        check_len += read_size;
    }
	if(calc_dcrc!=head.ih_dcrc)
	{
		LOG_DEBUG("data's crc error");
		LOG_DEBUG("expect crc:%d\r\n",head.ih_dcrc);
		LOG_DEBUG("calc crc:%d\r\n",calc_dcrc);
		OTA_Write_Flash_Flag(BOOT_CORRUPTED_FIRMWARE);
		return 0;
	}
	/* success */
	return 1;
}

static void OTA_Init(W25Q64_t* flash_dev)
{
	Flash_Device=flash_dev;
	Flash_Device->Init(Flash_Device);
}

static uint32_t last_erased_sector = 0xFFFFFFFF;
/* write flash */
static int OTA_Write_Flash(uint32_t offset, uint8_t *data, uint32_t len)
{
	uint32_t write_addr=OTA_HEAD_START_ADDR+offset;
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
/* 字符串不区分大小写比较函数 */
static int strncasecmp_custom(const char *s1, const char *s2, size_t len) {
    while (len > 0 && *s1 && *s2)
	{
        char c1 = (*s1 >= 'A' && *s1 <= 'Z') ? (*s1 + 32) : *s1;
        char c2 = (*s2 >= 'A' && *s2 <= 'Z') ? (*s2 + 32) : *s2;
        if (c1 != c2) break;
        s1++; s2++; len--;
    }
    return (len == 0) ? 0 : (*s1 - *s2);
}
int reconnect_tcp(uint8_t* ip,uint32_t port)
{
	int fd=-1;
	/* allocate struct and set loacal*/
	fd=socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)	return -1;
	/* set remote and connect */
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = port;
	server_addr.sin_addr.s_addr = IP4_ADDR(ip[0], ip[1], ip[2], ip[3]);
	if (connect(fd,(struct sockaddr *)&server_addr,sizeof(server_addr))!= 0)
	{
		close(fd);
		return -1;
	}
	return fd;
}
/* req http */
static int http_req(OTA_Config* cfg,int sock_fd,uint32_t start_addr,uint32_t end_addr)
{
	char req[256];
	snprintf(req, sizeof(req), 
					"GET %s HTTP/1.1\r\n"
					"Host: %d.%d.%d.%d:%d\r\n"
					"Range: bytes=%u-%u\r\n"
					"Accept: application/octet-stream\r\n"
					"Connection: keep-alive\r\n\r\n",
					cfg->path, cfg->ip[0], cfg->ip[1], cfg->ip[2], cfg->ip[3], cfg->port,
					start_addr, end_addr);
	return send(sock_fd, req, strlen(req), 0);
}
/* return current size */
static uint32_t http_head_deal(int sock_fd,uint32_t* total_len,uint8_t* is_206)
{
	char header_line[128];
	int line_len = 0;
	uint8_t header_end=0;
	uint32_t current_chunk_size=0;
	while(!header_end)
	{
		char c;
		if (recv(sock_fd, &c, 1, 5000)==-1)	return 0;
		if (line_len <sizeof(header_line)-1)
		{
			header_line[line_len++] = c;
		}
		else
		{
			LOG_DEBUG("ota recv overflow\r\n");
		}
		if(c=='\n')
		{
			header_line[line_len] = '\0';
			/* is normal response ? */
			if (strstr(header_line, "HTTP/"))
			{
				if(strstr(header_line, "206"))
				{
					*is_206=1;
				}
			}
			/* current head end */
			if(line_len==2&&header_line[0] == '\r')
			{
				header_end = 1;
				break;
			}
			
			if (strncasecmp_custom(header_line, "Content-Length:", 15) == 0)
			{
				current_chunk_size = atoi(header_line + 15);
			}
			if (strncasecmp_custom(header_line, "Content-Range:", 14) == 0)
            {
                char *slash = strchr(header_line, '/');
                if (slash) 
                {
                    *total_len = atoi(slash + 1);
                }
            }
			/* prepare next line */
			line_len = 0;
		}
		if(line_len >= sizeof(header_line) - 1) line_len = 0;
	}
	return current_chunk_size;
}
/* recv signal Segment data  */
static uint8_t ota_down_buf[OTA_DOWNLOAD_REQ_LEN*2];
static uint32_t http_recv(int sock_fd,uint32_t current_chunk_size,uint32_t* received_len)
{
	uint32_t chunk_received = 0;
	while (chunk_received < current_chunk_size)
    {
        int expect = current_chunk_size - chunk_received;
        if (expect > OTA_DOWNLOAD_REQ_LEN)
        {
            expect = OTA_DOWNLOAD_REQ_LEN;
        }
        int len = recv(sock_fd, ota_down_buf, expect, 5000);
        if (len <= 0) break;
		
        if(OTA_Write_Flash(*received_len, ota_down_buf, len)!=1) break;
        *received_len += len;
        chunk_received += len;
    }
	return chunk_received;
}
extern lfs_t lfs;
extern EventGroupHandle_t xGlobalEventGroup;
uint8_t is_have_later_firmware=0;
void OTA_Task(void* arg)
{
	LOG_DEBUG("OTA start");
	QueueHandle_t ota_queue=xQueueCreate(1,sizeof(OTA_Config));
	Broker_Subscribe(TOPIC_OTA_DATA,ota_queue);
	OTA_Init(&W25QHandle_t);
	
	OTA_Status status;
	uint32_t boot_flag;	
	Flash_Device->ReadDatas(Flash_Device,BOOT_FLAGS_ADDR,(uint8_t*)&boot_flag,4);
	while(1)
	{
		/* wait later farmware */
		if(boot_flag==BOOT_FLAG_DOWNLOADING)
		{
			is_have_later_firmware=1;
			EventBits_t uxBits=xEventGroupWaitBits(xGlobalEventGroup,EVENT_OTA_RESUMEDOWNLOAD
				,pdFALSE,pdTRUE,pdMS_TO_TICKS(500));
			if((uxBits&EVENT_OTA_RESUMEDOWNLOAD)!=0)
			{
				status=OTA_STATUS_RESUMING;
				Broker_Publish(TOPIC_OTA_STATUS,&status);
				LOG_DEBUG("Detect unfinished OTA, trying to resume...");
				lfs_file_t file;
				OTA_Config saved_cfg;
				int err = lfs_file_open(&lfs, &file, OTA_CFG_FILE, LFS_O_RDONLY);
				if(err == LFS_ERR_OK)
				{
					lfs_ssize_t read_len = lfs_file_read(&lfs, &file, &saved_cfg, sizeof(OTA_Config));
					lfs_file_close(&lfs, &file);
					if(read_len==sizeof(OTA_Config))
					{
						saved_cfg.is_new=0;
						LOG_DEBUG("Resume Config Loaded. Pushing to OTA Queue...");
						xQueueSend(ota_queue, &saved_cfg, 0);
					}
					else
					{
						LOG_DEBUG("OTA Config file corrupted!");
					}
				}
				else
				{
					LOG_DEBUG("No OTA Config file found in LittleFS!");
				}
			}
		}
		/* wait new farmware */
		OTA_Config cfg;
		if(xQueueReceive(ota_queue,&cfg,pdMS_TO_TICKS(500))==pdTRUE)
		{
			if(cfg.is_new==1)
			{
				lfs_file_t file;
				int err = lfs_file_open(&lfs,&file,OTA_CFG_FILE,LFS_O_WRONLY|LFS_O_CREAT|LFS_O_TRUNC);
				if(err == LFS_ERR_OK)
				{
					lfs_file_write(&lfs, &file, &cfg, sizeof(OTA_Config));
					lfs_file_close(&lfs, &file);
					LOG_DEBUG("OTA Config saved to LittleFS.");
				}
				Flash_Device->SectorErase(Flash_Device,OTA_PROGRESS_ADDR&0xFFFFF000);
			}
		}
		else 	continue;
		/* start download */
		OTA_Write_Flash_Flag(BOOT_FLAG_DOWNLOADING);
		LOG_DEBUG("OTA upgrade start to run");
		status=OTA_STATUS_DOWNLOADING;
		Broker_Publish(TOPIC_OTA_STATUS,&status);
		
		uint32_t exact_offset=ota_get_exact_offset();
		
		uint32_t received_len = exact_offset&0xFFFFF000;
		last_erased_sector = 0xFFFFFFFF;
		uint32_t content_length = 0xFFFFFFFF;
		int fd_ota = -1;
		uint8_t need_reconnect = 1;
		while(received_len<content_length)
		{
			if(need_reconnect)
			{
				xEventGroupWaitBits(xGlobalEventGroup,EVENT_MQTT_WIFICONNECTED,pdFALSE
					,pdFALSE,portMAX_DELAY);
				if(fd_ota>=0)
				{
					close(fd_ota);
					fd_ota = -1;
				}
				fd_ota = reconnect_tcp(cfg.ip,htons(cfg.port));
				if(fd_ota==-1)
				{
					vTaskDelay(pdMS_TO_TICKS(1000));
					continue;
				}
				need_reconnect = 0;
				LOG_DEBUG("OTA: Connecting Successfull");
			}
			/* send request to wait response */
			uint32_t req_start = received_len;
			uint32_t req_end = req_start + OTA_DOWNLOAD_REQ_LEN - 1;
			if(http_req(&cfg,fd_ota,req_start,req_end)==-1)
			{
				need_reconnect = 1;
				vTaskDelay(pdMS_TO_TICKS(3000));
				continue;
			}
			/* deal HTTP head */
			uint8_t is_206=0;
			uint32_t current_chunk_size = http_head_deal(fd_ota,&content_length,&is_206);
			if(current_chunk_size==0)
			{
				LOG_DEBUG("HTTP Header Error,reconnect...");
                need_reconnect = 1;
				vTaskDelay(pdMS_TO_TICKS(3000));
                continue;
			}
			if(!is_206)
			{
				LOG_DEBUG("Server reply 200, reset download!");
				received_len = 0;
				last_erased_sector = 0xFFFFFFFF;
				need_reconnect = 1;
				Flash_Device->SectorErase(Flash_Device, OTA_PROGRESS_ADDR & 0xFFFFF000); 
				continue;
			}
			/* recv singal data */
			uint32_t chunk_size=http_recv(fd_ota,current_chunk_size,&received_len);
			if(chunk_size!=current_chunk_size)
			{
				LOG_DEBUG("recv signal error, reset download!");
				need_reconnect = 1;
				continue;
			}
			/* record download progress */
			ota_record_download_progress(received_len);
			/* publish download progress */
			if(content_length != 0xFFFFFFFF && content_length > 0)
			{
				uint8_t percent = (uint8_t)(received_len*100/content_length);
				Broker_Publish(TOPIC_OTA_PROGRESS,&percent);
				HAL_IWDG_Refresh(&hiwdg);
				vTaskDelay(pdMS_TO_TICKS(100));
			}
			/* judge is pause */
			xEventGroupWaitBits(xGlobalEventGroup,EVENT_OTA_NORMALCONTROL
						,pdFALSE,pdFALSE,portMAX_DELAY);
		}
		Flash_Device->SectorErase(Flash_Device,OTA_PROGRESS_ADDR&0xFFFFF000);
		if(fd_ota >= 0)
		{
			close(fd_ota);
			fd_ota = -1;
		}
		if(content_length != 0xFFFFFFFF && received_len == content_length)
		{
			/* firmware crc check */
			if(OTA_Download_Check(OTA_HEAD_START_ADDR,OTA_HAED_LENGTH)==1)
			{
				/* updata boot flag */
				if(OTA_Write_Flash_Flag(BOOT_FLAG_NEED_UPDATE)==1)
				{
					lfs_remove(&lfs, OTA_CFG_FILE);
					
					status=OTA_STATUS_SUCCESS;
					Broker_Publish(TOPIC_OTA_STATUS,&status);
					LOG_DEBUG("OTA: Download Success!");
					/* restart */
					xEventGroupWaitBits(xGlobalEventGroup,EVENT_OTA_SYSTEMRESET,pdFALSE,pdTRUE,portMAX_DELAY);
					LOG_DEBUG("Rebooting in 1s...");
					vTaskDelay(pdMS_TO_TICKS(1000));
					NVIC_SystemReset();
				}
			}
			else
			{
				status=OTA_STATUS_FAILED;
				Broker_Publish(TOPIC_OTA_STATUS,&status);
				need_reconnect = 1;
				LOG_DEBUG("Download Firmware check error");
			}
		}
		else
		{
			LOG_DEBUG("OTA: Download Failed! Dropped.");
		}
		vTaskDelay(pdMS_TO_TICKS(2000));
		status=OTA_STATUS_IDLE;
		Broker_Publish(TOPIC_OTA_STATUS,&status);
	}
}