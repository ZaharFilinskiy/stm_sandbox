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
#include <stdio.h>
#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif


IWDG_HandleTypeDef hiwdg1;

TIM_HandleTypeDef htim2;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart;

I2C_HandleTypeDef i2c1;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

static void MX_TIM2_Init(void);

static void MX_RTC_Init(void);

static void MX_UART_Init(void);

static void MX_I2C_INIT(void);

uint8_t buftx[] = "VASAP\n";

uint8_t bufrx[5];

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */
	/* USER CODE BEGIN Boot_Mode_Sequence_0 */
	// int32_t timeout;
	/* USER CODE END Boot_Mode_Sequence_0 */

	/* USER CODE BEGIN Boot_Mode_Sequence_1 */
	  /* Wait until CPU2 boots and enters in stop mode or timeout*/
	  //  timeout = 0xFFFF;
	  //  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
	  //  if ( timeout < 0 )
	  //  {
	  //  Error_Handler();
	  //  }
	  /* USER CODE END Boot_Mode_Sequence_1 */
	    /* MCU Configuration--------------------------------------------------------*/

	    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();



	// HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET); // Включить желтый светодиод
	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();
	/* USER CODE BEGIN Boot_Mode_Sequence_2 */
	/* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
	HSEM notification */
	/*HW semaphore Clock enable*/
	//__HAL_RCC_HSEM_CLK_ENABLE();
	///*Take HSEM */
	//HAL_HSEM_FastTake(HSEM_ID_0);
	///*Release HSEM in order to notify the CPU2(CM4)*/
	//HAL_HSEM_Release(HSEM_ID_0,0);
	///* wait until CPU2 wakes up from stop mode */
	//timeout = 0xFFFF;
	//while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));
	//if ( timeout < 0 )
	//{
	//Error_Handler();
	//}
	/* USER CODE END Boot_Mode_Sequence_2 */

	  /* USER CODE BEGIN SysInit */

	  /* USER CODE END SysInit */

	  /* Initialize all configured peripherals */
	MX_GPIO_Init();
	//  MX_IWDG1_Init();
	MX_TIM2_Init();

	MX_RTC_Init();

	MX_UART_Init();
	
	MX_I2C_INIT();
	
	//Код библиотеки настройки дисплея ssd1306
	ssd1306_Init();
	ssd1306_Fill(Black);
	ssd1306_TestAll();
	// ssd1306_SetCursor(0, 0);
	// ssd1306_WriteString("Privet!", Font_7x10, White);
	// ssd1306_UpdateScreen();
	
	HAL_NVIC_SetPriority(TIM2_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(TIM2_IRQn);
	HAL_TIM_Base_Start_IT(&htim2);
	buftx[0] = 'V';
	buftx[1] = 'A';
	buftx[2] = 'S';
	buftx[3] = 'A';
	buftx[4] = 'P';
	buftx[5] = '\n';
	/* USER CODE END 2 */
	HAL_StatusTypeDef rvl;
	RTC_TimeTypeDef mi;
	RTC_DateTypeDef dat;
	char min_str[3]; // "00" + '\0'
	char date_str[3]; // "00" + '\0'
	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		/* USER CODE END WHILE */
		/* USER CODE BEGIN 3 */
		//	  if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13))
		//	  {
		//		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
		//		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
		//		  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
		//	  }
		//	  else
		//	  {
		//		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
		//		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
		//		  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
		//	  }
		memset(bufrx, 0, sizeof(bufrx));                          // очистили буфер приёма
		memcpy(bufrx, buftx, sizeof(bufrx) - 1);                 // скопировали "VASA"
		bufrx[sizeof(bufrx) - 1] = '\0';                         // завершающий ноль

		if (huart.gState == HAL_UART_STATE_READY)
		{
			rvl = HAL_UART_Transmit_IT(&huart, buftx, sizeof(buftx) - 1);

			if (rvl != HAL_OK)
			{
				HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
			}
		}
		// HAL_RTC_GetTime(&hrtc,&mi,RTC_FORMAT_BIN);
		// HAL_RTC_GetDate(&hrtc,&dat,RTC_FORMAT_BIN);
		// ssd1306_SetCursor(0, 0);
		// snprintf(min_str, sizeof(min_str), "%02u", mi.Seconds);
		// ssd1306_WriteString(min_str, Font_7x10, White);
		// ssd1306_UpdateScreen();
		// ssd1306_SetCursor(2, 10);
		// snprintf(date_str, sizeof(date_str), "%02u", dat.Date);
		// ssd1306_WriteString(date_str, Font_7x10, White);
		// ssd1306_UpdateScreen();
		
		HAL_Delay(1000);
		// HAL_PWR_EnterSLEEPMode
		// HAL_UART_Receive();

	}
	/* USER CODE END 3 */
}

void MX_I2C_INIT()
{
	// HAL_StatusTypeDef rev;
	i2c1.Instance = I2C1;
	i2c1.Init.Timing = 0x70421F2F; // 100 kHz for I2C kernel clock = 64 MHz (HSI/APB1)
 	i2c1.Init.OwnAddress1 = 0;
  	i2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  	i2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  	i2c1.Init.OwnAddress2 = 0;
  	i2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  	i2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  	i2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

	if(HAL_I2C_Init(&i2c1) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_I2CEx_ConfigAnalogFilter(&i2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_I2CEx_ConfigDigitalFilter(&i2c1, 0) != HAL_OK)
	{
		Error_Handler();
	}

}


//Блабла
void MX_UART_Init()
{
	HAL_StatusTypeDef rvl;
	huart.Instance = USART3;
	huart.Init.BaudRate = 115200;
	huart.Init.WordLength = UART_WORDLENGTH_8B;
	huart.Init.StopBits = UART_STOPBITS_1;
	huart.Init.Parity = UART_PARITY_NONE;
	huart.Init.OverSampling = UART_OVERSAMPLING_16;
	huart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart.Init.Mode = UART_MODE_TX_RX;
	huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	// huart.pRxBuffPtr = &bufrx;
	// huart.pTxBuffPtr = &buftx;

	rvl = HAL_UART_Init(&huart);

	if (rvl != HAL_OK)
	{
		Error_Handler();
	}
	
}

void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
       HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    }
}


void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
    }
}

void TIM2_IRQHandler()
{
	HAL_TIM_IRQHandler(&htim2);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim == &htim2)
	{
	// HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
	// HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
	HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_1);
	}
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Supply configuration update enable
	*/
	HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

	/** Configure the main internal regulator output voltage
	*/
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

	while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

	/** Initializes the RCC Oscillators according to the specified parameters
	* in the RCC_OscInitTypeDef structure.
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	*/
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
	                            | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
	                            | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
	RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
  * @brief IWDG1 Initialization Function
  * @param None
  * @retval None
  */
// static void MX_IWDG1_Init(void)
// {

// 	/* USER CODE BEGIN IWDG1_Init 0 */

// 	/* USER CODE END IWDG1_Init 0 */

// 	/* USER CODE BEGIN IWDG1_Init 1 */

// 	/* USER CODE END IWDG1_Init 1 */
// 	hiwdg1.Instance = IWDG1;
// 	hiwdg1.Init.Prescaler = IWDG_PRESCALER_4;
// 	hiwdg1.Init.Window = 4095;
// 	hiwdg1.Init.Reload = 4095;
// 	if (HAL_IWDG_Init(&hiwdg1) != HAL_OK)
// 	{
// 		Error_Handler();
// 	}
// 	/* USER CODE BEGIN IWDG1_Init 2 */

// 	/* USER CODE END IWDG1_Init 2 */

// }

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

	/* USER CODE BEGIN TIM1_Init 0 */
	__HAL_RCC_TIM2_CLK_ENABLE();
	/* USER CODE END TIM1_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM1_Init 1 */

	/* USER CODE END TIM1_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 1000;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 64000;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.RepetitionCounter = 0;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM1_Init 2 */

	/* USER CODE END TIM1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

	  /* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_14, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);

	/*Configure GPIO pin : PC0 */
	GPIO_InitStruct.Pin = GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : PB0 PB14 */
	GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_14;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : PE1 */
	GPIO_InitStruct.Pin = GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */
	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

	/* USER CODE BEGIN RTC_Init 0 */

	/* USER CODE END RTC_Init 0 */

	RTC_TimeTypeDef sTime = { 0 };
	RTC_DateTypeDef sDate = { 0 };

	/* USER CODE BEGIN RTC_Init 1 */

	/* USER CODE END RTC_Init 1 */

	/** Initialize RTC Only
	*/
	hrtc.Instance = RTC;
	hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
	hrtc.Init.AsynchPrediv = 127;
	hrtc.Init.SynchPrediv = 255;
	hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
	hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
	hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
	hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
	if (HAL_RTC_Init(&hrtc) != HAL_OK)
	{
		Error_Handler();
	}

	/* USER CODE BEGIN Check_RTC_BKUP */

	/* USER CODE END Check_RTC_BKUP */

	/** Initialize RTC and set the Time and Date
	*/
	sTime.Hours = 0x0;
	sTime.Minutes = 0x0;
	sTime.Seconds = 0x0;
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_RESET;
	if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
	{
		Error_Handler();
	}
	sDate.WeekDay = RTC_WEEKDAY_MONDAY;
	sDate.Month = RTC_MONTH_JANUARY;
	sDate.Date = 0x1;
	sDate.Year = 0x0;

	if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN RTC_Init 2 */

	/* USER CODE END RTC_Init 2 */

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

#ifdef  USE_FULL_ASSERT
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
