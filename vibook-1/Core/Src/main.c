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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t found_addresses[10];
uint8_t device_count = 0;

volatile uint8_t touch_event_detected = 0;
uint16_t chip1_touched = 0;
uint16_t chip2_touched = 0;

#define MPR121_ADDR1 (0x5A << 1) // U1's address shifted for STM32 HAL
#define MPR121_ADDR2 (0x5C << 1)
volatile uint16_t current_touches = 0;

void MPR121_Init_Simple(void) {
    uint8_t config_data[2];

    // 1. Soft Reset
    config_data[0] = 0x80; config_data[1] = 0x63;
    HAL_I2C_Master_Transmit(&hi2c1, MPR121_ADDR1, config_data, 2, 100);
    HAL_I2C_Master_Transmit(&hi2c1, MPR121_ADDR2, config_data, 2, 100);
    HAL_Delay(10);

    // 2. Set touch and release thresholds for all electrodes (approximate values)
    for (uint8_t i = 0x41; i < 0x5A; i+=2) {
        config_data[0] = i;     // Touch Threshold Register
        config_data[1] = 12;    // Touch Value (Lower = more sensitive)
        HAL_I2C_Master_Transmit(&hi2c1, MPR121_ADDR1, config_data, 2, 100);
        HAL_I2C_Master_Transmit(&hi2c1, MPR121_ADDR2, config_data, 2, 100);


        config_data[0] = i + 1; // Release Threshold Register
        config_data[1] = 6;     // Release Value
        HAL_I2C_Master_Transmit(&hi2c1, MPR121_ADDR1, config_data, 2, 100);
        HAL_I2C_Master_Transmit(&hi2c1, MPR121_ADDR2, config_data, 2, 100);

    }

    // 3. Turn on all 12 electrodes and enter "Run Mode"
    config_data[0] = 0x5E; config_data[1] = 0x0C;
    HAL_I2C_Master_Transmit(&hi2c1, MPR121_ADDR1, config_data, 2, 100);
    HAL_I2C_Master_Transmit(&hi2c1, MPR121_ADDR2, config_data, 2, 100);

}

volatile uint16_t raw_electrode_data[24] = {0};

// Keep your MPR121_Init_Simple() function exactly as it is!

// Replace MPR121_Read_Touches with this:
void MPR121_Read_Raw_Data(void) {
    uint8_t reg = 0x04; // Start at the first raw data register
    uint8_t data[48];   // 12 electrodes * 2 bytes each = 24 bytes

    // Ask for 24 bytes starting at register 0x04
    HAL_I2C_Master_Transmit(&hi2c1, MPR121_ADDR1, &reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c1, MPR121_ADDR1, data, 24, 100);
    HAL_I2C_Master_Transmit(&hi2c1, MPR121_ADDR2, &reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c1, MPR121_ADDR2, &data[24], 24, 100);

    // The MPR121 sends the Least Significant Byte (LSB) first, then the MSB.
    // We combine them into our 16-bit array.
    for (int i = 0; i < 24; i++) {
        raw_electrode_data[i] = ((uint16_t)data[(i*2) + 1] << 8) | data[(i*2)];
    }
}

void MPR121_Read_Touch_Status(void) {
	uint8_t reg = 0x00;
	uint8_t status_data[2];

	HAL_I2C_Master_Transmit(&hi2c1, MPR121_ADDR1, &reg, 1, 100);
	HAL_I2C_Master_Receive(&hi2c1, MPR121_ADDR1, status_data, 2, 100);
	chip1_touched = ((uint16_t)status_data[1] << 8) | status_data[0];

	HAL_I2C_Master_Transmit(&hi2c1, MPR121_ADDR2, &reg, 1, 100);
	HAL_I2C_Master_Receive(&hi2c1, MPR121_ADDR2, status_data, 2, 100);
    chip2_touched = ((uint16_t)status_data[1] << 8) | status_data[0];

}

void DFPlayer_Send(uint8_t source, uint8_t command, uint8_t dat1, uint8_t dat2) {
    uint8_t msg[10] = {0x7E, 0xFF, 0x06, command, 0x00, dat1, dat2, 0x00, 0x00, 0xEF};

    // Checksum calculation (Required for most clones)
    uint16_t checksum = 0;
    for (int i=1; i<7; i++) checksum += msg[i];
    checksum = -checksum;

    msg[7] = (uint8_t)(checksum >> 8);
    msg[8] = (uint8_t)checksum;

    // Use &huart2 because that's what your main.c initialized
    HAL_UART_Transmit(&huart2, msg, 10, 100);
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
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  MPR121_Init_Simple();
    HAL_Delay(500); // Give DFPlayer time to wake up

    // Set initial volume (0x00 to 0x1E / 30)
    DFPlayer_Send(0, 0x06, 0, 30);

    uint16_t last_touches = 0; // To prevent re-triggering
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	if(touch_event_detected == 1) {
		touch_event_detected = 0;
		MPR121_Read_Touch_Status();

		if(chip1_touched != 0) {
			for(int i = 0; i < 12; ++i) {
				if(chip1_touched & (1 << i)) {
					DFPlayer_Send(0, 0x16, 0x00, 0x00);
					DFPlayer_Send(0, 0x03, 0x00, i+1);
				}
			}
			DFPlayer_Send(0, 0x16, 0x00, 0x00);
		}
		if(chip2_touched != 0) {
			for(int i = 0; i < 12; ++i) {
				if(chip2_touched & (1 << i)) {
					DFPlayer_Send(0, 0x16, 0x00, 0x00);
					DFPlayer_Send(0, 0x03, 0x00, i+12);
				}
			}
		}
	}

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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_5;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_I2C1;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00000608;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if(GPIO_Pin == GPIO_PIN_5) {
		touch_event_detected = 1;
	}
}

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
