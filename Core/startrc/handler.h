/***********************************************************************
 * @file   startc/handler.h
 * @author Z.Filinskiy
 * @brief  Бутлоадер хедер
 * ********************************************************************/
#ifndef HANDLER_H
    #define HANDLER_H

#include "stm32h7xx_hal.h"

#include <stdint.h>

void NMI_Handler(void);

void HardFault_Handler(void);

void MemManage_Handler(void);

void BusFault_Handler(void);

void UsageFault_Handler(void);

void SVC_Handler(void);

void DebugMon_Handler(void);

void PendSV_Handler(void);

void SysTick_Handler(void);

void FLASH_IRQHandler(void);

void RCC_IRQHandler(void);

#endif