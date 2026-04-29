#include <stdio.h>
#include <string.h>
#include "main.h"
#include "UART_OOP.h"
#include "usart.h"
#if UART_USE_FREERTOS
#include "FreeRTOS.h"
#include "Semphr.h"
#endif
typedef struct
{
	UART_HandleTypeDef* uart;
	uint8_t* at_dma_rx_buf;
}UART_config;
//轮询
static int uart_init(UART_Device* self)
{
	UART_config* cfg = self->priv_data;
	if (cfg != NULL && cfg->at_dma_rx_buf == NULL) 
    {
        cfg->at_dma_rx_buf = (uint8_t*)pvPortMalloc(UART_RX_MAX_LEN);
        if (cfg->at_dma_rx_buf == NULL) {
            return -1;
        }
    }
    return 0;
}
static int uart_send(UART_Device* self, uint8_t *datas, uint32_t len, int timeout)
{
    UART_config* cfg = self->priv_data;
    UART_HandleTypeDef *huart = cfg->uart;
    if (HAL_OK == HAL_UART_Transmit(huart, datas, len, timeout))
        return 0;
    else
        return -1;
}
static int uart_recv(UART_Device* self, uint8_t *data,uint16_t len, int timeout)
{
    UART_config* cfg = self->priv_data;
    UART_HandleTypeDef *huart = cfg->uart;
    if (HAL_OK == HAL_UART_Receive(huart, data, 1, timeout))
        return 0;
    else
        return -1;
}
static int uart_flush(UART_Device* self)
{
    return 0;
}
//中断_立即返回
static int uart_send_it(UART_Device* self, uint8_t *datas, uint32_t len, int timeout)
{
    UART_config* cfg = self->priv_data;
    uint32_t start = HAL_GetTick(); 
    while (cfg->uart->gState != HAL_UART_STATE_READY)
    {
        if (HAL_GetTick() - start > timeout) return -1;
        vTaskDelay(1);
    }
    if (HAL_OK == HAL_UART_Transmit_IT(cfg->uart, datas, len))
        return 0;
    else
        return -1;
}
static int uart_recv_it(UART_Device* self, uint8_t *data,uint16_t len, int timeout)
{
	UART_config* cfg = self->priv_data;
    UART_HandleTypeDef *huart = cfg->uart;
    if (HAL_OK == HAL_UART_Receive_IT(huart, data, 1))
        return 0;
    else
        return -1;
}
//DMA_立即返回
static int uart_send_dma(UART_Device* self, uint8_t *datas, uint32_t len, int timeout)
{
    UART_config* cfg = self->priv_data;
    if (HAL_OK == HAL_UART_Transmit_DMA(cfg->uart, datas, len))
        return 0;
    else
        return -1;
}
static int uart_recv_dma(UART_Device* self, uint8_t *data,uint16_t len, int timeout)
{
    UART_config* cfg = self->priv_data;
	__HAL_UART_CLEAR_OREFLAG(cfg->uart);
    __HAL_UART_CLEAR_FEFLAG(cfg->uart);
    __HAL_UART_CLEAR_NEFLAG(cfg->uart);
    __HAL_UART_CLEAR_PEFLAG(cfg->uart);
    __HAL_UART_CLEAR_IDLEFLAG(cfg->uart);
    if (HAL_OK == HAL_UARTEx_ReceiveToIdle_DMA(cfg->uart,cfg->at_dma_rx_buf,UART_RX_MAX_LEN))
	{
		__HAL_DMA_DISABLE_IT(cfg->uart->hdmarx, DMA_IT_HT);
		return 0;
	}
        
    else
        return -1;
}
//Device
static UART_config uart1_cfg={
	.uart=&huart1
};
static UART_config uart2_cfg={
	.uart=&huart2
};
static UART_config uart2_dma_cfg={
	.uart=&huart2,
	.at_dma_rx_buf=NULL,
};

static UART_Device uart1_dev = {
	.name="uart1",
	.Init=uart_init,
	.Send=uart_send,
	.RecvByte=uart_recv,
	.priv_data=&uart1_cfg,
};
static UART_Device uart2_dev = {
	.name="uart2",
	.Init=uart_init,
	.Send=uart_send,
	.RecvByte=uart_recv,
	.priv_data=&uart2_cfg,
};
static UART_Device uart1_it_dev = {
	.name="uart1_it",
	.Init=uart_init,
	.Send=uart_send_it,
	.RecvByte=uart_recv_it,
	.priv_data=&uart1_cfg
};
static UART_Device uart2_dma_dev = {
	.name="uart2_dma",
	.Init=uart_init,
	.Send=uart_send_it,
	.RecvByte=uart_recv_dma,
	.priv_data=&uart2_dma_cfg
};
static UART_Device *uart_devices[] = {&uart1_it_dev,&uart2_dma_dev};


UART_Device* GetUARTDevice(char *name)
{
	int i = 0;
	for (i = 0; i < sizeof(uart_devices)/sizeof(uart_devices[0]); i++)
	{
		if (!strcmp(name, uart_devices[i]->name))
			return uart_devices[i];
	}
	
	return NULL;
}
UART_HandleTypeDef* Get_UART_Handle(UART_Device* pdev)
{
	if(pdev==NULL||pdev->priv_data==NULL) return NULL;
	UART_config* cfg=pdev->priv_data;
	return cfg->uart;
}

UART_Device* Get_UARTDev_FromUart(UART_HandleTypeDef *huart)
{
	for(uint8_t i=0;i<sizeof(uart_devices)/sizeof(uart_devices[0]);i++)
	{
		UART_config* cfg = (UART_config*)uart_devices[i]->priv_data;
        if(cfg != NULL && cfg->uart == huart)
		{
			return uart_devices[i];
		}
	}
	return NULL;
}
uint8_t* Get_dmabuf_FromUART(UART_HandleTypeDef *huart)
{
	UART_Device* pdev=Get_UARTDev_FromUart(huart);
	if(pdev!=NULL&&pdev->priv_data!=NULL)
	{
		UART_config* cfg=pdev->priv_data;
		return cfg->at_dma_rx_buf;
	}
	return NULL;
}

static uint8_t* log_buffer = NULL;
static volatile uint16_t buf_head = 0; // 写指针
static volatile uint16_t buf_tail = 0; // 读指针
static volatile uint8_t  is_sending = 0; // 发送状态标志
static volatile uint16_t current_send_len = 0;
static void log_start_transmit();
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	//LOG_DEBUG("Rx Callback");
	UART_Device* dev = Get_UARTDev_FromUart(huart);
	if(dev!=NULL&&dev->rx_event_cb!=NULL)
	{
		dev->rx_event_cb(huart,Size);
	}
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	UART_Device* dev = Get_UARTDev_FromUart(huart);
	if(dev!=NULL&&dev->rx_cplt_cb!=NULL)
	{
		dev->rx_cplt_cb(huart);
	}
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	UART_Device* dev = Get_UARTDev_FromUart(huart);
	if(dev!=NULL&&dev->err_cb!=NULL)
	{
		dev->err_cb(huart);
	}
}

static void log_start_transmit()
{
    if (is_sending || buf_head == buf_tail) {
        return;
    }
    is_sending = 1;
    if (buf_head > buf_tail) {
        current_send_len = buf_head - buf_tail;
    } else {
        current_send_len = LOG_BUF_SIZE - buf_tail;
    }
    if (HAL_UART_Transmit_DMA(&huart6, &log_buffer[buf_tail], current_send_len) != HAL_OK)
    {
        is_sending = 0; // 发送失败，释放标志，让下一个 fputc 还能尝试救活它
    }
}
int fputc(int ch, FILE *f)
{
	if (log_buffer == NULL) {
        log_buffer = (uint8_t*)pvPortMalloc(LOG_BUF_SIZE);
        if (log_buffer == NULL) return ch;
    }
    uint16_t next_head = (buf_head + 1) % LOG_BUF_SIZE;
	uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (next_head != buf_tail) 
	{
        log_buffer[buf_head] = (uint8_t)ch;
        buf_head = next_head;
    }
    if (!is_sending) {
        log_start_transmit();
    }
	__set_PRIMASK(primask);
    return ch;
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART6)
    {
		uint32_t primask = __get_PRIMASK();
        __disable_irq();
        buf_tail = (buf_tail + current_send_len) % LOG_BUF_SIZE;
        is_sending = 0;
        log_start_transmit();
		__set_PRIMASK(primask);
		return;
    }
	UART_Device* dev = Get_UARTDev_FromUart(huart);
	if(dev!=NULL&&dev->tx_cplt_cb!=NULL)
	{
		dev->tx_cplt_cb(huart);
	}
}