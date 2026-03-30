#include "UART_OOP.h"
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "Modbus_Rtu.h"
#include "FreeRTOS.h"
#include "Task.h"
#include "semphr.h"
#include "Modbus_Data.h"

typedef struct
{
	SemaphoreHandle_t rxCpltSema;
	SemaphoreHandle_t txCpltSema;
	UART_Device *uart_dev;
	TIM_HandleTypeDef* tim;
	uint8_t rx_buf[MODBUS_MAX_ADU];
	uint16_t rx_len;
	uint32_t timer_count;
}modbus_config;
static uint32_t Calculate_TimeCount(Modbus_RTU *self)
{
	modbus_config* cfg=self->pData;
	UART_HandleTypeDef* uart_handle=Get_UART_Handle(cfg->uart_dev);
	uint32_t Baud=uart_handle->Init.BaudRate;
	if(Baud>=19200)
	{
		return 65535-1750;
	}
	else
	{
		return 65535-38500000/Baud;
	}
}
//Modbus 异常码回复封装
static int Send_Exception(Modbus_RTU *self,uint8_t exp_code) 
{
	modbus_config* cfg=self->pData;
    uint8_t tx_buf[5];
    tx_buf[0] = self->slave_id;
    tx_buf[1] = cfg->rx_buf[1] | 0x80; // 异常回复，功能码最高位置 1
    tx_buf[2] = exp_code;         // 异常码 (01:非法功能, 02:非法地址, 03:非法数据)
    
    uint16_t crc = crc16(tx_buf, 3);
    tx_buf[3] = crc & 0xFF;
    tx_buf[4] = (crc >> 8) & 0xFF;
    
	xSemaphoreTake(cfg->txCpltSema, 0);
    cfg->uart_dev->Send(cfg->uart_dev, tx_buf, 5, 100);
	if(pdTRUE != xSemaphoreTake(cfg->txCpltSema, pdMS_TO_TICKS(1000)))
    {
        return -1;
    }
    return 0;
}
//通用写回传函数
static int Modbus_Send_Echo(Modbus_RTU *self, uint32_t timeout)
{
	modbus_config* cfg=self->pData;
    uint8_t tx_buf[8];
    
    for(uint8_t i=0; i<6; i++) 
    {
        tx_buf[i] = cfg->rx_buf[i];
    }
    uint16_t crc_value = crc16(tx_buf, 6);
    tx_buf[6] = crc_value & 0xFF;
    tx_buf[7] = (crc_value >> 8) & 0xFF;
    
    UART_Device* pdev = cfg->uart_dev;
	xSemaphoreTake(cfg->txCpltSema, 0);
    pdev->Send(pdev, tx_buf, 8, 0);
    
    if(pdTRUE != xSemaphoreTake(cfg->txCpltSema, pdMS_TO_TICKS(timeout)))
    {
        return -1;
    }
    return 0;
}
static int Modbus_Reply_Bits(Modbus_RTU *self,Modbus_RegType regtype,uint32_t timeout)
{
	if(self->data_hooks == NULL) return -1;
	modbus_config* cfg=self->pData;
	uint8_t tx_buf[MODBUS_MAX_ADU]={0x00};
	uint8_t func_code = cfg->rx_buf[1];
	uint16_t start_addr=(cfg->rx_buf[2]<<8)|cfg->rx_buf[3];
	uint16_t bit_count=(cfg->rx_buf[4]<<8)|cfg->rx_buf[5];
	uint8_t byte_count=(bit_count / 8) + ((bit_count % 8) ? 1 : 0);
	uint16_t RegTableSize = ((regtype==Both_Write_Read)?NUM_BITS:NUM_INPUT_BITS);
	if(bit_count < 1 || bit_count > 2000 || (start_addr + bit_count) > RegTableSize)
    {
        Send_Exception(self,ILLEGAL_ADDRESS);
        return -1;
    }
	tx_buf[0]=self->slave_id;
	tx_buf[1]=func_code;
	tx_buf[2]=byte_count;
	uint8_t tx_idx = 3;
	for(uint16_t i=0;i<byte_count;i++)
	{
		uint8_t pack_byte = 0;
		for(uint8_t bit=0;bit<8;bit++)
		{
			uint16_t current_addr = start_addr + (i * 8) + bit;
			if (current_addr < start_addr + bit_count)
			{
				if(regtype==Both_Write_Read)
				{
					if(self->data_hooks->ReadBit(current_addr))
					{
						pack_byte|=(1<<bit);
					}
				}
				else if(regtype==Only_Read)
				{
					if(self->data_hooks->ReadInputBit(current_addr))
					{
						pack_byte|=(1<<bit);
					}
				}
				
			}
		}
		tx_buf[tx_idx++] = pack_byte;
	}
	uint16_t crc = crc16(tx_buf, tx_idx);
	tx_buf[tx_idx++] = crc & 0xFF;
    tx_buf[tx_idx++] = (crc >> 8) & 0xFF;
	UART_Device* pdev=cfg->uart_dev;
	xSemaphoreTake(cfg->txCpltSema, 0);
	pdev->Send(pdev,tx_buf,tx_idx,0);
	if(pdTRUE!=xSemaphoreTake(cfg->txCpltSema,pdMS_TO_TICKS(timeout)))
	{
		return -1;
	}
	return 0;
}
static int Modbus_Reply_Registers(Modbus_RTU *self,Modbus_RegType regtype,uint32_t timeout) 
{
	if(self->data_hooks == NULL) return -1;
	modbus_config* cfg=self->pData;
	uint8_t tx_buf[MODBUS_MAX_ADU]={0x00};
	uint8_t func_code = cfg->rx_buf[1];
	uint16_t start_addr=(cfg->rx_buf[2]<<8)|cfg->rx_buf[3];
	uint16_t reg_count=(cfg->rx_buf[4]<<8)|cfg->rx_buf[5];
	uint16_t RegTableSize = ((regtype==Both_Write_Read)?NUM_REGISTERS:NUM_INPUT_REGISTERS);
	if(reg_count < 1 || reg_count > 125 || (start_addr + reg_count) > RegTableSize) 
    {
        Send_Exception(self,ILLEGAL_ADDRESS);
        return -1;
    }
	tx_buf[0]=self->slave_id;
	tx_buf[1]=func_code;
	tx_buf[2]=reg_count*2;
	uint8_t tx_idx = 3;
	for(uint16_t i=0;i<reg_count;i++)
	{
		uint16_t pack_byte = ((regtype==Both_Write_Read)?self->data_hooks->ReadRegister(start_addr+i)
			:self->data_hooks->ReadInputRegister(start_addr+i));
		tx_buf[tx_idx++] = pack_byte>>8;
		tx_buf[tx_idx++] = pack_byte&0xFF;
	}
	uint16_t crc = crc16(tx_buf, tx_idx);
	tx_buf[tx_idx++] = crc & 0xFF;
    tx_buf[tx_idx++] = (crc >> 8) & 0xFF;
	UART_Device* pdev=cfg->uart_dev;
	xSemaphoreTake(cfg->txCpltSema, 0);
	pdev->Send(pdev,tx_buf,tx_idx,0);
	if(pdTRUE!=xSemaphoreTake(cfg->txCpltSema,pdMS_TO_TICKS(timeout)))
	{
		return -1;
	}
	return 0;
}
static int Modbus_Write_Bit(Modbus_RTU *self,uint32_t timeout)
{
	if(self->data_hooks == NULL) return -1;
	modbus_config* cfg=self->pData;
	uint16_t start_addr=(cfg->rx_buf[2]<<8)|cfg->rx_buf[3];
	uint16_t state=(cfg->rx_buf[4]<<8)|cfg->rx_buf[5];
	if(start_addr+1>NUM_BITS)
	{
		Send_Exception(self,ILLEGAL_ADDRESS);
		return -1;
	}
	if(state!=0xFF00&&state!=0x0000)
	{
		Send_Exception(self,ILLEGAL_DATA);
		return -1;
	}
	else
	{
		uint8_t byte=((state==0xFF00)?1:0);
		self->data_hooks->WriteBit(start_addr,byte);
		return Modbus_Send_Echo(self,timeout);
	}
}
static int Modbus_Write_Bits(Modbus_RTU *self,uint32_t timeout)
{
	if(self->data_hooks == NULL) return -1;
	modbus_config* cfg=self->pData;
	uint16_t start_addr=(cfg->rx_buf[2]<<8)|cfg->rx_buf[3];
	uint16_t bit_count=(cfg->rx_buf[4]<<8)|cfg->rx_buf[5];
	uint8_t byte_count=cfg->rx_buf[6];
	if(bit_count < 1 || bit_count > 2000 || (start_addr+bit_count>NUM_BITS))
	{
		Send_Exception(self,ILLEGAL_ADDRESS);
		return -1;
	}
	for(uint16_t bit=0;bit<bit_count;bit++)
	{
		uint8_t temp_byte=cfg->rx_buf[7+bit/8];
		if(temp_byte&(0x01<<(bit%8)))
		{
			self->data_hooks->WriteBit(start_addr+bit,1);
		}
		else
		{
			self->data_hooks->WriteBit(start_addr+bit,0);
		}
	}
	return Modbus_Send_Echo(self,timeout);
}
static int Modbus_Write_Register(Modbus_RTU *self,uint32_t timeout)
{
	if(self->data_hooks == NULL) return -1;
	if(self->data_hooks == NULL) return -1;
	modbus_config* cfg=self->pData;
	uint16_t start_addr=(cfg->rx_buf[2]<<8)|cfg->rx_buf[3];
	uint16_t value=(cfg->rx_buf[4]<<8)|cfg->rx_buf[5];
	if(start_addr+1>NUM_REGISTERS)
	{
		Send_Exception(self,ILLEGAL_ADDRESS);
		return -1;
	}
	self->data_hooks->WriteRegister(start_addr,value);
	return Modbus_Send_Echo(self,timeout);
}
static int Modbus_Write_Registers(Modbus_RTU *self,uint32_t timeout)
{
	if(self->data_hooks == NULL) return -1;
	modbus_config* cfg=self->pData;
	uint16_t start_addr=(cfg->rx_buf[2]<<8)|cfg->rx_buf[3];
	uint16_t register_count=(cfg->rx_buf[4]<<8)|cfg->rx_buf[5];
	if(register_count < 1 || register_count > 125 || (start_addr + register_count) > NUM_REGISTERS) 
	{
		Send_Exception(self,ILLEGAL_ADDRESS);
		return -1;
	}
	uint8_t idx=7;
	for(uint16_t i=0;i<register_count;i++)
	{
		uint8_t Msb_Byte=cfg->rx_buf[idx++];
		uint8_t Lsb_Byte=cfg->rx_buf[idx++];
		self->data_hooks->WriteRegister(start_addr+i,(Msb_Byte<<8)|Lsb_Byte);
	}
	return Modbus_Send_Echo(self,timeout);
}
//modbus解析功能代码
static void Modbus_Dispatch(Modbus_RTU *self)
{
	modbus_config* cfg=self->pData;
	uint8_t func_code = cfg->rx_buf[1];
	switch(func_code)
	{
		case 0x01://读线圈
		{
			Modbus_Reply_Bits(self,Both_Write_Read,1000);
			break;
		}
		case 0x02://读离散输入状态
		{
			Modbus_Reply_Bits(self,Only_Read,1000);
			break;
		}
		case 0x03://读保持寄存器
		{
			Modbus_Reply_Registers(self,Both_Write_Read,1000);
			break;
		}
		case 0x04://读输入寄存器
		{
			Modbus_Reply_Registers(self,Only_Read,1000);
			break;
		}
		case 0x05://写单个线圈
		{
			Modbus_Write_Bit(self,1000);
			break;
		}
		case 0x06://写单个保持寄存器
		{
			Modbus_Write_Register(self,1000);
			break;
		}
		case 0x0F://写多个线圈
		{
			Modbus_Write_Bits(self,1000);
			break;
		}
		case 0x10://写多个保持寄存器
		{
			Modbus_Write_Registers(self,1000);
			break;
		}
		default:
		{
			Send_Exception(self,ILLEGAL_FUNCTION);
			break;
		}
	}
}
static void Modbus_UART_ErrorCallback(UART_HandleTypeDef *huart);
static void Modbus_UART_TxCpltCallback(UART_HandleTypeDef *huart);
static void Modbus_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void Modbus_Init(Modbus_RTU *self, uint8_t slave_id, char *uart_name)
{
	modbus_config* cfg=self->pData;
	if(cfg->rxCpltSema == NULL)
	{
		cfg->rxCpltSema = xSemaphoreCreateBinary();
		if(cfg->rxCpltSema == NULL)  while(1); 
	}
	if(cfg->txCpltSema == NULL)
	{
		cfg->txCpltSema = xSemaphoreCreateBinary();
		if(cfg->txCpltSema == NULL)  while(1); 
	}
    self->slave_id = slave_id;
    cfg->uart_dev = GetUARTDevice(uart_name);
	cfg->uart_dev->err_cb=Modbus_UART_ErrorCallback;
	cfg->uart_dev->rx_cplt_cb=Modbus_UART_RxCpltCallback;
	cfg->uart_dev->tx_cplt_cb=Modbus_UART_TxCpltCallback;
	cfg->rx_len=0;
	cfg->timer_count=Calculate_TimeCount(self);
    cfg->uart_dev->Init(cfg->uart_dev);
}
void Modbus_Receive(Modbus_RTU *self)
{
	modbus_config* cfg=self->pData;
	xSemaphoreTake(cfg->rxCpltSema, 0);
	cfg->rx_len = 0;
	while (cfg->uart_dev->RecvByte(cfg->uart_dev, &cfg->rx_buf[cfg->rx_len], 1, 0) != 0)
	{
		UART_HandleTypeDef* uart_handle=Get_UART_Handle(cfg->uart_dev);
		HAL_UART_AbortReceive(uart_handle);
		vTaskDelay(pdMS_TO_TICKS(2));
	}
	xSemaphoreTake(cfg->rxCpltSema,portMAX_DELAY);
	if (cfg->rx_len >= 4 && (cfg->rx_buf[0] == self->slave_id || cfg->rx_buf[0] == 0x00))
	{
		uint16_t calc_crc = crc16(cfg->rx_buf, cfg->rx_len - 2);
		uint16_t recv_crc = (cfg->rx_buf[cfg->rx_len - 1] << 8) | cfg->rx_buf[cfg->rx_len - 2];
		
		if (calc_crc == recv_crc) 
		{
			Modbus_Dispatch(self);
		}
	}
	cfg->rx_len=0;
}
//
modbus_config modbus_rtu_cfg={
	.rxCpltSema=NULL,
	.txCpltSema=NULL,
	.tim=&htim6,
};

Modbus_RTU modbus_rtu_dev1=
{
	.pData=&modbus_rtu_cfg,
	.Init=Modbus_Init,
	.Receive=Modbus_Receive,
};
static Modbus_RTU* modbus_device[]={&modbus_rtu_dev1,NULL};

static Modbus_RTU* get_modbus_from_tim(TIM_HandleTypeDef *htim)
{
	for(uint8_t i=0;modbus_device[i]!=NULL;i++)
	{
		modbus_config* cfg=modbus_device[i]->pData;
		if(cfg->tim==htim)
		return modbus_device[i];
	}
	return NULL;
}
static Modbus_RTU* get_modbus_from_uart(UART_HandleTypeDef* uart)
{
	for(uint8_t i=0;modbus_device[i]!=NULL;i++)
	{
		modbus_config* cfg=modbus_device[i]->pData;
		UART_HandleTypeDef* uart_handle=Get_UART_Handle(cfg->uart_dev);
		if(uart_handle==uart)
		{
			return modbus_device[i];
		}
	}
	return NULL;
}
/* 定时器6溢出中断：T.35断帧 */
void Modbus_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM6)
	{
		Modbus_RTU* modbus_rtu=get_modbus_from_tim(htim);
		if (modbus_rtu == NULL) return;
		modbus_config* cfg=modbus_rtu->pData;
		HAL_TIM_Base_Stop_IT(cfg->tim);
		UART_HandleTypeDef* uart_handle=Get_UART_Handle(cfg->uart_dev);
		HAL_UART_AbortReceive_IT(uart_handle);
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		if(cfg->rxCpltSema!=NULL)
		{
			xSemaphoreGiveFromISR(cfg->rxCpltSema, &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
}
/* 接收完成回调 */
static void Modbus_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	//LOG_DEBUG("modbus RxCplt");
	Modbus_RTU* modbus_rtu=get_modbus_from_uart(huart);
	if (modbus_rtu == NULL) return;
	modbus_config* cfg=modbus_rtu->pData;
	cfg->rx_len++;
	if(cfg->rx_len<MODBUS_MAX_ADU)
	{
		cfg->uart_dev->RecvByte(cfg->uart_dev,&cfg->rx_buf[cfg->rx_len],1,0);
		HAL_TIM_Base_Stop_IT(cfg->tim);
		__HAL_TIM_SET_COUNTER(cfg->tim,cfg->timer_count);
		__HAL_TIM_CLEAR_IT(cfg->tim, TIM_IT_UPDATE);
		HAL_TIM_Base_Start_IT(cfg->tim);
	}
	else
	{
		HAL_TIM_Base_Stop_IT(cfg->tim);
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		if(cfg->rxCpltSema!=NULL)
		{
			xSemaphoreGiveFromISR(cfg->rxCpltSema, &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
}
/* 发送完成回调 */
static void Modbus_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	Modbus_RTU* modbus_rtu=get_modbus_from_uart(huart);
	if (modbus_rtu == NULL) return;
	modbus_config* cfg=modbus_rtu->pData;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	if(cfg->txCpltSema!=NULL)
	{
		xSemaphoreGiveFromISR(cfg->txCpltSema, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}
/* 错误回调 */
static void Modbus_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	Modbus_RTU* modbus_rtu = get_modbus_from_uart(huart);
	if (modbus_rtu == NULL) return;
	modbus_config* cfg = modbus_rtu->pData;
	HAL_TIM_Base_Stop_IT(cfg->tim);
	HAL_UART_AbortReceive_IT(huart);
	cfg->rx_len = 0;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (cfg->rxCpltSema != NULL)
	{ 
		xSemaphoreGiveFromISR(cfg->rxCpltSema, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}