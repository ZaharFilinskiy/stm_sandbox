/**
 * @file app_cli.c
 * @brief UART-консоль устройства и вывод диагностических снимков камеры.
 * UART вывод выполняется только в uartProc; raw lwIP здесь не вызывается.
 * Команды камеры: camera, camera watch on, camera watch off.
 */

#include "app_cli.h"
#include "camera_stream.h"
#include "settings.h"
#include "embedded_cli.h"
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static struct embedded_cli cli;
static bool discard_line, previous_cr, camera_watch, camera_boot_reported;
static uint32_t camera_watch_tick;

/**
 * @brief Адаптер вывода embedded_cli в единственный UART-писатель.
 *
 * Вызывается из uartProc; параметры data/last здесь не используются.
 */
static void output(void *data, char ch, bool last)
{
    (void)data;

    (void)last;

    app_cli_putchar(ch);
}

static void print(const char *format, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Форматирует ограниченную строку и последовательно отправляет её в UART.
 *
 * Буфер 192 байта. Вызывать только из задачи CLI, не из IRQ/lwipProc.
 */
static void print(const char *format, ...)
{
    char buffer[192];

    va_list args;

    va_start(args, format);

    vsnprintf(buffer, sizeof(buffer), format, args);

    va_end(args);

    for (const char *p = buffer; *p; ++p)
        app_cli_putchar(*p);
}

/**
 * @brief Печатает имя устройства и IPv4 настройки указанной конфигурации.
 *
 * @param s Настройки RAM либо снимок, применённый при старте.
 */
static void show_settings(const DeviceSettings *s)
{
    print("name=%s\r\nip=%u.%u.%u.%u\r\nmask=%u.%u.%u.%u\r\ngateway=%u.%u.%u.%u\r\n", s->name,
          s->ip[0], s->ip[1], s->ip[2], s->ip[3], s->mask[0], s->mask[1], s->mask[2], s->mask[3],
          s->gateway[0], s->gateway[1], s->gateway[2], s->gateway[3]);
}

/**
 * @brief Печатает ресурсы RTOS, ошибки UART и активные сетевые настройки.
 *
 * Команда status читает только снимки и счётчики; конфигурацию не меняет.
 */
static void status(void)
{
    AppStats s;

    TaskStatus_t tasks[8];

    app_get_stats(&s);

    print("uptime=%lu ms, cpu=%lu Hz, tasks=%lu\r\n", (unsigned long)HAL_GetTick(),
          (unsigned long)SystemCoreClock, (unsigned long)uxTaskGetNumberOfTasks());

    print("RTOS heap: total=%lu, free=%lu, min-free=%lu bytes\r\n",
          (unsigned long)configTOTAL_HEAP_SIZE, (unsigned long)xPortGetFreeHeapSize(),
          (unsigned long)xPortGetMinimumEverFreeHeapSize());

    UBaseType_t count = uxTaskGetSystemState(tasks, 8, NULL);

    for (UBaseType_t i = 0; i < count; ++i)
        print("task=%s state=%u priority=%lu stack-min-free=%lu bytes\r\n", tasks[i].pcTaskName,
              (unsigned)tasks[i].eCurrentState, (unsigned long)tasks[i].uxCurrentPriority,
              (unsigned long)(tasks[i].usStackHighWaterMark * sizeof(StackType_t)));

    if (!count)
        print("Task snapshot unavailable (capacity=8).\r\n");

    print("UART RX=%lu dropped=%lu errors=%lu rearm-errors=%lu TX-errors=%lu queue=%lu/%lu\r\n",
          (unsigned long)s.rx_bytes, (unsigned long)s.rx_dropped, (unsigned long)s.uart_errors,
          (unsigned long)s.rx_restart_errors, (unsigned long)s.tx_errors,
          (unsigned long)s.queue_used, (unsigned long)s.queue_capacity);

    print("network-ready=%lu polls=%lu; settings=%s, unsaved=%s\r\n",
          (unsigned long)s.network_ready, (unsigned long)s.network_polls,
          settings_loaded() ? "flash" : "defaults", settings_dirty() ? "yes" : "no");

    print("Active boot configuration (network applies after reboot):\r\n");

    show_settings(settings_boot());
}

/**
 * @brief Печатает один подробный снимок состояния камеры.
 *
 * Данные передаются из lwipProc через camera_get_diagnostics.
 * GPIO sample — уровни в момент чтения, а не измеренные частоты.
 */
static void camera_status(void)
{
    CameraDiagnostics d;

    camera_get_diagnostics(&d);

    const char *state = !d.ready            ? "initializing"
                        : d.init_error == 0 ? "ready"
                        : d.init_error == 1 ? "HAL-init-error"
                        : d.init_error == 2 ? "SCCB-or-PID-error"
                                            : "register-write-error";

    const char *reason = d.last_reason == 1   ? "start-error"
                         : d.last_reason == 2 ? "DCMI-error"
                         : d.last_reason == 3 ? "timeout"
                         : d.last_reason == 4 ? "frame-ok"
                         : d.last_reason == 5 ? "short-frame"
                                              : "none";

    print("CAM state=%s init=%lu PID=0x%04lX XCLK=HSI/4 (expected 16000000 Hz)\r\n", state,
          (unsigned long)d.init_error, (unsigned long)d.pid);

    print("CAM SCCB status=%lu error=0x%08lX failed-bank=0x%02lX reg=0x%02lX\r\n",
          (unsigned long)d.sccb_status, (unsigned long)d.i2c_error, (unsigned long)d.fail_bank,
          (unsigned long)d.fail_reg);

    print("CAM frames=%lu errors=%lu capturing=%lu valid=%lu id=0x%08lX age=%lu ms\r\n",
          (unsigned long)d.frames, (unsigned long)d.capture_errors, (unsigned long)d.capturing,
          (unsigned long)d.valid, (unsigned long)d.frame_id, (unsigned long)d.age_ms);

    print("CAM UDP requests=%lu replies=%lu send-errors=%lu\r\n", (unsigned long)d.requests,
          (unsigned long)d.replies, (unsigned long)d.send_errors);

    print("CAM live: DMA remaining=%lu words (payload=38400, capacity=38432) err=0x%08lX DCMI "
          "err=0x%08lX\r\n",
          (unsigned long)d.dma_remaining, (unsigned long)d.dma_error, (unsigned long)d.dcmi_error);

    print("CAM DCMI CR=0x%08lX SR=0x%08lX RIS=0x%08lX\r\n", (unsigned long)d.dcmi_cr,
          (unsigned long)d.dcmi_sr, (unsigned long)d.dcmi_ris);

    print("CAM last=%s remaining=%lu DMA-err=0x%08lX DCMI-err=0x%08lX\r\n", reason,
          (unsigned long)d.last_remaining, (unsigned long)d.last_dma_error,
          (unsigned long)d.last_dcmi_error);

    print("CAM pre-stop CR=0x%08lX SR=0x%08lX RIS=0x%08lX\r\n", (unsigned long)d.last_cr,
          (unsigned long)d.last_sr, (unsigned long)d.last_ris);

    print("CAM GPIO sample: HREF(PA4)=%lu VSYNC(PB7)=%lu PCLK(PA6)=%lu RESET(PD4)=%lu "
          "PWDN(PD5)=%lu\r\n",
          (unsigned long)d.href, (unsigned long)d.vsync, (unsigned long)d.pclk,
          (unsigned long)d.reset, (unsigned long)d.pwdn);
}

/* Only the CLI task writes UART, including periodic reports. */
/**
 * @brief Выводит стартовый отчёт камеры и отчёты режима watch.
 *
 * Вызывается только из uartProc. Пока пользователь печатает строку,
 * периодический вывод откладывается; частота watch около одного раза в секунду.
 */
void app_cli_poll(void)
{
    CameraDiagnostics d;

    camera_get_diagnostics(&d);

    bool boot = !camera_boot_reported && d.ready;

    if (boot || (camera_watch && HAL_GetTick() - camera_watch_tick >= 1000U))
    {
        /* Avoid overwriting a command being typed or a discarded line. */
        if (cli.len || discard_line)
            return;

        camera_boot_reported = true;

        camera_watch_tick = HAL_GetTick();

        print("\r\n");

        camera_status();

        embedded_cli_prompt(&cli);
    }
}

/**
 * @brief Разбирает завершённую команду и вызывает соответствующее действие.
 *
 * @param argc Число аргументов.
 * @param argv Аргументы embedded_cli. Проверяется точное количество.
 * Изменения settings сначала попадают в RAM; применение сети требует reboot.
 */
static void command(int argc, char **argv)
{
    if (argc == 1 && !strcmp(argv[0], "help"))
    {
        print("help | status | reboot | camera | camera watch on|off\r\nsettings show | settings "
              "defaults | settings save\r\n");

        print("settings set <name|ip|mask|gateway> <value>\r\nChanges: RAM -> save -> reboot. "
              "Gateway 0.0.0.0 disables routing.\r\n");
    }
    else if (argc == 1 && !strcmp(argv[0], "status"))
    {
        status();
    }
    else if (argc == 1 && !strcmp(argv[0], "camera"))
    {
        camera_status();
    }
    else if (argc == 3 && !strcmp(argv[0], "camera") && !strcmp(argv[1], "watch") &&
             (!strcmp(argv[2], "on") || !strcmp(argv[2], "off")))
    {
        camera_watch = !strcmp(argv[2], "on");

        camera_watch_tick = HAL_GetTick();

        print("CAM watch=%s (1 second reports)\r\n", camera_watch ? "on" : "off");
    }
    else if (argc == 1 && !strcmp(argv[0], "reboot"))
    {
        print("Rebooting; unsaved changes are discarded.\r\n");

        /* Blocking UART output has completed, including the last stop bit. */
        NVIC_SystemReset();
    }
    else if (argc == 2 && !strcmp(argv[0], "settings") && !strcmp(argv[1], "show"))
    {
        show_settings(settings_get());

        print("unsaved=%s valid=%s; apply: settings save, reboot\r\n",
              settings_dirty() ? "yes" : "no", settings_validate(settings_get()) ? "yes" : "no");
    }
    else if (argc == 2 && !strcmp(argv[0], "settings") && !strcmp(argv[1], "defaults"))
    {
        settings_defaults();

        print("Defaults staged in RAM. Use settings save, then reboot.\r\n");
    }
    else if (argc == 2 && !strcmp(argv[0], "settings") && !strcmp(argv[1], "save"))
    {
        if (!settings_validate(settings_get()))
        {
            print("Invalid network: check mask, host IP and gateway subnet.\r\n");
        }
        else
        {
            print("Saving... wait for completion before sending another command.\r\n");

            print(settings_save()
                      ? "Saved. Apply with reboot.\r\n"
                      : "Save failed (Flash layout/protection, busy writer, or write error).\r\n");
        }
    }
    else if (argc == 4 && !strcmp(argv[0], "settings") && !strcmp(argv[1], "set"))
    {
        print(settings_set(argv[2], argv[3]) ? "Updated in RAM.\r\n"
                                             : "Unknown key or invalid value.\r\n");
    }
    else if (argc)
    {
        print("Unknown command or argument count. Use help.\r\n");
    }
}

/**
 * @brief Создаёт CLI, сбрасывает состояние ввода и выводит приглашение.
 *
 * Вызывается из uartProc после запуска scheduler, перед приёмом UART RX.
 */
void app_cli_init(void)
{
    embedded_cli_init(&cli, "cm7> ", output, NULL);

    discard_line = previous_cr = camera_watch = camera_boot_reported = false;

    print("\r\nCM7 CLI. Type help. Settings: %s.\r\n", settings_loaded() ? "flash" : "defaults");

    embedded_cli_prompt(&cli);
}

/**
 * @brief Отбрасывает повреждённую строку до следующего Enter.
 *
 * Применяется после потери RX байтов или переполнения длины команды.
 */
void app_cli_input_lost(void)
{
    embedded_cli_init(&cli, "cm7> ", output, NULL);

    discard_line = true;

    previous_cr = false;

    print("\r\nInput lost or too long; command discarded. Press Enter.\r\n");
}

/**
 * @brief Принимает один байт UART, собирает строку и выполняет команду.
 *
 * @param ch Принятый символ.
 * CRLF обрабатывается как один Enter; повреждённая строка не выполняется.
 */
void app_cli_rx(char ch)
{
    if (ch == '\n' && previous_cr)
    {
        previous_cr = false;

        return;
    }

    previous_cr = ch == '\r';

    if (discard_line)
    {
        if (ch == '\r' || ch == '\n')
        {
            discard_line = false;

            embedded_cli_prompt(&cli);
        }

        return;
    }

    if (cli.len >= EMBEDDED_CLI_MAX_LINE - 1U && ch >= ' ' && ch <= '~')
    {
        app_cli_input_lost();

        return;
    }

    if (embedded_cli_insert_char(&cli, ch))
    {
        char **argv;

        int argc = embedded_cli_argc(&cli, &argv);

        command(argc, argv);

        embedded_cli_prompt(&cli);
    }
}
