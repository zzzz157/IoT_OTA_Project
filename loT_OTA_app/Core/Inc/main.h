/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */
#define LOG_BUF_SIZE  512
typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

#define CURRENT_LOG_LEVEL LOG_LEVEL_DEBUG

#define LOG_LEVEL_STR(level) \
    ((level) == LOG_LEVEL_DEBUG ? "DEBUG" : \
     (level) == LOG_LEVEL_INFO  ? "INFO"  : \
     (level) == LOG_LEVEL_WARN  ? "WARN"  : "ERROR")
// 日志输出宏
#define LOG(level, fmt, ...) \
    do { \
        if ((level) >= CURRENT_LOG_LEVEL) { \
            printf("[%s] [%s:%d] " fmt "\r\n", \
                   LOG_LEVEL_STR(level), __FILE__, __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)
// 便捷宏
#define LOG_DEBUG(fmt, ...) LOG(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
