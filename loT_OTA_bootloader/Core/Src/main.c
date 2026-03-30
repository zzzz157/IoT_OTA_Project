/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "myuart.h"
#include "W25Q64.h"
#include "MCU_Flash.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define IH_MAGIC 			0x27051956
#define IH_NMLEN 			32

#define OTA_FLAG_ADDR       0x010000
#define OTA_HEAD_START_ADDR 0x000000
#define OTA_APP_START_ADDR 	0x000200

#define OTA_UPGRADE_DOING    0xA5A5
#define OTA_UPGRADE_CPLT     0x5A5A
#define OTA_UPGRADE_NON      0xAA55

#define HAED_LENGTH   		0x200

#define APP_ADDRESS 		0x08040200
#define APP_HEAD_ADDRESS 	0x08040000

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

extern void start_app(uint32_t new_vector);
#pragma pack(push, 1)
typedef struct image_header{
    uint32_t    ih_magic;       /* Image Header Magic Number    */
    uint32_t    ih_hcrc;        /* Image Header CRC Checksum    */
    uint32_t    ih_time;        /* Image Creation Timestamp     */
    uint32_t    ih_size;        /* Image Data Size              */
    uint32_t    ih_load;        /* Data  Load  Address          */
    uint32_t    ih_ep;          /* Entry Point Address          */
    uint32_t    ih_dcrc;        /* Image Data CRC Checksum      */
    uint8_t     ih_os;          /* Operating System             */
    uint8_t     ih_arch;        /* CPU architecture             */
    uint8_t     ih_type;        /* Image Type                   */
    uint8_t     ih_comp;        /* Compression Type             */
    uint8_t     ih_name[IH_NMLEN];  /* Image Name             */
}header_t;
#pragma pack(pop)
static W25Q64_t* Exteral_Flash=NULL;
void Exteral_Flash_Init(W25Q64_t* ex_handle)
{
	Exteral_Flash=ex_handle;
	Exteral_Flash->Init(Exteral_Flash);
}
#define ONCE_RELOCATE_MAX_LEN 512
void relocate_out_to_in(uint32_t out_addr,uint32_t inter_addr,uint32_t len)
{
	MCU_Flash_Erase(inter_addr,len);
	uint8_t data[ONCE_RELOCATE_MAX_LEN];
	uint32_t relocated_len=0;
	while(relocated_len<len)
	{
		uint32_t remain_len=len-relocated_len;
		uint16_t length = (remain_len>ONCE_RELOCATE_MAX_LEN)?ONCE_RELOCATE_MAX_LEN:remain_len;
		if(Exteral_Flash->ReadDatas(Exteral_Flash,out_addr+relocated_len,data,length)!=1)
		{
			put_str("read spi flash error\r\n",17);
		}		
		if (MCU_Flash_Write(inter_addr+relocated_len, data, length) != 1)
		{
		    put_str("Flash Write Error!\r\n", 20);
		    while(1);
		}
		relocated_len+=length;
	}
}
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
static header_t head;
void relocate_and_start_app(uint32_t ota_head_addr,uint32_t ota_head_len)
{
	/* read head */
	put_str("read head\r\n",11);
	Exteral_Flash->ReadDatas(Exteral_Flash,ota_head_addr,(uint8_t*)&head,sizeof(header_t));
	/* deal head */
	put_str("deal head\r\n",11);
	uint32_t magic=head.ih_magic;
	uint32_t load=head.ih_load;
	uint32_t ep=head.ih_ep;
	uint32_t content_length=head.ih_size;
	uint32_t content_crc=head.ih_dcrc;
	uint32_t head_crc=head.ih_hcrc;
	if(IH_MAGIC!=magic)
	{
		uint32_t flag = OTA_UPGRADE_CPLT;
        Exteral_Flash->SectorErase(Exteral_Flash, OTA_FLAG_ADDR & 0xFFFFF000);
        Exteral_Flash->WritePage(Exteral_Flash, OTA_FLAG_ADDR, (uint8_t*)&flag, sizeof(flag));
		while(1);
		return;
	}		
			/* CRC */
	/* app head CRC */
	put_str("check head crc\r\n", 17);
	head.ih_hcrc=0;
	uint32_t new_crc=crc32_calculate(0, (uint8_t*)&head,sizeof(header_t));
	if(new_crc!=head_crc) 
	{
		put_str("App head crc error!\r\n", 21);
		uint32_t flag = OTA_UPGRADE_NON;
        Exteral_Flash->SectorErase(Exteral_Flash, OTA_FLAG_ADDR & 0xFFFFF000);
        Exteral_Flash->WritePage(Exteral_Flash, OTA_FLAG_ADDR, (uint8_t*)&flag, sizeof(flag));
		//while(1);
		return;
	}
	put_str("App head crc ok!\r\n", 18);
	/* app data CRC */
	put_str("check data crc\r\n", 16);
    uint32_t calc_dcrc = 0;
    uint8_t buf[256];
    uint32_t check_len = 0;
	while(check_len < content_length)
	{
        uint32_t remain = content_length - check_len;
        uint16_t read_size = (remain > 256) ? 256 : remain;
        Exteral_Flash->ReadDatas(Exteral_Flash, ota_head_addr+ HAED_LENGTH  + check_len, buf, read_size);
        calc_dcrc = crc32_calculate(calc_dcrc, buf, read_size);
        check_len += read_size;
    }
	if (calc_dcrc != content_crc)
	{
        put_str("App Data CRC Error!\r\n", 21);
		put_str("Calc:", 5); put_hex(calc_dcrc, 4); put_str("\r\n", 2);
        put_str("Expect:", 7); put_hex(content_crc, 4); put_str("\r\n", 2);
        uint32_t flag = OTA_UPGRADE_NON;
        Exteral_Flash->SectorErase(Exteral_Flash, OTA_FLAG_ADDR & 0xFFFFF000);
        Exteral_Flash->WritePage(Exteral_Flash, OTA_FLAG_ADDR, (uint8_t*)&flag, sizeof(flag));
        return;
    }
	put_str("App data crc ok!\r\n",18);
	/* relocate */
	put_str("relocate\r\n",10);
	relocate_out_to_in(ota_head_addr,load,content_length+ota_head_len);
	/* jump to app */
	put_str("jump\r\n",6);
	uint32_t flag=OTA_UPGRADE_NON;
	Exteral_Flash->SectorErase(Exteral_Flash,OTA_FLAG_ADDR & 0xFFFFF000);
	Exteral_Flash->WritePage(Exteral_Flash,OTA_FLAG_ADDR,(uint8_t*)&flag,sizeof(flag));
	__disable_irq();
	HAL_RCC_DeInit();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
	HAL_DeInit();
	start_app(ep);
}
void prepare_start_app(uint32_t app_head_addr)
{
	/* read head */
	header_t* head=(header_t*)app_head_addr;
	/* deal head */
	uint32_t magic=head->ih_magic;
	if(magic!=IH_MAGIC)
	{
		put_str("magic error\r\n",13);
		return;
	}		
	uint32_t ep=head->ih_ep;
	/* jump to app */
	__disable_irq();
	HAL_RCC_DeInit();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
	HAL_DeInit();
	start_app(ep);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART6_UART_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
	put_str("bootloader\r\n",12);
	
	Exteral_Flash_Init(&W25QHandle_t);
	uint32_t flag=0xFFFF;
	Exteral_Flash->ReadDatas(Exteral_Flash,OTA_FLAG_ADDR,(uint8_t*)&flag,4);
	/* prepare to upgrade */
	if(flag==OTA_UPGRADE_DOING)
	{
		relocate_and_start_app(OTA_HEAD_START_ADDR,HAED_LENGTH);
	}
	prepare_start_app(APP_HEAD_ADDRESS);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
