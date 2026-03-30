#include "W25Q64.h"
#include "OTA.h"
#include "Socket.h"
#include "main.h"
#include "Broker.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static W25Q64_t* Flash_Device=NULL;

static uint32_t crc32_calculate(uint32_t crc, const uint8_t *buf, uint32_t len) 
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


void OTA_Task(void* arg)
{
	LOG_DEBUG("OTA start");
	QueueHandle_t ota_queue=xQueueCreate(1,sizeof(OTA_Config_t));
	Broker_Subscribe(TOPIC_OTA_DATA,ota_queue);
	
	OTA_Init(&W25QHandle_t);
	static uint8_t ota_down_buf[OTA_DOWN_RXBUF_LEN*2];
	while(1)
	{
		OTA_Config_t cfg;
		xQueueReceive(ota_queue,&cfg,portMAX_DELAY);
		LOG_DEBUG("OTA upgrade start to run");
		uint32_t received_len = 0;
		uint32_t content_length = 0xFFFFFFFF;
		uint32_t fw_crc = 0;
		last_erased_sector = 0xFFFFFFFF;
		int fd_ota = -1;
		uint8_t need_reconnect = 1;
		while(received_len < content_length)
		{
			if(need_reconnect)
			{
				/* allocate struct and set loacal*/
				if (fd_ota >= 0) 
				{
					close(fd_ota);
					fd_ota = -1;
				}
				fd_ota=socket(AF_INET, SOCK_STREAM, 0);
				if (fd_ota < 0) 
				{
					vTaskDelay(pdMS_TO_TICKS(1000));
					continue;
				}
				/* set remote and connect */
				struct sockaddr_in server_addr;
				server_addr.sin_family = AF_INET;
				server_addr.sin_port = htons(cfg.port);
				server_addr.sin_addr.s_addr = IP4_ADDR(cfg.ip[0], cfg.ip[1], cfg.ip[2], cfg.ip[3]);
				LOG_DEBUG("OTA: Connecting to HTTP Server...");
				if (connect(fd_ota, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) 
				{
					need_reconnect = 1;
					LOG_DEBUG("OTA: Connect Failed");
					close(fd_ota);
					fd_ota = -1;
					continue;
				}
				need_reconnect = 0;
				LOG_DEBUG("OTA: Connecting Successfull");
			}
			uint32_t req_start = received_len;
			uint32_t req_end = req_start + OTA_DOWN_RXBUF_LEN - 1;
			/* send request to wait response */
			char req[256];
			snprintf(req, sizeof(req), 
					"GET %s HTTP/1.1\r\n"
					"Host: %d.%d.%d.%d:%d\r\n"
					"Range: bytes=%u-%u\r\n"
					"Accept: application/octet-stream\r\n"
					"Connection: keep-alive\r\n\r\n",
					cfg.path, cfg.ip[0], cfg.ip[1], cfg.ip[2], cfg.ip[3], cfg.port,
					req_start, req_end);
			if (send(fd_ota, req, strlen(req), 0) <= 0)
			{
				need_reconnect = 1;
				continue;
			}
			/* deal HTTP head */
			char header_line[128];
			int line_len = 0;
			uint8_t header_end = 0;
			uint8_t is_ok = 0;
			uint8_t is_200 = 0;
			uint32_t current_chunk_size = 0;
			while(!header_end)
			{
				char c;
				if (recv(fd_ota, &c, 1, 5000) <= 0) 
				{
					need_reconnect = 1; 
					break;
				}
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
					    if (strstr(header_line, "200")) is_200 = 1;
						if (is_200|| strstr(header_line, "206"))
						{
							is_ok = 1;
						}
					}
					/* current head end */
					if(line_len==2&&header_line[0] == '\r')
					{
						header_end = 1;
						break;
					}
					/* get data len */
					if (strncasecmp_custom(header_line, "Content-Length:", 15) == 0) 
					{
						current_chunk_size = atoi(header_line + 15);
						if (content_length == 0xFFFFFFFF && is_200)
						{
                            content_length = current_chunk_size;
                        }
					}
					if (strncasecmp_custom(header_line, "Content-Range:", 14) == 0)
                    {
                        char *slash = strchr(header_line, '/');
                        if (slash) 
                        {
                            content_length = atoi(slash + 1);
                        }
                    }
					/* prepare next line */
					line_len = 0;
				}
				if (line_len >= sizeof(header_line) - 1) line_len = 0;
			}
			if (!is_ok || !header_end || current_chunk_size == 0)
            {
                LOG_DEBUG("OTA: HTTP Header Error, reconnecting...");
                need_reconnect = 1;
                continue;
            }
			if (is_200 && received_len > 0) 
			{
                LOG_DEBUG("Server doesn't support Range, reset download!");
                received_len = 0; 
                fw_crc = 0;
                last_erased_sector = 0xFFFFFFFF;
            }
			
			uint32_t chunk_received = 0;
			while (chunk_received < current_chunk_size) 
            {
                int expect = current_chunk_size - chunk_received;
                if (expect > OTA_DOWN_RXBUF_LEN) 
                {
                    expect = OTA_DOWN_RXBUF_LEN;
                }
                int len = recv(fd_ota, ota_down_buf, expect, 5000);
                if (len <= 0)
                {
                    need_reconnect = 1;
                    break; 
                }
                fw_crc = crc32_calculate(fw_crc, ota_down_buf, len);
                if(OTA_Write_Flash(received_len, ota_down_buf, len)!=1)
				{
					LOG_DEBUG("OTA_Write_Flash ERROR\r\n");
					need_reconnect = 1;
					break;
				}
                received_len += len;
                chunk_received += len;
            }
			LOG_DEBUG("OTA Progress: %d / %d", received_len, 
				(content_length != 0xFFFFFFFF) ? content_length:0);
		}
		if (fd_ota >= 0) 
		{
			close(fd_ota);
			fd_ota = -1;
		}
		if (content_length != 0xFFFFFFFF && received_len == content_length)
		{
			uint32_t flag=OTA_UPGRADE_DOING;
			if(Flash_Device->SectorErase(Flash_Device,OTA_FLAG_ADDR & 0xFFFFF000)!=1)
			{
				LOG_DEBUG("OTA Flag SectorErase ERROR");
			}
			if(Flash_Device->WritePage(Flash_Device,OTA_FLAG_ADDR,(uint8_t*)&flag,
				sizeof(flag))!=1)
			{
				LOG_DEBUG("OTA Flag Write ERROR");
			}
			/* restart */
			LOG_DEBUG("OTA: Download Success! Rebooting in 1s...");
			vTaskDelay(pdMS_TO_TICKS(1000));
			NVIC_SystemReset();
		}
		else
		{
			LOG_DEBUG("OTA: Download Failed! Dropped.");
		}
	}
}