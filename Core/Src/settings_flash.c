#include "settings_storage.h"
#include "flash_access.h"
#include "stm32h7xx_hal.h"
#include <string.h>

/* Reserved by both application linker scripts. Keep CM4/update tools out. */
extern const uint8_t __settings_a_start__[], __settings_b_start__[];

static uint32_t slot_address(unsigned slot)
{
    return (uint32_t)(slot == 0 ? __settings_a_start__ : __settings_b_start__);
}

static bool layout_valid(void)
{
    /* This layout assumes a 2 MiB H755 with bank swapping disabled. */
    return *(const uint16_t *)FLASHSIZE_BASE == 2048U &&
           !(FLASH->OPTSR_CUR & FLASH_OPTSR_SWAP_BANK_OPT) &&
           slot_address(0) == 0x081C0000UL && slot_address(1) == 0x081E0000UL;
}

static void invalidate(uint32_t address)
{
    if (SCB->CCR & SCB_CCR_DC_Msk)
        SCB_InvalidateDCache_by_Addr((uint32_t *)address, SETTINGS_RECORD_SIZE);
    __DSB();
}

bool settings_storage_read(unsigned slot, void *record)
{
    if (slot > 1 || !layout_valid()) return false;
    uint32_t address = slot_address(slot);
    invalidate(address);
    memcpy(record, (const void *)address, SETTINGS_RECORD_SIZE);
    return true;
}

bool settings_storage_write(unsigned slot, const void *record)
{
    if (slot > 1 || !layout_valid() || !flash_access_take()) return false;
    uint32_t address = slot_address(slot), failed_sector;
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t word[8] __attribute__((aligned(32)));
    bool ok = false;
    if (HAL_FLASH_Unlock() != HAL_OK) goto done;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK2);
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = FLASH_BANK_2;
    erase.Sector = slot == 0 ? FLASH_SECTOR_6 : FLASH_SECTOR_7;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3; /* Board supply: 2.7 to 3.6 V. */
    if (HAL_FLASHEx_Erase(&erase, &failed_sector) != HAL_OK) goto lock;
    /* Commit is its own last flashword; never program a flashword twice. */
    for (unsigned offset = 0; offset < SETTINGS_RECORD_SIZE; offset += sizeof(word)) {
        memcpy(word, (const uint8_t *)record + offset, sizeof(word));
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, address + offset,
                              (uint32_t)word) != HAL_OK) goto lock;
    }
    invalidate(address);
    ok = memcmp((const void *)address, record, SETTINGS_RECORD_SIZE) == 0;
lock:
    if (HAL_FLASH_Lock() != HAL_OK) ok = false;
done:
    flash_access_give();
    return ok;
}
