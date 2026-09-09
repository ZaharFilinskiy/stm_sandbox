/***********************************************************************
 * @file   startc/main.c
 * @author Z.Filinskiy
 * @brief  Бутлоадер хедер
 * ********************************************************************/
#ifndef BYTLODER_H
#define BYTLODER_H

#ifndef HANDLER_H
    #include <handler.h>
#endif

void Default_Handler(void)
{
    while (1)
    {
    }
}


/* ---------------------------------------------------------
 * External IRQ handlers
 * --------------------------------------------------------- */

void WWDG_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void PVD_AVD_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void TAMP_STAMP_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void RTC_WKUP_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void EXTI0_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void EXTI1_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void EXTI2_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void EXTI3_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void EXTI4_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));

void DMA1_Stream0_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream1_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream2_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream3_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream4_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream5_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream6_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));

void ADC_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void FDCAN1_IT0_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void FDCAN2_IT0_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void FDCAN1_IT1_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void FDCAN2_IT1_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));

void EXTI9_5_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));

void TIM1_BRK_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void TIM1_UP_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void TIM1_TRG_COM_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void TIM1_CC_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void TIM2_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void TIM3_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void TIM4_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));

void I2C1_EV_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void I2C1_ER_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void I2C2_EV_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void I2C2_ER_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));

void SPI1_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void SPI2_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void USART1_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void USART2_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void USART3_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));

void EXTI15_10_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void RTC_Alarm_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));

void TIM8_BRK_TIM12_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void TIM8_UP_TIM13_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));
void TIM8_TRG_COM_TIM14_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIM8_CC_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));

void DMA1_Stream7_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void FMC_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void SDMMC1_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void TIM5_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void SPI3_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void UART4_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void UART5_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void TIM6_DAC_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void TIM7_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));

void DMA2_Stream0_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream1_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream2_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream3_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream4_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));

void ETH_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void ETH_WKUP_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void FDCAN_CAL_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void CM7_SEV_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void CM4_SEV_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));

void DMA2_Stream5_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream6_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));
void DMA2_Stream7_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));

void USART6_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void I2C3_EV_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));
void I2C3_ER_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));

void OTG_HS_EP1_OUT_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void OTG_HS_EP1_IN_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void OTG_HS_WKUP_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void OTG_HS_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));

void DCMI_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void CRYP_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void HASH_RNG_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));
void FPU_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));

void UART7_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void UART8_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void SPI4_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void SPI5_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void SPI6_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));

void SAI1_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void LTDC_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void LTDC_ER_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void DMA2D_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void SAI2_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void QUADSPI_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void LPTIM1_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void CEC_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void I2C4_EV_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void I2C4_ER_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));
void SPDIF_RX_IRQHandler(void)         __attribute__((weak, alias("Default_Handler")));

void OTG_FS_EP1_OUT_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void OTG_FS_EP1_IN_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void OTG_FS_WKUP_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void OTG_FS_IRQHandler(void)          __attribute__((weak, alias("Default_Handler")));

void DMAMUX1_OVR_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));

void HRTIM1_Master_IRQHandler(void)   __attribute__((weak, alias("Default_Handler")));
void HRTIM1_TIMA_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void HRTIM1_TIMB_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void HRTIM1_TIMC_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void HRTIM1_TIMD_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void HRTIM1_TIME_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void HRTIM1_FLT_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));

void DFSDM1_FLT0_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void DFSDM1_FLT1_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void DFSDM1_FLT2_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void DFSDM1_FLT3_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));

void SAI3_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void SWPMI1_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));
void TIM15_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void TIM16_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void TIM17_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));

void MDIOS_WKUP_IRQHandler(void)       __attribute__((weak, alias("Default_Handler")));
void MDIOS_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void JPEG_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void MDMA_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));

void SDMMC2_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void HSEM1_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void HSEM2_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void ADC3_IRQHandler(void)               __attribute__((weak, alias("Default_Handler")));
void DMAMUX2_OVR_IRQHandler(void)        __attribute__((weak, alias("Default_Handler")));

void BDMA_Channel0_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void BDMA_Channel1_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void BDMA_Channel2_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void BDMA_Channel3_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void BDMA_Channel4_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void BDMA_Channel5_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void BDMA_Channel6_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));
void BDMA_Channel7_IRQHandler(void)     __attribute__((weak, alias("Default_Handler")));

void COMP1_IRQHandler(void)              __attribute__((weak, alias("Default_Handler")));
void LPTIM2_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void LPTIM3_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void LPTIM4_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void LPTIM5_IRQHandler(void)             __attribute__((weak, alias("Default_Handler")));
void LPUART1_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));

void WWDG_RST_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void CRS_IRQHandler(void)                 __attribute__((weak, alias("Default_Handler")));
void ECC_IRQHandler(void)                 __attribute__((weak, alias("Default_Handler")));
void SAI4_IRQHandler(void)                 __attribute__((weak, alias("Default_Handler")));
void HOLD_CORE_IRQHandler(void)            __attribute__((weak, alias("Default_Handler")));
void WAKEUP_PIN_IRQHandler(void)           __attribute__((weak, alias("Default_Handler")));


















#endif