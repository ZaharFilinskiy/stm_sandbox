/**
 * @file app_cli.h
 * @brief UART CLI API; init/rx/poll/output вызываются только в uartProc.
 */
#ifndef APP_CLI_H
#define APP_CLI_H

#include <stdint.h>

typedef struct
{
    uint32_t rx_bytes, rx_dropped, uart_errors, rx_restart_errors, tx_errors;
    uint32_t queue_used, queue_capacity;
    uint32_t network_polls, network_ready;
} AppStats;

void app_get_stats(AppStats *stats);
void app_cli_init(void);
void app_cli_poll(void);
void app_cli_rx(char ch);
void app_cli_input_lost(void);
/* Implemented by main.c; called only by the CLI task. */
void app_cli_putchar(char ch);

#endif
