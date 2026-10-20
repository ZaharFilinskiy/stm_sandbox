/**
 * @file camera_stream.h
 * @brief API камеры и снимок диагностики между lwipProc и uartProc.
 * Поля — числа и счётчики; структура не содержит указателей на живые HAL handles.
 */
#pragma once
#include <stdint.h>

typedef struct
{
    /* Инициализация, успешные кадры и состояние единственного буфера. */
    uint32_t ready, init_error, pid, frames, capture_errors, capturing, valid;
    /* Raw UDP статистика, идентификатор и возраст активного захвата. */
    uint32_t requests, replies, send_errors, frame_id, age_ms;
    /* SCCB: текущая ошибка HAL и место последнего отказа записи/чтения. */
    uint32_t i2c_error, sccb_status, fail_bank, fail_reg;
    /* Текущие HAL ошибки, остаток DMA и регистры DCMI. */
    uint32_t dcmi_error, dma_error, dma_remaining, dcmi_cr, dcmi_sr, dcmi_ris;
    /* Причина результата и регистры ДО остановки DMA/DCMI. */
    uint32_t last_reason, last_remaining, last_dcmi_error, last_dma_error;
    /* Остальные pre-stop регистры и мгновенные уровни GPIO. */
    uint32_t last_cr, last_sr, last_ris, href, vsync, pclk, reset, pwdn;
} CameraDiagnostics;

/* Init/poll: only task owning NO_SYS lwIP. Getter: coherent task-safe copy. */
void camera_stream_init(void);
void camera_stream_poll(void);
void camera_get_diagnostics(CameraDiagnostics *out);
