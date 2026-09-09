/**************************************************************************//**
 * @file     arch/mips/24k/arch_cpy.h
 * @author   A.Leutski
 * @brief    MIPS-24K Aligned 32-bit word access memory copy/fill functions.
 * @note     This is _internal_ header. No include guard here.
 *            It must be ONLY (& the ONLY) included only by
 *            actual CPU model include file.
 ******************************************************************************/
#include "stm32h7xx_hal.h"
#include <stdint.h>

#define CCASM_OUT_REG(x)    "=r" (x)
#define CCASM_INOUT_REG(x)  "+r" (x)
#define CCASM_EARLY_REG(x)  "=&r" (x)  
#define CCASM_IN_REG(x)     "r"  (x)

/**
 * @brief Does word fill memory block with pattern.
 * @arg  sdest : 32-bit aligned pointer to start of memory area to fill 
 * @arg  pattern : A 32-bit value to fill memoty area
 * @arg  edest : 32-bit aligned pointer to the end of memory area to fill
 * @note  It is safe to pass zero sized destinaton block,
 * 			(numerically sdest == edest), no copy would be made.
 */
uint32_t* arch_mempset32(uint32_t* sdest, uint32_t pattern, const uint32_t* edest)
{
	register uint32_t*  _sdst;

	__ASM ("1:	cmp     %0, %1       \n"
	       "	itt     lo           \n"
	       "	 strlo  %2, [%0],#4  \n"  /* In post-indexed mode */
	       "	 blo    1b" /* It is permitted to use b{cond} as the last instruction within IT block */
	: CCASM_OUT_REG(_sdst)
	: CCASM_IN_REG(edest), CCASM_IN_REG(pattern), "0" (sdest)
	: "cc", "memory");

	return _sdst;
}

/**
 * @brief Does a word-by-word copy memory block.
 * @arg sdest : 32-bit aligned pointer to Start of copy destination block.
 * @arg src   : 32-bit aligned pointer to Start of copy source block.
 * @arg edest : 32-bit aligned pointer to End of copy destination block.
 * @note  It is safe to pass zero sized destinaton block,
 * 			(numerically sdest == edest), no copy would be made.
 */
uint32_t* arch_mempcpy32(uint32_t* sdest, const uint32_t* src, const uint32_t* edest)
{
	register uint32_t  tmp, *_sdst, *_ssrc;

	__ASM ("1:	cmp     %0, %3      \n"
	       "	ittt    lo          \n"
	       "	 ldrlo  %2, [%1],#4 \n" /* In post-indexed mode */
	       "	 strlo  %2, [%0],#4 \n" /* In post-indexed mode */
	       "	 blo    1b"  /* It is permitted to use b{cond} as the last instruction within IT block */
	: CCASM_OUT_REG(_sdst),  CCASM_OUT_REG(_ssrc), CCASM_EARLY_REG(tmp)
	: CCASM_IN_REG(edest), "0" (sdest), "1" (src)
	: "cc", "memory");

	return _sdst;
}

