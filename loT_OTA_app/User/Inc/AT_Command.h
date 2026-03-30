#ifndef __AT_COMMAND_H
#define __AT_COMMAND_H
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

#define IP_LENGTH 				15
#define AT_DEVICE_SOCKET_NUM 	5
#define AT_DATA_MAX_LEN      	2048
#define AT_RX_BUF_SIZE       	2048
#define RX_TRIGGERLEVEL      	1

typedef enum AT_SocketType
{
    AT_SOCK_STREAM = 1,        /* TCP */
    AT_SOCK_DGRAM              /* UDP */
} AT_SocketType;
typedef enum
{
	Rx_OK=0,
	Rx_ERR
}rx_state;
typedef struct Sockaddr
{
	uint16_t port; 			/*端口*/
	uint32_t ip; 			/*ip*/
}Sockaddr;

typedef struct _AT_Device AT_Device;
typedef struct _AT_Device
{
	char* name;
	int (*Init)(AT_Device* self,char* uart_name);
	int (*allocate)(AT_Device* self,Sockaddr* local,AT_SocketType type);
	int (*connect)(AT_Device* self,int sockfd,Sockaddr* remote);
	int (*Send)(AT_Device* self,const char* cmd,uint16_t size,const char* expected_resp,uint32_t timeout);
	int (*close)(AT_Device* self,int sockfd);
	uint8_t (*state)(AT_Device* self,int sockfd);
	void* pri_data;
}AT_Device;


extern AT_Device at_esp8266;

void AT_Recv_Task(void* arg);

AT_SocketType Get_SockType_FromATDev(AT_Device* at_dev,int sockfd);
StreamBufferHandle_t* Get_Stream_FromATDev(AT_Device* at_dev,int sockfd);

#endif