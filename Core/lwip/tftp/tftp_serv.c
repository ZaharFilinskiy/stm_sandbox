/*
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Dirk Ziegelmeier <dziegel@gmx.de>
 *
 */

#include <stdio.h>

#include "lwip/apps/tftp_server.h"
#include "tftp_serv.h"

#include <string.h>
#ifndef STM32H7xx_HAL_FLASH_H
  #include <stm32h7xx_hal_flash.h>
#endif

#include <main.h>
#include "flash_access.h"
#if LWIP_UDP

/* Define a base directory for TFTP access
 * ATTENTION: This code does NOT check for sandboxing,
 * i.e. '..' in paths is not checked! */
#ifndef LWIP_TFTP_EXAMPLE_BASE_DIR
#define LWIP_TFTP_EXAMPLE_BASE_DIR ""
#endif

/* Define this to a file to get via tftp client */
#ifndef LWIP_TFTP_EXAMPLE_CLIENT_FILENAME
#define LWIP_TFTP_EXAMPLE_CLIENT_FILENAME "test.bin"
#endif

/* Define this to a server IP string */
#ifndef LWIP_TFTP_EXAMPLE_CLIENT_REMOTEIP
#define LWIP_TFTP_EXAMPLE_CLIENT_REMOTEIP "192.168.0.1"
#endif

static char full_filename[256];
static uint16_t sdf = 1;
static size_t flash_written;
static void *
tftp_open_file(const char* fname, u8_t is_write)
{
  snprintf(full_filename, sizeof(full_filename), "%s%s", LWIP_TFTP_EXAMPLE_BASE_DIR, fname);
  full_filename[sizeof(full_filename)-1] = 0;

  if (is_write) {
    flash_written = 0;
    return (void*)sdf;
  } else {
    return (void*)sdf;
  }
}

static void*
tftp_open(const char* fname, const char* mode, u8_t is_write)
{
  LWIP_UNUSED_ARG(mode);
  return tftp_open_file(fname, is_write);
}

static void
tftp_close(void* handle)
{
  // fclose((FILE*)handle);
}

static int
tftp_read(void* handle, void* buf, int bytes)
{
  // int ret = fread(buf, 1, bytes, (FILE*)handle);
  // if (ret <= 0) {
  //   return -1;
  // }
  return 1;
}
static int writeflashDat(uint32_t* pbuf, size_t len, uint32_t* bootadr)
{
  /* Contain this experimental updater in the inactive 384 KiB firmware slot.
   * Never let a malformed transfer reach the bootloader, CM4 or settings. */
  const uint32_t slot_size = 384U * 1024U;
  uint32_t base = *bootadr;
  uint32_t vector = SCB->VTOR;
  if ((base != 0x08020000U && base != 0x08080000U) ||
      (vector >= base && vector < base + slot_size) ||
      flash_written > slot_size || len > slot_size - flash_written ||
      (len % 32U) != 0 || !flash_access_take()) return -1;

  int result = -1;
  if (HAL_FLASH_Unlock() != HAL_OK) goto done;
  for (size_t offset = 0; offset < len; offset += 32U) {
    uint32_t word[8] __attribute__((aligned(32)));
    memcpy(word, (const uint8_t *)pbuf + offset, sizeof(word));
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, base + flash_written,
                          (uint32_t)word) != HAL_OK) goto lock;
    flash_written += sizeof(word);
  }
  result = 0;
lock:
  if (HAL_FLASH_Lock() != HAL_OK) result = -1;
done:
  flash_access_give();
  return result;
}
static uint32_t badr;
void resetaftcloe(void)
{
    if ((SCB->VTOR >= 0x08020000U && SCB->VTOR < 0x080A0000U) ||
        !flash_access_take()) return;
    /* Разрешаем доступ к Flash */
    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        flash_access_give();
        return;
    }

    /* Разрешаем изменение Option Bytes */
    if (HAL_FLASH_OB_Unlock() != HAL_OK)
    {
        HAL_FLASH_Lock();
        flash_access_give();
        return;
    }
    FLASH_EraseInitTypeDef fer;
    uint32_t Sec;
    fer.TypeErase = FLASH_TYPEERASE_SECTORS;
    fer.Banks = FLASH_BANK_1;
    fer.Sector = FLASH_SECTOR_1;
    fer.NbSectors = 4;
    fer.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&fer,&Sec);

    if (st != HAL_OK)
    {
      Error_Handler();
    }
    
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
    flash_access_give();
    NVIC_SystemReset();
}

static int tftp_write(void* handle, struct pbuf* p)
{

  if (handle == NULL)
    {
        printf("TFTP CLOSE: NULL HANDLE!\r\n");
        return -1;
    }

  uint32_t bootadr = READ_REG(FLASH->BOOT7_CUR);

  bootadr = bootadr == 0x08080000 ? 0x08000000 : 0x08080000;

  memcpy(&badr, &bootadr, sizeof(bootadr));;
  
  while (p != NULL)
  {

    if (writeflashDat((uint32_t*)p->payload, (size_t)p->len, &bootadr) != 0)
    {
      return -1;
    }

    p = p->next;
  }

  return 0;
}
/* For TFTP client only */
static void
tftp_error(void* handle, int err, const char* msg, int size)
{
  char message[100];

  LWIP_UNUSED_ARG(handle);

  memset(message, 0, sizeof(message));
  MEMCPY(message, msg, LWIP_MIN(sizeof(message)-1, (size_t)size));

  printf("TFTP error: %d (%s)", err, message);
}

static const struct tftp_context tftp = {
  tftp_open,
  tftp_close,
  tftp_read,
  tftp_write,
  tftp_error
};

void
tftp_example_init_server(void)
{
  tftp_init_server(&tftp);
}

#endif /* LWIP_UDP */
