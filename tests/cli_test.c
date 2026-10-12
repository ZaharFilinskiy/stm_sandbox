#include "app_cli.h"
#include "settings.h"
#include "settings_storage.h"
#include "task.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char output[32768];
static size_t used;
static unsigned resets;
static unsigned char flash[2][SETTINGS_RECORD_SIZE];
uint32_t SystemCoreClock = 64000000U;
uint32_t HAL_GetTick(void) { return 1000; }
void NVIC_SystemReset(void) { ++resets; }
size_t xPortGetFreeHeapSize(void) { return 4000; }
size_t xPortGetMinimumEverFreeHeapSize(void) { return 3500; }
UBaseType_t uxTaskGetNumberOfTasks(void) { return 2; }
UBaseType_t uxTaskGetSystemState(TaskStatus_t *tasks, UBaseType_t n, uint32_t *runtime)
{
    (void)runtime;
    assert(n >= 2);
    tasks[0] = (TaskStatus_t){"cli", 0, 1, 400};
    tasks[1] = (TaskStatus_t){"lwip", 2, 1, 500};
    return 2;
}
void app_get_stats(AppStats *stats)
{
    memset(stats, 0, sizeof(*stats));
    stats->queue_capacity = 256;
    stats->network_ready = 1;
}
void app_cli_putchar(char ch)
{
    assert(used + 1 < sizeof(output));
    output[used++] = ch;
    output[used] = 0;
}
bool settings_storage_read(unsigned slot, void *record)
{
    memcpy(record, flash[slot], SETTINGS_RECORD_SIZE);
    return true;
}
bool settings_storage_write(unsigned slot, const void *record)
{
    memcpy(flash[slot], record, SETTINGS_RECORD_SIZE);
    return true;
}
static void input(const char *s)
{
    used = 0;
    output[0] = 0;
    while (*s) app_cli_rx(*s++);
}

int main(void)
{
    memset(flash, 0xFF, sizeof(flash));
    settings_init();
    app_cli_init();
    input("help\r\n");
    assert(strstr(output, "settings set"));
    char *prompt = strstr(output, "cm7> ");
    assert(prompt && !strstr(prompt + 1, "cm7> ")); /* CRLF is one Enter. */
    input("settings set name test-board\r");
    assert(!strcmp(settings_get()->name, "test-board"));
    input("settings set ip 300.1.2.3\r");
    assert(strstr(output, "invalid value"));
    input("settings save extra\r");
    assert(!settings_loaded());
    input("settings save\r");
    assert(settings_loaded() && strstr(output, "Saved."));
    input("status\r");
    assert(strstr(output, "task=cli") && strstr(output, "task=lwip"));
    assert(strstr(output, "min-free=3500") && strstr(output, "stack-min-free=1600"));
    input("reboot extra\r");
    assert(resets == 0);
    input("reb");
    app_cli_input_lost();
    input("oot\r");
    assert(resets == 0);
    input("reboot\r\n");
    assert(resets == 1);
    used = 0;
    for (unsigned i = 0; i < 140; ++i) app_cli_rx('x');
    input("reboot\r");
    assert(resets == 1); /* Overflow must discard the entire line. */
    input("settings defaults\r");
    assert(settings_dirty() && !strcmp(settings_get()->name, "cm7-device"));
    puts("PASS: CLI commands, CRLF, validation, resources, lost input and line overflow");
    return 0;
}
