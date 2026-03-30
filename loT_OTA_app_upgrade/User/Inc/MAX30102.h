#ifndef __MAX30102__H
#define __MAX30102__H

#include "I2C_OOP.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdbool.h>

#define MAX30102_ADDRESS_7BIT 0x57
// --- 状态寄存器 ---
#define MAX30102_REG_INT_STAT_1     0x00
#define MAX30102_REG_INT_STAT_2     0x01
#define MAX30102_REG_INT_ENABLE_1   0x02
#define MAX30102_REG_INT_ENABLE_2   0x03
// --- FIFO 寄存器 ---
#define MAX30102_REG_FIFO_WR_PTR    0x04
#define MAX30102_REG_OVF_COUNTER    0x05
#define MAX30102_REG_FIFO_RD_PTR    0x06
#define MAX30102_REG_FIFO_DATA      0x07
// --- 配置寄存器 ---
#define MAX30102_REG_FIFO_CONFIG    0x08
#define MAX30102_REG_MODE_CONFIG    0x09
#define MAX30102_REG_SPO2_CONFIG    0x0A
#define MAX30102_REG_LED1_PA        0x0C  // 红光 LED 电流
#define MAX30102_REG_LED2_PA        0x0D  // 红外 LED 电流
// --- ID 寄存器 ---
#define MAX30102_REG_REV_ID         0xFE
#define MAX30102_REG_PART_ID        0xFF  //ID:0x15



// --- 模式配置 (MODE_CONFIG) ---
#define MAX30102_MODE_RESET         (0x01 << 6) // 复位
#define MAX30102_MODE_HR            0x02        // 单红光模式
#define MAX30102_MODE_SPO2          0x03        // 红光+红外模式 (SpO2)

// --- 中断使能 (INT_ENABLE_1) ---
#define MAX30102_INT_A_FULL_EN      (0x01 << 7) // FIFO 几乎满中断使能
#define MAX30102_INT_PPG_RDY_EN     (0x01 << 6) // 新数据就绪中断使能

// --- FIFO 配置 (FIFO_CONFIG) ---
// 样本平均 (SMP_AVE)
#define MAX30102_SMP_AVE_1          (0x00 << 5)
#define MAX30102_SMP_AVE_2          (0x01 << 5)
#define MAX30102_SMP_AVE_4          (0x02 << 5)
#define MAX30102_SMP_AVE_8          (0x03 << 5)
// FIFO 滚动更新使能
#define MAX30102_ROLLOVER_EN        (0x01 << 4)

// --- SpO2 配置 (SPO2_CONFIG) ---
// ADC 范围 (SPO2_ADC_RGE)
#define MAX30102_ADC_RGE_2048       (0x00 << 5)
#define MAX30102_ADC_RGE_4096       (0x01 << 5)
#define MAX30102_ADC_RGE_8192       (0x02 << 5)
#define MAX30102_ADC_RGE_16384      (0x03 << 5)
// 采样率 (SPO2_SR)
#define MAX30102_SR_50              (0x00 << 2)
#define MAX30102_SR_100             (0x01 << 2)
#define MAX30102_SR_200             (0x02 << 2)
#define MAX30102_SR_400             (0x03 << 2)
// LED 脉冲宽度 (LED_PW) / 分辨率
#define MAX30102_PW_15BIT_69US      0x00
#define MAX30102_PW_16BIT_118US     0x01
#define MAX30102_PW_17BIT_215US     0x02
#define MAX30102_PW_18BIT_411US     0x03
typedef struct
{
	uint32_t timestamp_ms;
	uint16_t HeartRate_Value;
	uint16_t Spo2_Value;
	bool confidence;
}HealthData_t;

extern SemaphoreHandle_t g_xMAX30102AcquireSemaphore_t;

void MAX30102_Init(const I2C_Device* MAX_I2C);
HAL_StatusTypeDef MAX30102_ReadData(uint32_t *RedData, uint32_t *IrData);
void MAX30102_Task(void* arg);

#endif