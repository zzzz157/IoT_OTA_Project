#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "Task.h"
#include "Semphr.h"
#include "stream_buffer.h"
#include "ESP8266.h"
#include "AT_Command.h"
#include "main.h"
static AT_Device* sock_esp8266=&at_esp8266;
static Sockaddr local_addr={.port=8181,.ip=(10<<24)|(177<<16)|(21<<8)|11};
/* 内部分配结构体 */
int esp8266_socket(int domain, int type, int protocol)
{
	AT_SocketType sock_type=type;
	int sockfd =sock_esp8266->allocate(sock_esp8266,&local_addr,sock_type);
	return sockfd;
}
/* 连接 */
int esp8266_connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen)
{
	struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
	Sockaddr remote;
	char cmd[64];
	remote.port=htons(addr_in->sin_port);
	remote.ip=addr_in->sin_addr.s_addr;
	if(sock_esp8266->connect(sock_esp8266,sockfd,&remote)!=0) return -1;
	uint8_t *ip_bytes = (uint8_t *)&addr_in->sin_addr.s_addr;
	AT_SocketType socktype=Get_SockType_FromATDev(sock_esp8266,sockfd);
	char ip[16];
	snprintf(ip, sizeof(ip), "%d.%d.%d.%d", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
	if(socktype==AT_SOCK_STREAM)
	{
		snprintf(cmd, sizeof(cmd), "AT+CIPSTART=%d,\"TCP\",\"%s\",%d\r\n", sockfd, ip, remote.port);
		//snprintf(cmd, sizeof(cmd), "AT+CIPSTART=%d,\"TCP\",\"broker.emqx.io\",%d\r\n", sockfd, remote.port);
	}
	else if(socktype==AT_SOCK_DGRAM)
	{
		snprintf(cmd, sizeof(cmd), "AT+CIPSTART=%d,\"UDP\",\"%s\",%d,%d,0\r\n", 
             sockfd, ip, remote.port, local_addr.port);
	}
	if (sock_esp8266->Send(sock_esp8266, cmd,strlen(cmd) ,"OK", 5000) == Rx_ERR) 
	{
        return -1;
    }
	return 0;
}
/* 初始化：联网 */
int esp8266_init(void* at_device,char* wifi_name,char* wifi_password)
{
	AT_Device* at_dev=(AT_Device*)at_device;
	LOG_DEBUG("init Start");
	//vTaskDelay(pdMS_TO_TICKS(7000));
	if(at_dev->Send(at_dev, "AT+RST\r\n", 8, "OK", 5000)!=Rx_OK)
	{
		LOG_DEBUG("AT+RST Timeout");
	}
	vTaskDelay(pdMS_TO_TICKS(3000));
	//at_dev->Send(at_dev, "\r\n\r\n", 4, "OK", 1000);
	while(at_dev->Send(at_dev, "ATE0\r\n", 6, "OK", 2000) != Rx_OK)
    {
        LOG_DEBUG("ATE0 ERR, check wire or power...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
	vTaskDelay(pdMS_TO_TICKS(2000));
	if(at_dev->Send(at_dev, "AT+CWMODE=1\r\n", 13, "OK", 2000)!=Rx_OK) 
	{
		LOG_DEBUG("AT+CWMODE ERR");
		return -1;
	}
	if(at_dev->Send(at_dev, "AT+CIPMUX=1\r\n", 13, "OK", 2000)!=Rx_OK) 
	{
		LOG_DEBUG("AT+CIPMUX ERR");
		return -1;
	}
	char wifi_cmd[64];
	snprintf(wifi_cmd, sizeof(wifi_cmd),"AT+CWJAP=\"%s\",\"%s\"\r\n", wifi_name,wifi_password);
	if (at_dev->Send(at_dev, wifi_cmd, strlen(wifi_cmd), "OK", 10000) != Rx_OK)
	{
		LOG_DEBUG("CONNECTED ERR");
		return -1;
	}
	return 0;
}
uint8_t esp8266_state(int sockfd)
{
	if (sock_esp8266 != NULL && sock_esp8266->state != NULL)
	{
		return sock_esp8266->state(sock_esp8266,sockfd);
	}
	return 0;
}
/* 发送数据 */
int esp8266_send(int sockfd, const void *buf, size_t len, int flags)
{
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d,%d\r\n", sockfd,len);
	if (sock_esp8266->Send(sock_esp8266, cmd,strlen(cmd), ">", 5000) == Rx_ERR)
	{
        return -1;
    }
	if (sock_esp8266->Send(sock_esp8266,buf,len,"OK",5000) == Rx_ERR)
	{
        return -1;
    }
	return len;
}
/* 接收数据 */
int esp8266_recv(int sockfd, void *buf, size_t len, int flags)
{
	StreamBufferHandle_t* rx_stream=Get_Stream_FromATDev(sock_esp8266,sockfd);
	size_t rx_len=xStreamBufferReceive(*rx_stream,buf,len,pdMS_TO_TICKS(flags));
	if(rx_len>0) return rx_len;
	return -1;
}
/* 关闭sockfd连接 */
int esp8266_close(int sockfd)
{
	char cmd[32];
	if (sock_esp8266->state(sock_esp8266, sockfd) == 1)
    {
        snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE=%d\r\n", sockfd);
        sock_esp8266->Send(sock_esp8266, cmd, strlen(cmd), "OK", 1000);
    }
	else
	{
		LOG_DEBUG("Socket %d already closed by peer, skip AT+CIPCLOSE", sockfd);
	}
	return sock_esp8266->close(sock_esp8266, sockfd);
}
