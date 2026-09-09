/***********************************************************************
 * @file   startc/main.c
 * @author Z.Filinskiy
 * @brief  Бутлоадер
 * ********************************************************************/
#include <cpy.h>

#ifndef BYTLODER_H
    #include "main.h"
#endif

extern void __libc_init_array(void);

extern uint32_t _sdata[]; // начало секции .data
extern uint32_t _edata[]; // конец секции .data
extern uint32_t _sidata[];

extern uint32_t _estack;

extern uint32_t _sbss[]; // начало секции .bss
extern uint32_t __bss_end__[]; // конец секции .bss

extern uint32_t flash_s;

#define APP_ADDRESS 0x08080000
/**
 * @brief Функция загрузчик (bootloader)
 * @note Поиск и переход у нужному сегменту
 */
int main(void)
{
    uint32_t* p;

    p = &flash_s;

    while (*p == 0xFFFFFFFFU)
    {
        p++;
    }
    
    // 1. Взять начальный Stack Pointer приложения
    uint32_t app_msp = *p;

    // 2. Взять адрес Reset_Handler приложения
    uint32_t app_reset = *(uint32_t *)(((uint8_t*)p + 4));

    // 3. Указать процессору, где находится таблица векторов
    SCB->VTOR = (uint32_t)p;

    // 4. Переключить стек
    __set_MSP(app_msp);

    // 5. Перейти в Reset_Handler
    ((void (*)(void))app_reset)();

}

void _sysini_memcpy(uint32_t* dst, const uint32_t* src, const uint32_t* pend)
{
	if (dst != src)
		arch_mempcpy32(dst, src, pend);
}

__STATIC_INLINE void CleanUP_BSS(void)
{
	arch_mempset32(_sbss, 0, __bss_end__);
}

__STATIC_INLINE void Load_DataSection(void)
{
	_sysini_memcpy(_sdata, _sidata, _edata);
}

void Reset_Handler(void)
{
    // uint32_t *src;
    // uint32_t *dst;

    /* ----------------------------------------------------
     * Установка Stack Pointer
     *
     * В C напрямую сделать:
     *     SP = &_estack;
     *
     * нельзя стандартными средствами C.
     *
     * Поэтому этот пункт либо оставляем в маленьком
     * assembly-wrapper, либо используем CMSIS.
     * ---------------------------------------------------- */

    __set_MSP((uint32_t)&_estack);

    /* ----------------------------------------------------
     * System initialization
     * ---------------------------------------------------- */

    SystemInit();

    CleanUP_BSS();
    Load_DataSection();
    // /* ----------------------------------------------------
    //  * Copy .data from Flash to RAM
    //  *
    //  * _sidata → начало .data во Flash
    //  * _sdata  → начало .data в RAM
    //  * _edata  → конец .data в RAM
    //  * ---------------------------------------------------- */

    // src = &_sidata;
    // dst = &_sdata;

    // while (dst < &_edata)
    // {
    //     *dst++ = *src++;
    // }


    // /* ----------------------------------------------------
    //  * Zero initialize .bss
    //  *
    //  * _sbss → начало .bss
    //  * _ebss → конец .bss
    //  * ---------------------------------------------------- */

    // dst = &_sbss;

    // while (dst < &_ebss)
    // {
    //     *dst++ = 0;
    // }


    /* ----------------------------------------------------
     * C++ static constructors / libc initialization
     * ---------------------------------------------------- */

    __libc_init_array();


    /* ----------------------------------------------------
     * Application
     * ---------------------------------------------------- */

    int ret = main();

    UNUSED(ret);

    /* Reset_Handler не должен возвращаться */
	__DSB();

	while (1)
	{
		__BKPT(1);
		__ISB();
	}
}
/* ---------------------------------------------------------
 * Vector table
 * --------------------------------------------------------- */

typedef void (*vector_handler_t)(void);

__attribute__((section(".isr_vector"), used))
const uintptr_t g_pfnVectors[] =
{
    (uintptr_t)&_estack,
    (uintptr_t)Reset_Handler,

    (uintptr_t)NMI_Handler,
    (uintptr_t)HardFault_Handler,
    (uintptr_t)MemManage_Handler,
    (uintptr_t)BusFault_Handler,
    (uintptr_t)UsageFault_Handler,

    0,
    0,
    0,
    0,

    (uintptr_t)SVC_Handler,
    (uintptr_t)DebugMon_Handler,

    0,

    (uintptr_t)PendSV_Handler,
    (uintptr_t)SysTick_Handler,

    /* External Interrupts */

    (uintptr_t)WWDG_IRQHandler,
    (uintptr_t)PVD_AVD_IRQHandler,
    (uintptr_t)TAMP_STAMP_IRQHandler,
    (uintptr_t)RTC_WKUP_IRQHandler,
    (uintptr_t)FLASH_IRQHandler,
    (uintptr_t)RCC_IRQHandler,
    (uintptr_t)EXTI0_IRQHandler,
    (uintptr_t)EXTI1_IRQHandler,
    (uintptr_t)EXTI2_IRQHandler,
    (uintptr_t)EXTI3_IRQHandler,
    (uintptr_t)EXTI4_IRQHandler,

    (uintptr_t)DMA1_Stream0_IRQHandler,
    (uintptr_t)DMA1_Stream1_IRQHandler,
    (uintptr_t)DMA1_Stream2_IRQHandler,
    (uintptr_t)DMA1_Stream3_IRQHandler,
    (uintptr_t)DMA1_Stream4_IRQHandler,
    (uintptr_t)DMA1_Stream5_IRQHandler,
    (uintptr_t)DMA1_Stream6_IRQHandler,

    (uintptr_t)ADC_IRQHandler,
    (uintptr_t)FDCAN1_IT0_IRQHandler,
    (uintptr_t)FDCAN2_IT0_IRQHandler,
    (uintptr_t)FDCAN1_IT1_IRQHandler,
    (uintptr_t)FDCAN2_IT1_IRQHandler,

    (uintptr_t)EXTI9_5_IRQHandler,

    (uintptr_t)TIM1_BRK_IRQHandler,
    (uintptr_t)TIM1_UP_IRQHandler,
    (uintptr_t)TIM1_TRG_COM_IRQHandler,
    (uintptr_t)TIM1_CC_IRQHandler,
    (uintptr_t)TIM2_IRQHandler,
    (uintptr_t)TIM3_IRQHandler,
    (uintptr_t)TIM4_IRQHandler,

    (uintptr_t)I2C1_EV_IRQHandler,
    (uintptr_t)I2C1_ER_IRQHandler,
    (uintptr_t)I2C2_EV_IRQHandler,
    (uintptr_t)I2C2_ER_IRQHandler,

    (uintptr_t)SPI1_IRQHandler,
    (uintptr_t)SPI2_IRQHandler,
    (uintptr_t)USART1_IRQHandler,
    (uintptr_t)USART2_IRQHandler,
    (uintptr_t)USART3_IRQHandler,

    (uintptr_t)EXTI15_10_IRQHandler,
    (uintptr_t)RTC_Alarm_IRQHandler,

    0,

    (uintptr_t)TIM8_BRK_TIM12_IRQHandler,
    (uintptr_t)TIM8_UP_TIM13_IRQHandler,
    (uintptr_t)TIM8_TRG_COM_TIM14_IRQHandler,
    (uintptr_t)TIM8_CC_IRQHandler,

    (uintptr_t)DMA1_Stream7_IRQHandler,
    (uintptr_t)FMC_IRQHandler,
    (uintptr_t)SDMMC1_IRQHandler,
    (uintptr_t)TIM5_IRQHandler,
    (uintptr_t)SPI3_IRQHandler,
    (uintptr_t)UART4_IRQHandler,
    (uintptr_t)UART5_IRQHandler,
    (uintptr_t)TIM6_DAC_IRQHandler,
    (uintptr_t)TIM7_IRQHandler,

    (uintptr_t)DMA2_Stream0_IRQHandler,
    (uintptr_t)DMA2_Stream1_IRQHandler,
    (uintptr_t)DMA2_Stream2_IRQHandler,
    (uintptr_t)DMA2_Stream3_IRQHandler,
    (uintptr_t)DMA2_Stream4_IRQHandler,

    (uintptr_t)ETH_IRQHandler,
    (uintptr_t)ETH_WKUP_IRQHandler,
    (uintptr_t)FDCAN_CAL_IRQHandler,
    (uintptr_t)CM7_SEV_IRQHandler,
    (uintptr_t)CM4_SEV_IRQHandler,

    0,
    0,

    (uintptr_t)DMA2_Stream5_IRQHandler,
    (uintptr_t)DMA2_Stream6_IRQHandler,
    (uintptr_t)DMA2_Stream7_IRQHandler,

    (uintptr_t)USART6_IRQHandler,
    (uintptr_t)I2C3_EV_IRQHandler,
    (uintptr_t)I2C3_ER_IRQHandler,

    (uintptr_t)OTG_HS_EP1_OUT_IRQHandler,
    (uintptr_t)OTG_HS_EP1_IN_IRQHandler,
    (uintptr_t)OTG_HS_WKUP_IRQHandler,
    (uintptr_t)OTG_HS_IRQHandler,

    (uintptr_t)DCMI_IRQHandler,
    (uintptr_t)CRYP_IRQHandler,
    (uintptr_t)HASH_RNG_IRQHandler,
    (uintptr_t)FPU_IRQHandler,

    (uintptr_t)UART7_IRQHandler,
    (uintptr_t)UART8_IRQHandler,
    (uintptr_t)SPI4_IRQHandler,
    (uintptr_t)SPI5_IRQHandler,
    (uintptr_t)SPI6_IRQHandler,

    (uintptr_t)SAI1_IRQHandler,
    (uintptr_t)LTDC_IRQHandler,
    (uintptr_t)LTDC_ER_IRQHandler,
    (uintptr_t)DMA2D_IRQHandler,
    (uintptr_t)SAI2_IRQHandler,
    (uintptr_t)QUADSPI_IRQHandler,
    (uintptr_t)LPTIM1_IRQHandler,
    (uintptr_t)CEC_IRQHandler,
    (uintptr_t)I2C4_EV_IRQHandler,
    (uintptr_t)I2C4_ER_IRQHandler,
    (uintptr_t)SPDIF_RX_IRQHandler,

    (uintptr_t)OTG_FS_EP1_OUT_IRQHandler,
    (uintptr_t)OTG_FS_EP1_IN_IRQHandler,
    (uintptr_t)OTG_FS_WKUP_IRQHandler,
    (uintptr_t)OTG_FS_IRQHandler,

    (uintptr_t)DMAMUX1_OVR_IRQHandler,

    (uintptr_t)HRTIM1_Master_IRQHandler,
    (uintptr_t)HRTIM1_TIMA_IRQHandler,
    (uintptr_t)HRTIM1_TIMB_IRQHandler,
    (uintptr_t)HRTIM1_TIMC_IRQHandler,
    (uintptr_t)HRTIM1_TIMD_IRQHandler,
    (uintptr_t)HRTIM1_TIME_IRQHandler,
    (uintptr_t)HRTIM1_FLT_IRQHandler,

    (uintptr_t)DFSDM1_FLT0_IRQHandler,
    (uintptr_t)DFSDM1_FLT1_IRQHandler,
    (uintptr_t)DFSDM1_FLT2_IRQHandler,
    (uintptr_t)DFSDM1_FLT3_IRQHandler,

    (uintptr_t)SAI3_IRQHandler,
    (uintptr_t)SWPMI1_IRQHandler,
    (uintptr_t)TIM15_IRQHandler,
    (uintptr_t)TIM16_IRQHandler,
    (uintptr_t)TIM17_IRQHandler,

    (uintptr_t)MDIOS_WKUP_IRQHandler,
    (uintptr_t)MDIOS_IRQHandler,
    (uintptr_t)JPEG_IRQHandler,
    (uintptr_t)MDMA_IRQHandler,

    0,

    (uintptr_t)SDMMC2_IRQHandler,
    (uintptr_t)HSEM1_IRQHandler,
    (uintptr_t)HSEM2_IRQHandler,
    (uintptr_t)ADC3_IRQHandler,
    (uintptr_t)DMAMUX2_OVR_IRQHandler,

    (uintptr_t)BDMA_Channel0_IRQHandler,
    (uintptr_t)BDMA_Channel1_IRQHandler,
    (uintptr_t)BDMA_Channel2_IRQHandler,
    (uintptr_t)BDMA_Channel3_IRQHandler,
    (uintptr_t)BDMA_Channel4_IRQHandler,
    (uintptr_t)BDMA_Channel5_IRQHandler,
    (uintptr_t)BDMA_Channel6_IRQHandler,
    (uintptr_t)BDMA_Channel7_IRQHandler,

    (uintptr_t)COMP1_IRQHandler,
    (uintptr_t)LPTIM2_IRQHandler,
    (uintptr_t)LPTIM3_IRQHandler,
    (uintptr_t)LPTIM4_IRQHandler,
    (uintptr_t)LPTIM5_IRQHandler,
    (uintptr_t)LPUART1_IRQHandler,

    (uintptr_t)WWDG_RST_IRQHandler,
    (uintptr_t)CRS_IRQHandler,
    (uintptr_t)ECC_IRQHandler,
    (uintptr_t)SAI4_IRQHandler,

    0,

    (uintptr_t)HOLD_CORE_IRQHandler,
    (uintptr_t)WAKEUP_PIN_IRQHandler
};