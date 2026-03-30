#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "Semphr.h"
#include "stream_buffer.h"
#include "main.h"
#include "UART_OOP.h"
#include "AT_Command.h"

typedef struct AT_Socket
{
	AT_SocketType type; /*TCP or UDP*/
	Sockaddr local;  	/*本地 ip port*/
	Sockaddr remote; 	/*远端 ip port*/
	StreamBufferHandle_t rx_stream; /* data接收流 */
	uint8_t is_connected; /* 连接状态 */
}AT_Socket;
typedef struct
{
	UART_Device* uart_dev;
	AT_Socket* socket[AT_DEVICE_SOCKET_NUM];
	/* 互斥/同步 */
	SemaphoreHandle_t at_lock;
	SemaphoreHandle_t at_resp_sem;
	/*接收*/
	StreamBufferHandle_t uart_rxstream; /* reply + data 接收流 */
	int transparent_rx_len;
	int rx_idx;
	int current_link_id;
	uint8_t rx_state;
	const char* expected_resp;
}AT_config;
static const AT_Device* Get_ATdev_From_UARTdev(UART_Device* uart_dev);
/* 错误回调 */
static void AT_ErrorCallback(UART_HandleTypeDef *huart)
{
	__HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
	uint8_t* buf=Get_dmabuf_FromUART(huart);
	if(buf==NULL) return ;
	HAL_UART_AbortReceive(huart);
	huart->Lock = HAL_UNLOCKED;
	HAL_UARTEx_ReceiveToIdle_DMA(huart, buf, UART_RX_MAX_LEN);
    __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
}
/* dma空闲中断回调 */
static void AT_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	UART_Device* uart_dev=Get_UARTDev_FromUart(huart);
	const AT_Device* at_dev=Get_ATdev_From_UARTdev(uart_dev);
	AT_config* cfg=at_dev->pri_data;
	uint8_t* buf=Get_dmabuf_FromUART(huart);
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xStreamBufferSendFromISR(cfg->uart_rxstream,buf,Size,&xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	huart->Lock = HAL_UNLOCKED;
	HAL_UARTEx_ReceiveToIdle_DMA(huart, buf, UART_RX_MAX_LEN);
	__HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
}
/* 通过AT_Device sockfy 得到 连接类型 */
AT_SocketType Get_SockType_FromATDev(AT_Device* at_dev,int sockfd)
{
	AT_config* cfg=at_dev->pri_data;
	return cfg->socket[sockfd]->type;
}
/* 通过AT_Device sockfy 得到 数据接收流缓冲区 */
StreamBufferHandle_t* Get_Stream_FromATDev(AT_Device* at_dev,int sockfd)
{
	AT_config* cfg=at_dev->pri_data;
	return &cfg->socket[sockfd]->rx_stream;
}
/* AT_Socket结构体预分配 */
static AT_Socket pre_socket[AT_DEVICE_SOCKET_NUM]={NULL};
/* 分配AT_Socket结构体  注入本地ip port type */ 
static int allocate_socket(AT_Device* self,Sockaddr* local,AT_SocketType type)
{
	int socket_idx=-1;
	AT_config* cfg=self->pri_data;
	for(uint8_t i=0;i<AT_DEVICE_SOCKET_NUM;i++)
	{
		if(cfg->socket[i]==NULL)
		{
			cfg->socket[i]=&pre_socket[i];
			cfg->socket[i]->local.port=local->port;
			cfg->socket[i]->local.ip=local->ip;
			cfg->socket[i]->type=type;
			cfg->socket[i]->is_connected=0;
			if (cfg->socket[i]->rx_stream == NULL) 
			{
				cfg->socket[i]->rx_stream = xStreamBufferCreate(AT_RX_BUF_SIZE,RX_TRIGGERLEVEL);
			}
			else 
			{
				xStreamBufferReset(cfg->socket[i]->rx_stream);
			}
			socket_idx=i;
			break;
		}
	}
	return socket_idx;
}
/* 连接 对sockfd连接注入远端ip port */
static int connect_socket(AT_Device* self,int sockfd,Sockaddr* remote)
{
	AT_config* cfg=self->pri_data;
	if(cfg->socket[sockfd]==NULL) return -1;
	cfg->socket[sockfd]->remote.port=remote->port;
	cfg->socket[sockfd]->remote.ip=remote->ip;
	cfg->socket[sockfd]->is_connected=1; /* 标记已连接 */
	return 0;
}
/* 获取sockfd连接状态 */
uint8_t state_socket(AT_Device* self,int sockfd)
{
	AT_config* cfg=self->pri_data;
	if(cfg->socket[sockfd]->is_connected==1)
	{
		return 1;
	}
	return 0;
}
/* 关闭sockfd连接：清理sockfd的AT_Device指针 */
static int close_socket(AT_Device* self,int sockfd)
{
	AT_config* cfg=self->pri_data;
	if(cfg->socket[sockfd]==NULL) return -1;
	cfg->socket[sockfd]=NULL;
	return 0;
}
/* 初始化：绑定uart设备 回调指针 同步 */
int at_init(AT_Device* self,char* uart_name)
{
	AT_config* cfg=self->pri_data;
	cfg->uart_dev=GetUARTDevice(uart_name);
	if(cfg->uart_dev==NULL) return 1;
	cfg->uart_dev->err_cb=AT_ErrorCallback;
	cfg->uart_dev->rx_event_cb=AT_UARTEx_RxEventCallback;
	cfg->uart_dev->Init(cfg->uart_dev);
	if(cfg->at_lock==NULL) cfg->at_lock=xSemaphoreCreateMutex();
	if(cfg->at_resp_sem==NULL) cfg->at_resp_sem=xSemaphoreCreateBinary();
	if(cfg->uart_rxstream==NULL) cfg->uart_rxstream=xStreamBufferCreate(AT_RX_BUF_SIZE,RX_TRIGGERLEVEL);
	if(cfg->at_lock==NULL||cfg->at_resp_sem==NULL||cfg->uart_rxstream==NULL) return 1;
	cfg->transparent_rx_len=0;
	cfg->current_link_id=-1;
	cfg->rx_idx=0;
	int start_rcev=cfg->uart_dev->RecvByte(cfg->uart_dev, NULL, 0, 0);
	assert_param(start_rcev==0);
	return 0;
}
/* 接收前初始化：清除状态 */
static void at_rx_init(AT_Device* self,const char* expected_resp)
{
	AT_config* cfg=self->pri_data;
	cfg->rx_state=Rx_ERR;
	cfg->expected_resp = expected_resp;
}
/* 发送指令cmd */
int at_send(AT_Device* self,const char* cmd,uint16_t size ,const char* expected_resp, uint32_t timeout)
{
	AT_config* cfg=self->pri_data;
	if(xSemaphoreTake(cfg->at_lock,pdMS_TO_TICKS(timeout))!=pdTRUE) return Rx_ERR;
	at_rx_init(self,expected_resp);
	xSemaphoreTake(cfg->at_resp_sem, 0);
	if(cfg->uart_dev->Send(cfg->uart_dev,(uint8_t*)cmd,size,timeout)!=0)
	{
		LOG_DEBUG("UART TX Failed!");
		xSemaphoreGive(cfg->at_lock);
		return Rx_ERR;
	}
	if(xSemaphoreTake(cfg->at_resp_sem,pdMS_TO_TICKS(timeout))==pdFALSE)
	{
		cfg->rx_state=Rx_ERR;
		cfg->expected_resp=NULL;
	}
	xSemaphoreGive(cfg->at_lock);
	return cfg->rx_state;
}
static uint8_t rx_buf[AT_RX_BUF_SIZE];
/* 监听RX */
void AT_Recv_Task(void* arg)
{
	LOG_DEBUG("AT_Recv_Task");
	AT_Device* at_dev=(AT_Device*)arg;
	AT_config* cfg=at_dev->pri_data;
	at_dev->Init(at_dev,"uart2_dma");
	while(1)
	{
		/* 监听RX：判断并解析 回复or数据 */
		size_t len=xStreamBufferReceive(cfg->uart_rxstream,rx_buf+cfg->rx_idx,
			sizeof(rx_buf)-1-cfg->rx_idx,portMAX_DELAY);
		
		if(len>0)
		{
			cfg->rx_idx+=len;
			rx_buf[cfg->rx_idx] ='\0';
			LOG_DEBUG("RAW RX: %s", rx_buf + cfg->rx_idx - len);
			/* 旧数据 */
			if(cfg->transparent_rx_len>0)
			{
				size_t push_len = (len > cfg->transparent_rx_len) ? cfg->transparent_rx_len : len;
				if(cfg->socket[cfg->current_link_id] != NULL && 
					cfg->socket[cfg->current_link_id]->rx_stream != NULL)
				{
					xStreamBufferSend(cfg->socket[cfg->current_link_id]->rx_stream,rx_buf,
						push_len,portMAX_DELAY);
				}
				cfg->transparent_rx_len-=push_len;
				if (len == push_len)
				{
					cfg->rx_idx=0;
					continue;
				}
				else
				{
					memmove(rx_buf, rx_buf + push_len, len - push_len);
					cfg->rx_idx -= push_len;
					rx_buf[cfg->rx_idx] = '\0';
				}
			}
			char* ipd_ptr = strstr((char*)rx_buf, "+IPD");
			/* 新数据 */
			if(ipd_ptr!=NULL)
			{
				int use_len=(uint8_t*)ipd_ptr-rx_buf;
				int link_id = 0;
				int data_len = 0;
				if(sscanf(ipd_ptr,"+IPD,%d,%d:",&link_id,&data_len)==2)
				{
					cfg->current_link_id=link_id;
					uint8_t* data_ptr = (uint8_t*)strchr(ipd_ptr, ':');
					if(data_ptr!=NULL)
					{
						data_ptr++;
						int heade_len=data_ptr-(uint8_t*)ipd_ptr;
						if(link_id>=0&&link_id<AT_DEVICE_SOCKET_NUM)
						{
							int valid_len = (rx_buf + cfg->rx_idx) - (uint8_t*)data_ptr;
							int push_len = (valid_len >= data_len) ? data_len : valid_len;
							if(valid_len >0&&cfg->socket[link_id]->rx_stream!=NULL)
							{
								xStreamBufferSend(cfg->socket[link_id]->rx_stream,data_ptr,
									push_len,portMAX_DELAY);
								cfg->transparent_rx_len=data_len-push_len;
							}
							if(push_len==data_len)
							{
								cfg->rx_idx-=(heade_len+push_len);
								memmove(rx_buf+use_len, rx_buf + use_len+heade_len+push_len,
									valid_len-push_len);
								rx_buf[cfg->rx_idx] = '\0';
							}
							else
							{
								cfg->rx_idx=use_len;
								rx_buf[cfg->rx_idx] = '\0';
							}
						}
					}
				}
			}
			/* 找到期待回复的char* 回复 */
			if(cfg->expected_resp!=NULL&&strstr((char*)rx_buf, cfg->expected_resp)!=NULL)
			{
				cfg->rx_state = Rx_OK;
				cfg->expected_resp = NULL;
				xSemaphoreGive(cfg->at_resp_sem);
				cfg->rx_idx = 0;
				rx_buf[0] = '\0';
			}
			/* 模块报错 */
			else if(strstr((char*)rx_buf, "ERROR")!=NULL || strstr((char*)rx_buf, "FAIL") != NULL)
			{
				LOG_DEBUG("AT CMD ERR, ESP8266 Reply: %s", rx_buf);
				cfg->rx_state = Rx_ERR;
				cfg->expected_resp = NULL;
				xSemaphoreGive(cfg->at_resp_sem);
				cfg->rx_idx = 0;
			}
			/* 断开连接 */
			else if(strstr((char*)rx_buf, "CLOSED") != NULL)
			{
				int link_id = -1;
				if (sscanf((char*)rx_buf, "%d,CLOSED", &link_id) == 1)
				{
					if (link_id >= 0 && link_id < AT_DEVICE_SOCKET_NUM && cfg->socket[link_id] != NULL)
					{
						LOG_DEBUG("Link %d Closed by peer/network!", link_id);
						cfg->socket[link_id]->is_connected = 0; /* 标记连接断开 */
					}
				}
			}
			if(cfg->rx_idx >= sizeof(rx_buf) - 64) cfg->rx_idx=0;
        }
	}
}
static AT_config at_cfg={NULL};
AT_Device at_esp8266={
	.name="esp8266",
	.Init=at_init,
	.Send=at_send,
	.allocate=allocate_socket,
	.connect=connect_socket,
	.state=state_socket,
	.close=close_socket,
	.pri_data=&at_cfg,
};
static const AT_Device* AT_Dev[]={&at_esp8266,NULL};

static const AT_Device* Get_ATdev_From_UARTdev(UART_Device* uart_dev)
{
	for(uint8_t i=0;AT_Dev[i]!=NULL;i++)
	{
		AT_config* cfg=AT_Dev[i]->pri_data;
		if(cfg!=NULL&&cfg->uart_dev==uart_dev)
		{
			return AT_Dev[i];
		}
	}
	return NULL;
}