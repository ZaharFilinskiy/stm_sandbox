#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#include <stddef.h>
#include <stdint.h>
typedef unsigned UBaseType_t;
typedef uint32_t StackType_t;
#define configTOTAL_HEAP_SIZE 15360U
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
#endif
