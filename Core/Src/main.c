/**************************************************************************
 * @file       Core/src/main.c
 * @author     Z.Filinskiy
 * @brief      Program start and common board state API implementation
 *************************************************************************/

 #include "main.h"

#include <stdio.h>

#ifndef __ETHERNETIF_H__
	#include "ether.h"
#endif

#ifndef TFTP_EXAMPLE_H
	#include <tftp_serv.h>
#endif

#include <lwi.h>

#include <lan8742.h>

#ifndef EMBEDDED_CLI
	#include <embedded_cli.h>
#endif


/* Scheduler includes. */
#include "include/FreeRTOS.h"

#include "include/task.h"

#include "include/queue.h"

struct embedded_cli cli;

ETH_HandleTypeDef  heth;

IWDG_HandleTypeDef hiwdg1;

TIM_HandleTypeDef htim2;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart;

I2C_HandleTypeDef i2c1;

ETH_TxPacketConfigTypeDef TX;

ETH_BufferTypeDef dwTX;

#define LEN_BUF_ETH 1524

uint8_t mac[6] = {0x00, 0x12, 0x6D, 0x00, 0x01, 0x02};

uint8_t RXBUF[LEN_BUF_ETH] __attribute__((aligned(32)));

uint8_t TXBUF[LEN_BUF_ETH] __attribute__((aligned(32)));

ETH_DMADescTypeDef TX_DESC[ETH_TX_DESC_CNT] __attribute__((section(".TxDecripSection")))  __attribute__((aligned(32)));

ETH_DMADescTypeDef RX_DESC[ETH_TX_DESC_CNT] __attribute__((section(".RxDecripSection"))) __attribute__((aligned(32)));

uint8_t data[] = "ethINIT";

uint8_t bdata[] = "ethNOinit";

void SystemClock_Config(void);

static void MX_ETH_Init(void);

static void MX_GPIO_Init(void);

static void MX_TIM2_Init(void);

static void MX_RTC_Init(void);

static void MX_UART_Init(void);

static void MX_I2C_INIT(void);

extern void Ethernet_Link_Periodic_Handle(struct netif *netif);

void cli_output(void *data, char ch, bool is_last);

void app_on_rx_byte(char ch);

extern struct netif gnetif;

uint8_t bufrx[5];

uint8_t bufsv[] = {
    /* Destination MAC */
    0x01, 0x0C, 0xCD, 0x04, 0x00, 0x01,

    /* Source MAC */
    0x00, 0x12, 0x6D, 0x00, 0x01, 0x02,

    /* EtherType = IEC 61850 Sampled Values */
    0x88, 0xBA,

    /* ---- SV Header (8 байт) ---- */
    /* APPID */
    0x40, 0x00,
    /* Length = 8 (header) + 38 (savPdu) = 46 = 0x002E */
    0x00, 0x2E,
    /* Reserved1 */
    0x00, 0x00,
    /* Reserved2 */
    0x00, 0x00,

    /* ---- savPdu (APDU), length = 0x24 (36) ---- */
    0x60, 0x24,

    /* noASDU = 1 */
    0x80, 0x01,
    0x01,

    /* seqASDU, length = 0x1F (31) */
    0xA2, 0x1F,

    /* ASDU1, length = 0x1D (29) */
    0x30, 0x1D,

    /* svID = "TEST" */
    0x80, 0x04,
    0x54, 0x45, 0x53, 0x54,

    /* smpCnt = 1 */
    0x82, 0x02,
    0x00, 0x01,

    /* confRev = 1 */
    0x83, 0x04,
    0x00, 0x00, 0x00, 0x01,

    /* smpSynch = 0 */
    0x85, 0x01,
    0x00,

    /* seqData (пример, 8 значений) */
    0x87, 0x08,
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88
};
static void lwipProc( void * pvParameters );

static void uartProc(void* pvParameters);


static void uartSend(void* pvParameters);

uint8_t mesg ={0};

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
	HAL_Init();

	SystemClock_Config();
	
	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	//  MX_IWDG1_Init();
	MX_TIM2_Init();

	MX_RTC_Init();

	MX_UART_Init();

	embedded_cli_init(&cli, "zah> ", cli_output, NULL);

    embedded_cli_prompt(&cli);
	
	MX_I2C_INIT();

	MX_ETH_Init();

	MX_LWIP_Init();
	
	// библиотека дисплея ssd1306
	// ssd1306_Init();
	
	HAL_NVIC_SetPriority(TIM2_IRQn, 5, 0);

	HAL_NVIC_EnableIRQ(TIM2_IRQn);

	HAL_TIM_Base_Start_IT(&htim2);

	HAL_ETH_Start(&heth);

	TX.TxBuffer = &dwTX;

	TX.TxBuffer->buffer = TXBUF;

	TX.TxBuffer->len = sizeof(bufsv);

	memcpy(TXBUF,bufsv,sizeof(bufsv));

	tftp_example_init_server();

	 /* Start the tasks defined within this file/specific to this demo. */
    BaseType_t ret = xTaskCreate( lwipProc, "lwip", 1024, NULL, 1, NULL );

	BaseType_t ter = xTaskCreate( uartProc, "uart", 1024, NULL, 1, NULL );

	BaseType_t arr = xTaskCreate( uartSend, "uartSend", 1024, NULL, 1, NULL );

	if ((ret != 1) | (ter != 1))
	{
		Error_Handler();
	}
	
    // xTaskCreate( prvLCDTask, "LCD", configLCD_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL );
    // xTaskCreate( prvFlashTask, "Flash", configMINIMAL_STACK_SIZE, NULL, mainFLASH_TASK_PRIORITY, NULL );

    /* Start the scheduler. */
    vTaskStartScheduler();


	// while (1)
	// {

	// 	HAL_UART_Receive(&huart,&mesg,1,HAL_MAX_DELAY);
		
	// 	app_on_rx_byte((char)mesg);

	// 	MX_LWIP_Process();

	// 	sys_check_timeouts();
	// }
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


void cli_output(void *data, char ch, bool is_last)
{
	UNUSED(data);
	UNUSED (is_last);
	HAL_UART_Transmit(&huart, (uint8_t *)&ch, 1, HAL_MAX_DELAY);

}

void app_on_rx_byte(char ch)
{
    if (embedded_cli_insert_char(&cli, ch)) {
        char **argv;
        int argc = embedded_cli_argc(&cli, &argv);

        if (argc > 0) {
            // handle_command(argc, argv);
        }

        embedded_cli_prompt(&cli);
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

void MX_ETH_Init(void)
{
	heth.Instance = ETH;
	heth.Init.MACAddr = mac;
	heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
	heth.Init.TxDesc = TX_DESC;
	heth.Init.RxDesc = RX_DESC;
	heth.Init.RxBuffLen = LEN_BUF_ETH;

	if (HAL_ETH_Init(&heth))
	{
		HAL_UART_Transmit_IT(&huart,bdata,sizeof(bdata));
		Error_Handler();
	}
	HAL_UART_Transmit_IT(&huart,data,sizeof(data));

}


void lwipProc( void * pvParameters )
{
	while (1)
	{
	
		MX_LWIP_Process();

		sys_check_timeouts();

	}

}


static void uartProc(void* pvParameters)
{

    uint8_t ch;
    for (;;)
    {
        if (HAL_UART_Receive_IT(&huart, &ch, 1) == HAL_OK)
        {
            app_on_rx_byte((char)ch);
        }
        // при таймауте цикл идёт дальше, задача не залипает навсегда
    }
}


static void uartSend(void* pvParameters)
{
	while (1)
	{
		HAL_UART_Transmit_IT(&huart,bdata,sizeof(bdata));
	}
}

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
