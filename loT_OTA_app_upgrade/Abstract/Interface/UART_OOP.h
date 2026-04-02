#ifndef __UART_OOP_H
#define __UART_OOP_H
#include "main.h"
#include <stdint.h>
#include <stdio.h>
#define UART_RX_MAX_LEN 4000

typedef struct _UART_Device UART_Device;

typedef struct _UART_Device
{
	char *name;
	int (*Init)(UART_Device* self);
	int (*Send)(UART_Device* self,uint8_t *datas,uint32_t len,int timeout);
	int (*RecvByte)(UART_Device* self, uint8_t *data,uint16_t len, int timeout);
	
	void (*tx_cplt_cb)(UART_HandleTypeDef *huart);
	void (*rx_cplt_cb)(UART_HandleTypeDef *huart);
	void (*err_cb)(UART_HandleTypeDef *huart);
	void (*rx_event_cb)(UART_HandleTypeDef *huart, uint16_t Size);
	
    void* priv_data;
}UART_Device;

UART_Device* GetUARTDevice(char *name);
UART_HandleTypeDef* Get_UART_Handle(UART_Device* pdev);
uint8_t* Get_dmabuf_FromUART(UART_HandleTypeDef *huart);
UART_Device* Get_UARTDev_FromUart(UART_HandleTypeDef *huart);

int fputc(int ch, FILE *f);

#endif