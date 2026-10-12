#include "app_cli.h"
#include "settings.h"
#include "embedded_cli.h"
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static struct embedded_cli cli;
static bool discard_line, previous_cr;

static void output(void *data, char ch, bool last)
{
    (void)data;
    (void)last;
    app_cli_putchar(ch);
}

static void print(const char *format, ...) __attribute__((format(printf, 1, 2)));
static void print(const char *format, ...)
{
    char buffer[192];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    for (const char *p = buffer; *p; ++p) app_cli_putchar(*p);
}

static void show_settings(const DeviceSettings *s)
{
    print("name=%s\r\nip=%u.%u.%u.%u\r\nmask=%u.%u.%u.%u\r\ngateway=%u.%u.%u.%u\r\n",
          s->name, s->ip[0], s->ip[1], s->ip[2], s->ip[3],
          s->mask[0], s->mask[1], s->mask[2], s->mask[3],
          s->gateway[0], s->gateway[1], s->gateway[2], s->gateway[3]);
}

static void status(void)
{
    AppStats s;
    TaskStatus_t tasks[8];
    app_get_stats(&s);
    print("uptime=%lu ms, cpu=%lu Hz, tasks=%lu\r\n",
          (unsigned long)HAL_GetTick(), (unsigned long)SystemCoreClock,
          (unsigned long)uxTaskGetNumberOfTasks());
    print("RTOS heap: total=%lu, free=%lu, min-free=%lu bytes\r\n",
          (unsigned long)configTOTAL_HEAP_SIZE, (unsigned long)xPortGetFreeHeapSize(),
          (unsigned long)xPortGetMinimumEverFreeHeapSize());
    UBaseType_t count = uxTaskGetSystemState(tasks, 8, NULL);
    for (UBaseType_t i = 0; i < count; ++i)
        print("task=%s state=%u priority=%lu stack-min-free=%lu bytes\r\n",
              tasks[i].pcTaskName, (unsigned)tasks[i].eCurrentState,
              (unsigned long)tasks[i].uxCurrentPriority,
              (unsigned long)(tasks[i].usStackHighWaterMark * sizeof(StackType_t)));
    if (!count) print("Task snapshot unavailable (capacity=8).\r\n");
    print("UART RX=%lu dropped=%lu errors=%lu rearm-errors=%lu TX-errors=%lu queue=%lu/%lu\r\n",
          (unsigned long)s.rx_bytes, (unsigned long)s.rx_dropped,
          (unsigned long)s.uart_errors, (unsigned long)s.rx_restart_errors,
          (unsigned long)s.tx_errors, (unsigned long)s.queue_used, (unsigned long)s.queue_capacity);
    print("network-ready=%lu polls=%lu; settings=%s, unsaved=%s\r\n",
          (unsigned long)s.network_ready, (unsigned long)s.network_polls,
          settings_loaded() ? "flash" : "defaults", settings_dirty() ? "yes" : "no");
    print("Active boot configuration (network applies after reboot):\r\n");
    show_settings(settings_boot());
}

static void command(int argc, char **argv)
{
    if (argc == 1 && !strcmp(argv[0], "help")) {
        print("help | status | reboot\r\nsettings show | settings defaults | settings save\r\n");
        print("settings set <name|ip|mask|gateway> <value>\r\nChanges: RAM -> save -> reboot. Gateway 0.0.0.0 disables routing.\r\n");
    } else if (argc == 1 && !strcmp(argv[0], "status")) {
        status();
    } else if (argc == 1 && !strcmp(argv[0], "reboot")) {
        print("Rebooting; unsaved changes are discarded.\r\n");
        /* Blocking UART output has completed, including the last stop bit. */
        NVIC_SystemReset();
    } else if (argc == 2 && !strcmp(argv[0], "settings") && !strcmp(argv[1], "show")) {
        show_settings(settings_get());
        print("unsaved=%s valid=%s; apply: settings save, reboot\r\n",
              settings_dirty() ? "yes" : "no", settings_validate(settings_get()) ? "yes" : "no");
    } else if (argc == 2 && !strcmp(argv[0], "settings") && !strcmp(argv[1], "defaults")) {
        settings_defaults();
        print("Defaults staged in RAM. Use settings save, then reboot.\r\n");
    } else if (argc == 2 && !strcmp(argv[0], "settings") && !strcmp(argv[1], "save")) {
        if (!settings_validate(settings_get())) {
            print("Invalid network: check mask, host IP and gateway subnet.\r\n");
        } else {
            print("Saving... wait for completion before sending another command.\r\n");
            print(settings_save() ? "Saved. Apply with reboot.\r\n" :
                  "Save failed (Flash layout/protection, busy writer, or write error).\r\n");
        }
    } else if (argc == 4 && !strcmp(argv[0], "settings") && !strcmp(argv[1], "set")) {
        print(settings_set(argv[2], argv[3]) ? "Updated in RAM.\r\n" : "Unknown key or invalid value.\r\n");
    } else if (argc) {
        print("Unknown command or argument count. Use help.\r\n");
    }
}

void app_cli_init(void)
{
    embedded_cli_init(&cli, "cm7> ", output, NULL);
    discard_line = previous_cr = false;
    print("\r\nCM7 CLI. Type help. Settings: %s.\r\n", settings_loaded() ? "flash" : "defaults");
    embedded_cli_prompt(&cli);
}

void app_cli_input_lost(void)
{
    embedded_cli_init(&cli, "cm7> ", output, NULL);
    discard_line = true;
    previous_cr = false;
    print("\r\nInput lost or too long; command discarded. Press Enter.\r\n");
}

void app_cli_rx(char ch)
{
    if (ch == '\n' && previous_cr) { previous_cr = false; return; }
    previous_cr = ch == '\r';
    if (discard_line) {
        if (ch == '\r' || ch == '\n') {
            discard_line = false;
            embedded_cli_prompt(&cli);
        }
        return;
    }
    if (cli.len >= EMBEDDED_CLI_MAX_LINE - 1U && ch >= ' ' && ch <= '~') {
        app_cli_input_lost();
        return;
    }
    if (embedded_cli_insert_char(&cli, ch)) {
        char **argv;
        int argc = embedded_cli_argc(&cli, &argv);
        command(argc, argv);
        embedded_cli_prompt(&cli);
    }
}
