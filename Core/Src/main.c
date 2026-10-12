/**************************************************************************
 * @file       Core/src/main.c
 * @author     Z.Filinskiy
 * @brief      Program start and common board state API implementation
 *************************************************************************/

#include "main.h"
#include "app_cli.h"
#include "settings.h"
#include "flash_access.h"

#include "ether.h"
#include "tftp_serv.h"
#include "lwi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

ETH_HandleTypeDef  heth;

IWDG_HandleTypeDef hiwdg1;

TIM_HandleTypeDef htim2;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart;

I2C_HandleTypeDef i2c1;

#define LEN_BUF_ETH ETH_RX_BUFFER_SIZE

uint8_t mac[6] = {0x00, 0x12, 0x6D, 0x00, 0x01, 0x02};

ETH_DMADescTypeDef TX_DESC[ETH_TX_DESC_CNT] __attribute__((section(".TxDecripSection")))  __attribute__((aligned(32)));

ETH_DMADescTypeDef RX_DESC[ETH_RX_DESC_CNT] __attribute__((section(".RxDecripSection"))) __attribute__((aligned(32)));

void SystemClock_Config(void);

static void MX_ETH_Init(void);

static void MX_GPIO_Init(void);

static void MX_TIM2_Init(void);

static void MX_RTC_Init(void);

static void MX_UART_Init(void);

static void MX_I2C_INIT(void);

static void lwipProc( void * pvParameters );

static void uartProc(void* pvParameters);


#define UART_RX_QUEUE_SIZE 256U
static uint8_t rxByte;
static QueueHandle_t uartRxQueue;
static volatile uint32_t rxBytes, rxDropped, uartErrors, rxRestartErrors;
static volatile uint32_t rxFaultGeneration;
static uint32_t txErrors;
static volatile uint32_t networkPolls, networkReady;

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* Works both after the custom bootloader and direct debugger launch. */
    extern const uint32_t g_pfnVectors[];
    SCB->VTOR = (uint32_t)g_pfnVectors;
    __DSB();
    __ISB();
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_RTC_Init();
    MX_UART_Init();
    MX_I2C_INIT();

    settings_init();
    if (!flash_access_init()) Error_Handler();
    uartRxQueue = xQueueCreate(UART_RX_QUEUE_SIZE, sizeof(uint8_t));
    if (uartRxQueue == NULL) Error_Handler();
    vQueueAddToRegistry(uartRxQueue, "uart-rx");

    HAL_NVIC_SetPriority(TIM2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) Error_Handler();

    /* Same priority: preemptive time slicing keeps both tasks progressing.
     * Only lwipProc owns the NO_SYS lwIP stack and Ethernet driver.
     * USART RX is armed inside uartProc after the scheduler has started. */
    if (xTaskCreate(lwipProc, "lwip", 1024, NULL, 1, NULL) != pdPASS ||
        xTaskCreate(uartProc, "cli", 1024, NULL, 1, NULL) != pdPASS)
        Error_Handler();
    vTaskStartScheduler();
    Error_Handler();
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

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart->Instance != USART3) return;
    BaseType_t woken = pdFALSE;
    ++rxBytes;
    /* Queue copies the byte before the receive buffer is reused by HAL. */
    if (xQueueSendFromISR(uartRxQueue, &rxByte, &woken) != pdPASS) {
        ++rxDropped;
        ++rxFaultGeneration;
    }
    if (HAL_UART_Receive_IT(uart, &rxByte, 1) != HAL_OK) {
        ++rxRestartErrors;
        ++rxFaultGeneration;
    }
    portYIELD_FROM_ISR(woken);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart->Instance != USART3) return;
    ++uartErrors;
    ++rxFaultGeneration;
    /* HAL ends RX on overrun. Non-blocking errors can leave RX armed. */
    if (uart->RxState == HAL_UART_STATE_READY &&
        HAL_UART_Receive_IT(uart, &rxByte, 1) != HAL_OK)
        ++rxRestartErrors;
}

void app_cli_putchar(char ch)
{
    if (HAL_UART_Transmit(&huart, (uint8_t *)&ch, 1, 20) != HAL_OK)
        ++txErrors;
}

void app_get_stats(AppStats *stats)
{
    taskENTER_CRITICAL();
    stats->rx_bytes = rxBytes;
    stats->rx_dropped = rxDropped;
    stats->uart_errors = uartErrors;
    stats->rx_restart_errors = rxRestartErrors;
    stats->tx_errors = txErrors;
    stats->network_polls = networkPolls;
    stats->network_ready = networkReady;
    taskEXIT_CRITICAL();
    stats->queue_used = uxQueueMessagesWaiting(uartRxQueue);
    stats->queue_capacity = UART_RX_QUEUE_SIZE;
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
		Error_Handler();
	}

}


static void lwipProc(void *pvParameters)
{
    (void)pvParameters;
    MX_ETH_Init();
    MX_LWIP_Init();
    tftp_example_init_server();
    networkReady = 1;
    for (;;) {
        /* MX_LWIP_Process already calls sys_check_timeouts. */
        MX_LWIP_Process();
        ++networkPolls;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void uartProc(void *pvParameters)
{
    (void)pvParameters;
    uint8_t ch;
    uint32_t seenFault = 0;
    app_cli_init();
    if (HAL_UART_Receive_IT(&huart, &rxByte, 1) != HAL_OK) Error_Handler();
    for (;;) {
        BaseType_t received = xQueueReceive(uartRxQueue, &ch, pdMS_TO_TICKS(100));
        bool lost;
        /* Serialize task-side rearming with the UART interrupt. */
        taskENTER_CRITICAL();
        if (huart.RxState == HAL_UART_STATE_READY &&
            HAL_UART_Receive_IT(&huart, &rxByte, 1) != HAL_OK) {
            ++rxRestartErrors;
            ++rxFaultGeneration;
        }
        lost = seenFault != rxFaultGeneration;
        if (lost) {
            seenFault = rxFaultGeneration;
            xQueueReset(uartRxQueue);
        }
        taskEXIT_CRITICAL();
        if (lost) app_cli_input_lost();
        else if (received == pdPASS) app_cli_rx((char)ch);
    }
}

void vApplicationMallocFailedHook(void) { Error_Handler(); }
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    Error_Handler();
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
