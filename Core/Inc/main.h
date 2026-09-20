/**************************************************************************
 * @file       Core/inc/main.h
 * @author     Z.Filinskiy
 * @brief      Program start and common board state API implementation header
 *************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"
#ifndef _STDLIB_H_
  #include <stdlib.h>
#endif

#ifndef _STRING_H_
  #include <string.h>
#endif

#ifndef _STDBOOL_H
  #include <stdbool.h>
#endif

#include "ssd1306.h"

#include "opt.h"

#include "ssd1306_tests.h"

#include "ssd1306_fonts.h"

void Error_Handler(void);


#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
