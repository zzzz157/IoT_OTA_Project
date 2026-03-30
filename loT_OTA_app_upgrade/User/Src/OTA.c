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
static void OTA_Write_Flash(uint32_t offset, uint8_t *data, uint32_t len)
{
	uint32_t write_addr=OTA_APP_START_ADDR+offset;
	uint32_t remain = len;
    uint8_t* ptr = data;
	while(remain>0)
	{
		/* erase current sector */
		uint32_t current_sector = write_addr & 0xFFFFF000;
		if(current_sector!=last_erased_sector)
		{
			Flash_Device->SectorErase(Flash_Device,current_sector);
			last_erased_sector=current_sector;
		}
		
		/* write flash page */
		uint32_t page_remain = W25Q64_PAGE_LEN-(write_addr%W25Q64_PAGE_LEN);
        uint32_t write_len = (remain <= page_remain) ? remain : page_remain;
		
		Flash_Device->WritePage(Flash_Device,write_addr,ptr,write_len);
		
		write_addr+=write_len;
		ptr+=write_len;
		remain-=write_len;
	}
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
	static uint8_t ota_down_buf[OTA_DOWN_RXBUF_LEN];
	while(1)
	{
		OTA_Config_t cfg;
		xQueueReceive(ota_queue,&cfg,portMAX_DELAY);
		/* allocate struct and set loacal*/
		int fd_ota = socket(AF_INET, SOCK_STREAM, 0);
		if (fd_ota < 0) continue;
		
		/* set remote and connect */
		struct sockaddr_in server_addr;
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(cfg.port);
		server_addr.sin_addr.s_addr = IP4_ADDR(cfg.ip[0], cfg.ip[1], cfg.ip[2], cfg.ip[3]);
		LOG_DEBUG("OTA: Connecting to HTTP Server...");
		if (connect(fd_ota, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) 
		{
			LOG_DEBUG("OTA: Connect Failed");
			close(fd_ota);
			continue;
		}
		LOG_DEBUG("OTA: Connecting Successfull");
		
		/* send request to wait response */
		char req[256];
		snprintf(req, sizeof(req), 
				"GET %s HTTP/1.0\r\n"
				"Host: %d.%d.%d.%d:%d\r\n"
				"Accept: application/octet-stream\r\n"
				"Connection: close\r\n\r\n", 
				cfg.path, cfg.ip[0], cfg.ip[1], cfg.ip[2], cfg.ip[3], cfg.port);
		send(fd_ota, req, strlen(req), 0);
		LOG_DEBUG("OTA: HTTP Request Sent.");
		
		/* deal HTTP head */
		char header_line[128];
		int line_len = 0;
		uint8_t header_end = 0;
		uint32_t content_length = 0;
		uint8_t is_200_ok = 0;
		while (!header_end)
		{
			char c;
			/* start to recv */
			if (recv(fd_ota, &c, 1, 5000) <= 0) break;
			header_line[line_len++] = c;
			/* sign of line's end */
			if(c=='\n')
			{
				header_line[line_len] = '\0';
				/* is normal response ? */
				if (strstr(header_line, "HTTP/") != NULL && strstr(header_line, "200 OK") != NULL) 
				{
					is_200_ok = 1;
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
					content_length = atoi(header_line + 15);
				}
				/* prepare next line */
				line_len = 0;
			}
			if (line_len >= sizeof(header_line) - 1) line_len = 0;
		}
		if(content_length == 0 || !is_200_ok || !header_end)
		{
			LOG_DEBUG("OTA: HTTP Error or File Not Found! Abort.");
			close(fd_ota);
			continue;
		}
		LOG_DEBUG("OTA: Start Downloading %d Bytes...", content_length);
		
		/* deal data and download start */
		uint32_t received_len = 0;
		last_erased_sector = 0xFFFFFFFF;
		uint32_t fw_crc = 0;
		/* deal data */
		while (received_len < content_length)
		{
			int expect = content_length - received_len;
			if(expect > OTA_DOWN_RXBUF_LEN) expect = OTA_DOWN_RXBUF_LEN;
			int len = recv(fd_ota, ota_down_buf, expect, 5000);
			if (len <= 0)
			{
				LOG_DEBUG("OTA: Network Error!");
				break;
			}
			fw_crc = crc32_calculate(fw_crc, ota_down_buf, len);
			/* download */
			OTA_Write_Flash(received_len, ota_down_buf, len);
			received_len += len;
			LOG_DEBUG("OTA Progress: %d / %d", received_len, content_length);
		}
		/* down end */
		close(fd_ota);
		/* checksum and restart */
		if (received_len == content_length) 
		{
			/*写入一个 OTA_FLAG (如 0xAA55)，告知 Bootloader 有新固件需要搬运*/
			uint32_t flag=OTA_UPGRADE_DOING;
			Flash_Device->SectorErase(Flash_Device,OTA_FLAG_ADDR & 0xFFFFF000);
			Flash_Device->WritePage(Flash_Device,OTA_FLAG_ADDR,(uint8_t*)&flag,sizeof(flag));
			header_t hdr;
			memset(&hdr, 0, sizeof(header_t));
			hdr.ih_magic = IH_MAGIC;
			hdr.ih_size  = content_length;
			hdr.ih_load  = APP_LOAD_ADDRESS;
			hdr.ih_ep    = APP_EP_ADDRESS;
			hdr.ih_dcrc  = fw_crc;
			hdr.ih_os    = 0;
			hdr.ih_arch  = 2;
			hdr.ih_type  = 5;
			hdr.ih_comp  = 0;
			strncpy((char*)hdr.ih_name, "STM32_APP_OTA", IH_NMLEN);
			hdr.ih_hcrc = 0;
			hdr.ih_hcrc = crc32_calculate(0, (uint8_t*)&hdr, sizeof(header_t));
			Flash_Device->WritePage(Flash_Device, OTA_HEAD_START_ADDR,
				(uint8_t*)&hdr, sizeof(header_t));
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