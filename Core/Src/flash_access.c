#include "flash_access.h"
#include "FreeRTOS.h"
#include "semphr.h"

static StaticSemaphore_t storage;
static SemaphoreHandle_t mutex;
bool flash_access_init(void)
{
    mutex = xSemaphoreCreateMutexStatic(&storage);
    return mutex != NULL;
}
bool flash_access_take(void)
{
    return mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE;
}
void flash_access_give(void) { (void)xSemaphoreGive(mutex); }
