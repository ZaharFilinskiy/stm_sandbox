#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
extern uint32_t SystemCoreClock;
uint32_t HAL_GetTick(void);
void NVIC_SystemReset(void);
#endif
