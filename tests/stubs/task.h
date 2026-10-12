#ifndef TEST_TASK_H
#define TEST_TASK_H
#include "FreeRTOS.h"
typedef struct {
    const char *pcTaskName;
    unsigned eCurrentState;
    UBaseType_t uxCurrentPriority;
    uint32_t usStackHighWaterMark;
} TaskStatus_t;
UBaseType_t uxTaskGetNumberOfTasks(void);
UBaseType_t uxTaskGetSystemState(TaskStatus_t *, UBaseType_t, uint32_t *);
#endif
