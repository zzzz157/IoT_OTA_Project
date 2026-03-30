#include "main.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "Task.h"
#include "semphr.h"
#include "queue.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "MAX30102.h"
#include "algorithm.h"
#include "OLED.h"
#include "Broker.h"
#include "DataCenter.h"

static const I2C_Device* MAX_I2C=NULL;
/**
  * @brief  MAX30102初始化
  * @param  无
  * @retval 无
  */
void MAX30102_Init(const I2C_Device* i2c_bus)
{
	if (i2c_bus == NULL) return;
	MAX_I2C=i2c_bus;
	MAX_I2C->Init(MAX_I2C);
	vTaskDelay(1);
	uint8_t id;
	MAX_I2C->ReadReg(MAX_I2C,MAX30102_ADDRESS_7BIT,MAX30102_REG_PART_ID,&id,10);
	OLED_ShowHexNum(2,1,id,2);
	
	MAX_I2C->WriteReg(MAX_I2C, MAX30102_ADDRESS_7BIT, 
        MAX30102_REG_MODE_CONFIG, MAX30102_MODE_RESET,1000);
    vTaskDelay(100);
    
    //开启 FIFO 满中断和数据就绪中断
    MAX_I2C->WriteReg(MAX_I2C, MAX30102_ADDRESS_7BIT, 
        MAX30102_REG_INT_ENABLE_1, MAX30102_INT_A_FULL_EN | MAX30102_INT_PPG_RDY_EN,1000);
    MAX_I2C->WriteReg(MAX_I2C, MAX30102_ADDRESS_7BIT, 
        MAX30102_REG_INT_ENABLE_2, 0x00,10);

    //清零指针
    MAX_I2C->WriteReg(MAX_I2C, MAX30102_ADDRESS_7BIT, MAX30102_REG_FIFO_WR_PTR, 0x00,1000);
    MAX_I2C->WriteReg(MAX_I2C, MAX30102_ADDRESS_7BIT, MAX30102_REG_OVF_COUNTER, 0x00,1000);
    MAX_I2C->WriteReg(MAX_I2C, MAX30102_ADDRESS_7BIT, MAX30102_REG_FIFO_RD_PTR, 0x00,1000);
  
    //FIFO 配置：1样本平均 | 启用滚动更新 | 满中断阈值设置为15 (0x0F)
    MAX_I2C->WriteReg(MAX_I2C, MAX30102_ADDRESS_7BIT, 
        MAX30102_REG_FIFO_CONFIG, MAX30102_SMP_AVE_4 | MAX30102_ROLLOVER_EN | 0x0F,10);

    //模式配置：SpO2 模式 (红光+红外)
    MAX_I2C->WriteReg(MAX_I2C, MAX30102_ADDRESS_7BIT, 
        MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2,10);

    //SpO2 精度配置：量程4096nA | 采样率400Hz | 脉宽411us(18位)
    MAX_I2C->WriteReg(MAX_I2C, MAX30102_ADDRESS_7BIT, MAX30102_REG_SPO2_CONFIG, 
		MAX30102_ADC_RGE_4096 | MAX30102_SR_400 | MAX30102_PW_18BIT_411US,10);
        
    //LED 电流配置：0x31 约等于 9.8mA
    MAX_I2C->WriteReg(MAX_I2C, MAX30102_ADDRESS_7BIT, MAX30102_REG_LED1_PA, 0x31,10);
    MAX_I2C->WriteReg(MAX_I2C, MAX30102_ADDRESS_7BIT, MAX30102_REG_LED2_PA, 0x31,10);
	
	uint8_t InvalidData;
	MAX_I2C->ReadReg(MAX_I2C,MAX30102_ADDRESS_7BIT,MAX30102_REG_INT_STAT_1,&InvalidData,10);
	MAX_I2C->ReadReg(MAX_I2C,MAX30102_ADDRESS_7BIT,MAX30102_REG_INT_STAT_2,&InvalidData,10);
	
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 12, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}
/**
  * @brief  MAX30102读取FIFO
  * @param  RedData：存放红光数据的数组
			IrData： 存放红外数据的数组
  * @retval 无
  */
HAL_StatusTypeDef MAX30102_ReadData(uint32_t *RedData, uint32_t *IrData)
{
	uint8_t ReceiveBuf[6];
	HAL_StatusTypeDef State=MAX_I2C->ReadRegs(MAX_I2C,MAX30102_ADDRESS_7BIT,
		MAX30102_REG_FIFO_DATA,ReceiveBuf,6,10);
	*RedData = ((uint32_t)ReceiveBuf[0] << 16 | (uint32_t)ReceiveBuf[1] << 8 |
	ReceiveBuf[2]) & 0x03FFFF;
    *IrData  = ((uint32_t)ReceiveBuf[3] << 16 | (uint32_t)ReceiveBuf[4] << 8 |
	ReceiveBuf[5]) & 0x03FFFF;
	return State;
}
/**
  * @brief  MAX30102采集任务
  * @param
  * @retval 无
  */
static SemaphoreHandle_t g_xMAX30102AcquireSemaphore_t;
static SemaphoreHandle_t g_xMAX30102CalculateSemaphore_t;
static PPG_FIFO_t Ring_Buffer;
void vMAX30102_AcquireTask(void* arg)
{
	LOG_DEBUG("MAX_Acq start");
    while(1)
	{
		xSemaphoreTake(g_xMAX30102AcquireSemaphore_t,portMAX_DELAY);
		uint32_t r, ir;
		if (MAX30102_ReadData(&r, &ir) == HAL_OK)
		{
			uint8_t InvalidData;
			MAX_I2C->ReadReg(MAX_I2C,MAX30102_ADDRESS_7BIT,MAX30102_REG_INT_STAT_1,
				&InvalidData,10);
			if(r&&ir)
			{
				FIFO_Push(&Ring_Buffer,ir,r);
				if(FIFO_GetCount(&Ring_Buffer)>=BUFFER_SIZE)
				{
					xSemaphoreGive(g_xMAX30102CalculateSemaphore_t);
				}
			}
		}
		else
		{
			//Sensor Error
			DataCenter_UpdateSensorError(1);
		}
	}
}
/**
  * @brief  MAX30102处理任务
  * @param  
  * @retval 无
  */
int32_t n_heart_rate,n_spo2;
HealthData_t MAX_Res={75,98,};
int8_t ch_spo2_valid,ch_hr_valid;// 血氧 心率有效标志
void vMAX30102_CalculateTask(void* arg)
{
	//LOG_DEBUG("MAX_Calcu start");
	static uint8_t invalid_count=0;
	static uint32_t work_ir[BUFFER_SIZE];
	static uint32_t work_red[BUFFER_SIZE];
	while(1)
	{
		xSemaphoreTake(g_xMAX30102CalculateSemaphore_t,portMAX_DELAY);
		FIFO_PeekWindow(&Ring_Buffer,work_ir,work_red,BUFFER_SIZE);
		maxim_heart_rate_and_oxygen_saturation(work_ir, BUFFER_SIZE, work_red, &n_spo2
			, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);
		FIFO_Slide(&Ring_Buffer,BUFFER_SIZE/5);
		bool is_data_valid=ch_hr_valid && ch_spo2_valid;
		bool is_value_rational=(n_heart_rate<140&&n_heart_rate>45&&n_spo2>50);
		bool is_close_to_filtered=(abs(n_heart_rate - (HR_Filter.result_q10>>ALPHA_SHIFT)) < 25);
		if(is_data_valid&&is_value_rational&&is_close_to_filtered)
		{
			invalid_count = 0;
			MAX_Res.HeartRate_Value= Apply_IIR_Filter_Fixed(&HR_Filter,n_heart_rate);
			MAX_Res.Spo2_Value = Apply_IIR_Filter_Fixed(&SpO2_Filter,n_spo2);
			MAX_Res.confidence=1;
			MAX_Res.timestamp_ms=xTaskGetTickCount();
			LOG_DEBUG("Data Update");
			Broker_Publish(TOPIC_HEALTH_DATA,&MAX_Res);
		}
		else
		{
			if (invalid_count < 4)
			{
				invalid_count++;
			}
			else
			{
				invalid_count=0;
				MAX_Res.confidence=0;
				
				MAX_Res.timestamp_ms=xTaskGetTickCount();
				LOG_DEBUG("Error Heath Public");
				Broker_Publish(TOPIC_HEALTH_DATA,&MAX_Res);
				Apply_IIR_Filter_Fixed(&HR_Filter,MAX_Res.HeartRate_Value);
				Apply_IIR_Filter_Fixed(&HR_Filter,MAX_Res.Spo2_Value);
			}
		}
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if(GPIO_Pin==GPIO_PIN_14)
	{
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		if(g_xMAX30102AcquireSemaphore_t!=NULL)
		{
			xSemaphoreGiveFromISR(g_xMAX30102AcquireSemaphore_t,&xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
}
static TaskHandle_t xMAX_AcquireTaskHandler,xMAX_CalculateTaskHandler;
void MAX30102_Task(void* arg)
{
	g_xMAX30102AcquireSemaphore_t = xSemaphoreCreateBinary();
	g_xMAX30102CalculateSemaphore_t = xSemaphoreCreateBinary();
	FIFO_Init(&Ring_Buffer);
	MAX30102_Init(&HardI2C1_DMA_Obj);
	LOG_DEBUG("MAX init Cplt");
	xTaskCreate(vMAX30102_AcquireTask,"MAX30102_AcquireTask",1024,NULL,5,&xMAX_AcquireTaskHandler);
	xTaskCreate(vMAX30102_CalculateTask,"MAX30102_CalculateTask",1536,NULL,1,&xMAX_CalculateTaskHandler);
	vTaskDelete(NULL);
	while(1);
}