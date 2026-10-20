/**
 * @file camera_stream.c
 * @brief Драйвер OV2640, асинхронный захват DCMI/DMA и UDP-сервер CAM1.
 * Владелец HAL и raw lwIP — только lwipProc. IRQ выставляют флаги.
 * Payload: 320x240 RGB565; буфер DMA содержит дополнительный защитный хвост.
 * Подробный алгоритм: docs/CAMERA_ARCHITECTURE.md.
 */

#include "main.h"
#include "camera_stream.h"
#include "ov2640_settings.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

#define WIDTH 320U
#define HEIGHT 240U
#define FRAME_BYTES (WIDTH * HEIGHT * 2U)
/* После полезных пикселей возможны хвостовые байты DVP: запас до FRAME. */
#define DMA_GUARD_BYTES 128U
#define DMA_BUFFER_BYTES (FRAME_BYTES + DMA_GUARD_BYTES)
#define DMA_WORDS (DMA_BUFFER_BYTES / 4U)
_Static_assert(DMA_BUFFER_BYTES % 32U == 0, "Cache-aligned DMA size");
_Static_assert(DMA_WORDS <= 65535U, "Single DMA stream count limit");
#define CHUNK 1200U
#define PORT 5000U
static uint8_t frame[DMA_BUFFER_BYTES] __attribute__((aligned(32)));
static DCMI_HandleTypeDef dcmi;
static DMA_HandleTypeDef dma;
static I2C_HandleTypeDef sccb;
static struct udp_pcb *server;
/* IRQ -> lwipProc: только флаги, никаких сетевых вызовов из прерываний. */
static volatile unsigned completed, failed;
/* Inspect these in debugger: 0=ready, 1=camera_hardware_init init, 2=ID, 3=register writes. */
volatile uint32_t camera_init_error, camera_frames, camera_capture_errors;
/* Состояние одного буфера: захват, готовность и UDP-владелец. */
static unsigned capturing, valid, owner_set;
static uint32_t frame_id, started, touched;
static ip_addr_t owner;
static u16_t owner_port;
/* Рабочий снимок принадлежит lwipProc; published доступен CLI через getter. */
static CameraDiagnostics diagnostic, published;
static uint32_t snapshot_tick;
static unsigned init_done;
static uint8_t bank;

/**
 * @brief Собирает снимок состояния камеры и публикует его для UART.
 *
 * Контекст: только lwipProc. HAL/регистры читает владелец камеры.
 * Короткая critical section защищает лишь копирование, не печать.
 */
static void publish_diagnostics(void)
{
    diagnostic.ready = init_done;

    diagnostic.init_error = camera_init_error;

    diagnostic.frames = camera_frames;

    diagnostic.capture_errors = camera_capture_errors;

    diagnostic.capturing = capturing;

    diagnostic.valid = valid;

    diagnostic.frame_id = frame_id;

    diagnostic.age_ms = capturing ? HAL_GetTick() - started : 0;

    diagnostic.i2c_error = sccb.ErrorCode;

    diagnostic.dcmi_error = dcmi.ErrorCode;

    diagnostic.dma_error = dma.ErrorCode;

    if (dma.Instance)
        diagnostic.dma_remaining = __HAL_DMA_GET_COUNTER(&dma);

    if (dcmi.Instance)
    {
        diagnostic.dcmi_cr = DCMI->CR;

        diagnostic.dcmi_sr = DCMI->SR;

        diagnostic.dcmi_ris = DCMI->RISR;
    }

    diagnostic.href = (GPIOA->IDR >> 4) & 1U;

    diagnostic.pclk = (GPIOA->IDR >> 6) & 1U;

    diagnostic.vsync = (GPIOB->IDR >> 7) & 1U;

    diagnostic.reset = (GPIOD->ODR >> 4) & 1U;

    diagnostic.pwdn = (GPIOD->ODR >> 5) & 1U;

    taskENTER_CRITICAL();

    published = diagnostic;

    taskEXIT_CRITICAL();
}

/**
 * @brief Возвращает согласованную копию опубликованной диагностики.
 *
 * @param out Буфер вызывающей задачи; должен быть ненулевым.
 * Контекст: задача FreeRTOS. Не читает периферию и не обращается к lwIP.
 */
void camera_get_diagnostics(CameraDiagnostics *out)
{
    taskENTER_CRITICAL();

    *out = published;

    taskEXIT_CRITICAL();
}

/* Retain pre-stop registers, since HAL stop changes the peripheral state. */
/**
 * @brief Сохраняет результат захвата до изменения регистров HAL_DCMI_Stop.
 *
 * @param reason 1=start-error, 2=DCMI-error, 3=timeout, 4=frame-ok, 5=short-frame.
 * Контекст: lwipProc; DMA уже инициализирована.
 */
static void capture_result(uint32_t reason)
{
    diagnostic.last_reason = reason;

    diagnostic.last_remaining = __HAL_DMA_GET_COUNTER(&dma);

    diagnostic.last_dcmi_error = dcmi.ErrorCode;

    diagnostic.last_dma_error = dma.ErrorCode;

    diagnostic.last_cr = DCMI->CR;

    diagnostic.last_sr = DCMI->SR;

    diagnostic.last_ris = DCMI->RISR;
}

/**
 * @brief Запоминает банк и регистр последнего отказа SCCB.
 *
 * @param status Результат операции HAL I2C.
 * @param reg Адрес регистра, на котором произошёл отказ.
 * @return 1 при HAL_OK, иначе 0. Не печатает из сетевой задачи.
 */
static int sccb_result(HAL_StatusTypeDef status, uint8_t reg)
{
    if (status != HAL_OK)
    {
        diagnostic.sccb_status = status;

        diagnostic.fail_bank = bank;

        diagnostic.fail_reg = reg;

        return 0;
    }

    return 1;
}

/**
 * @brief Читает 32-битное число из сетевого заголовка без требований к выравниванию.
 *
 * @param p Указатель минимум на четыре байта, старший байт первым.
 * @return Значение в порядке байтов CPU.
 */
static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/**
 * @brief Записывает 32-битное число в сетевой заголовок, старший байт первым.
 *
 * @param p Указатель минимум на четыре доступных байта.
 * @param v Записываемое значение.
 */
static void write_be32(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24;

    p[1] = v >> 16;

    p[2] = v >> 8;

    p[3] = v;
}

/**
 * @brief Записывает один регистр OV2640 через SCCB на I2C2.
 *
 * @param reg Адрес регистра в текущем банке.
 * @param value Значение регистра.
 * @return 1 при успехе, 0 при ошибке; адрес 0x30 передаётся HAL как 0x60.
 */
static int camera_write_register(uint8_t reg, uint8_t value)
{
    uint8_t b[2] = {reg, value};

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&sccb, 0x60, b, 2, 30);

    if (!sccb_result(status, reg))
        return 0;

    if (reg == BANK_SEL)
        bank = value;

    return 1;
}

/**
 * @brief Читает один регистр OV2640 двумя отдельными SCCB операциями.
 *
 * @param reg Адрес регистра в текущем банке.
 * @param value Приёмник одного байта.
 * @return 1 при успехе, 0 при ошибке. Между фазами используется STOP.
 */
static int camera_read_register(uint8_t reg, uint8_t *value)
{
    /* SCCB reads use a STOP between subaddress write and data read. */
    if (!sccb_result(HAL_I2C_Master_Transmit(&sccb, 0x60, &reg, 1, 30), reg))
        return 0;

    return sccb_result(HAL_I2C_Master_Receive(&sccb, 0x60, value, 1, 30), reg);
}

/**
 * @brief Последовательно записывает таблицу настройки OV2640.
 *
 * @param t Пары {регистр, значение}; {0, 0} завершает таблицу.
 * @return 1 после всей таблицы, 0 на первом отказе. Порядок строк важен.
 */
static int camera_write_register_table(const uint8_t t[][2])
{
    for (unsigned i = 0; t[i][0] || t[i][1]; ++i)
        if (!camera_write_register(t[i][0], t[i][1]))
            return 0;

    return 1;
}

/**
 * @brief Назначает режим и альтернативную функцию выбранным GPIO.
 *
 * @param port Порт STM32, тактирование должно быть включено заранее.
 * @param mask Маска контактов GPIO_PIN_*.
 * @param af Номер AF; для обычных выходов значение не используется.
 * @param mode Режим GPIO_MODE_*. Внешние подтяжки SCCB задаются аппаратно.
 */
static void configure_gpio_pins(GPIO_TypeDef *port, uint32_t mask, uint32_t af, uint32_t mode)
{
    GPIO_InitTypeDef g = {0};

    g.Pin = mask;

    g.Mode = mode;

    g.Pull = GPIO_NOPULL;

    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    g.Alternate = af;

    HAL_GPIO_Init(port, &g);
}

/**
 * @brief Подготавливает GPIO, XCLK, SCCB, DCMI и DMA для модуля камеры.
 *
 * @return 1 при успешной настройке HAL, 0 при первом отказе.
 * Вызывается один раз из lwipProc до проверки PID. Здесь нет вызовов UDP.
 */
static int camera_hardware_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();

    __HAL_RCC_GPIOD_CLK_ENABLE();

    __HAL_RCC_GPIOE_CLK_ENABLE();

    __HAL_RCC_I2C2_CLK_ENABLE();

    __HAL_RCC_DCMI_CLK_ENABLE();

    __HAL_RCC_DMA2_CLK_ENABLE();

    /* PA8: постоянное аппаратное тактирование камеры, не программное GPIO. */
    configure_gpio_pins(GPIOA, GPIO_PIN_8, GPIO_AF0_MCO, GPIO_MODE_AF_PP);

    HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI, RCC_MCODIV_4); /* 16 MHz */

    /* Отдельная SCCB шина; существующая I2C1 PB6/PB9 остаётся свободной для базы. */
    configure_gpio_pins(GPIOB, GPIO_PIN_10 | GPIO_PIN_11, GPIO_AF4_I2C2, GPIO_MODE_AF_OD);

    configure_gpio_pins(GPIOD, GPIO_PIN_4 | GPIO_PIN_5, 0, GPIO_MODE_OUTPUT_PP);

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);

    HAL_Delay(5);

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET);

    HAL_Delay(10);

    sccb.Instance = I2C2;

    sccb.Init.Timing = 0x70421F2F;

    sccb.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;

    if (HAL_I2C_Init(&sccb) != HAL_OK)
        return 0;

    if (HAL_I2CEx_ConfigAnalogFilter(&sccb, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
        return 0;

    /* DVP входы: синхронизация и Y2..Y9 камеры как DCMI_D0..D7. */
    configure_gpio_pins(GPIOA, GPIO_PIN_4 | GPIO_PIN_6, GPIO_AF13_DCMI, GPIO_MODE_AF_PP);

    configure_gpio_pins(GPIOB, GPIO_PIN_7, GPIO_AF13_DCMI, GPIO_MODE_AF_PP);

    configure_gpio_pins(GPIOC, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9, GPIO_AF13_DCMI,
                        GPIO_MODE_AF_PP);

    configure_gpio_pins(GPIOD, GPIO_PIN_3, GPIO_AF13_DCMI, GPIO_MODE_AF_PP);

    configure_gpio_pins(GPIOE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_AF13_DCMI,
                        GPIO_MODE_AF_PP);

    /* DCMI_DR -> AXI SRAM; длина HAL указана в 32-битных словах. */
    dma.Instance = DMA2_Stream0;

    dma.Init.Request = DMA_REQUEST_DCMI;

    dma.Init.Direction = DMA_PERIPH_TO_MEMORY;

    dma.Init.PeriphInc = DMA_PINC_DISABLE;

    dma.Init.MemInc = DMA_MINC_ENABLE;

    dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;

    dma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;

    dma.Init.Mode = DMA_NORMAL;

    dma.Init.Priority = DMA_PRIORITY_HIGH;

    dma.Init.FIFOMode = DMA_FIFOMODE_ENABLE;

    dma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;

    dma.Init.MemBurst = DMA_MBURST_SINGLE;

    dma.Init.PeriphBurst = DMA_PBURST_SINGLE;

    if (HAL_DMA_Init(&dma) != HAL_OK)
        return 0;

    dcmi.Instance = DCMI;

    dcmi.Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;

    dcmi.Init.PCKPolarity = DCMI_PCKPOLARITY_RISING;

    /* OV2640 DSP output on this module has active-low VSYNC.
     * Verified by real captures: HIGH yielded no DMA words, LOW yields RGB565 frames. */
    dcmi.Init.VSPolarity = DCMI_VSPOLARITY_LOW;

    dcmi.Init.HSPolarity = DCMI_HSPOLARITY_LOW;

    dcmi.Init.CaptureRate = DCMI_CR_ALL_FRAME;

    dcmi.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;

    dcmi.Init.JPEGMode = DCMI_JPEG_DISABLE;

    dcmi.Init.ByteSelectMode = DCMI_BSM_ALL;

    dcmi.Init.LineSelectMode = DCMI_LSM_ALL;

    __HAL_LINKDMA(&dcmi, DMA_Handle, dma);

    if (HAL_DCMI_Init(&dcmi) != HAL_OK)
        return 0;

    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 6, 0);

    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    HAL_NVIC_SetPriority(DCMI_IRQn, 6, 0);

    HAL_NVIC_EnableIRQ(DCMI_IRQn);

    return 1;
}

/**
 * @brief Передаёт IRQ DMA2_Stream0 обработчику STM32 HAL.
 *
 * Контекст: прерывание. UART, задержки и lwIP здесь не используются.
 */
void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&dma);
}

/**
 * @brief Передаёт IRQ кадров/ошибок DCMI обработчику STM32 HAL.
 *
 * Контекст: прерывание; HAL вызывает короткие callbacks ниже.
 */
void DCMI_IRQHandler(void)
{
    HAL_DCMI_IRQHandler(&dcmi);
}

/**
 * @brief Помечает аппаратный конец кадра для обработки в lwipProc.
 *
 * @param h Handle периферии, вызвавшей callback.
 * Никакой копии изображения в IRQ нет; объём DMA проверяется в poll.
 */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *h)
{
    if (h == &dcmi)
        completed = 1;
}

/**
 * @brief Помечает отказ DCMI/DMA для завершения захвата в задаче.
 *
 * @param h Handle периферии, вызвавшей callback.
 * Контекст: IRQ. Подробности ошибок сохраняются позже, до HAL stop.
 */
void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *h)
{
    if (h == &dcmi)
        failed = 1;
}

/**
 * @brief Обслуживает один UDP-запрос CAM1: захват либо часть готового кадра.
 *
 * @param arg Не используется; userdata lwIP.
 * @param pcb Серверный UDP PCB.
 * @param p Входной pbuf; функция освобождает его во всех ветках.
 * @param addr IP отправителя; копируется при назначении владельца.
 * @param port UDP-порт отправителя.
 * Контекст: callback raw lwIP внутри lwipProc, не IRQ и не другая задача.
 */
static void camera_udp_receive(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                               const ip_addr_t *addr, u16_t port)
{
    (void)arg;

    uint8_t req[12], reply[24 + CHUNK];

    /* Входной pbuf принадлежит callback: освобождение обязательно в каждой ветке. */
    if (p->tot_len != 12)
    {
        pbuf_free(p);

        return;
    }

    pbuf_copy_partial(p, req, 12, 0);

    pbuf_free(p);

    if (memcmp(req, "CAM1", 4))
        return;

    ++diagnostic.requests;

    uint32_t id = read_be32(req + 4), off = read_be32(req + 8), status = 0, n = 0;

    /* Один буфер удерживается за парой IP/порт и frame_id; повтор не перезапускает DMA. */
    unsigned same = owner_set && ip_addr_cmp(&owner, addr) && owner_port == port;

    if (camera_init_error)
        status = 2;
    else if (owner_set && (!same || id != frame_id) && HAL_GetTick() - touched < 10000U)
    {
        /* The owner may start its next frame after reading the final chunk. */
        status = 1;
    }
    else if (!owner_set || !same || id != frame_id)
    {
        if (off != 0)
            status = 3;
        else
        {
            HAL_DCMI_Stop(&dcmi);

            /* HAL snapshot completion disables event/error interrupts; start fresh
             * for every requested frame and discard flags left by a timeout. */
            __HAL_DCMI_DISABLE_IT(&dcmi, DCMI_IT_FRAME);

            __HAL_DCMI_CLEAR_FLAG(&dcmi, DCMI_FLAG_FRAMERI | DCMI_FLAG_OVRRI | DCMI_FLAG_ERRRI |
                                             DCMI_FLAG_VSYNCRI | DCMI_FLAG_LINERI);

            __HAL_DCMI_ENABLE_IT(&dcmi, DCMI_IT_FRAME | DCMI_IT_LINE | DCMI_IT_VSYNC | DCMI_IT_ERR |
                                            DCMI_IT_OVR);

            dcmi.ErrorCode = HAL_DCMI_ERROR_NONE;

            owner = *addr;

            owner_port = port;

            owner_set = 1;

            frame_id = id;

            valid = 0;

            completed = failed = 0;

            started = touched = HAL_GetTick();

            capturing = 1;

            if (SCB->CCR & SCB_CCR_DC_Msk)
                SCB_CleanInvalidateDCache_by_Addr((uint32_t *)frame, DMA_BUFFER_BYTES);

            if (HAL_DCMI_Start_DMA(&dcmi, DCMI_MODE_SNAPSHOT, (uint32_t)frame, DMA_WORDS) != HAL_OK)
            {
                capture_result(1);

                capturing = 0;

                failed = 1;

                ++camera_capture_errors;
            }

            status = 1;
        }
    }

    if (!status)
    {
        touched = HAL_GetTick();

        if (failed)
            status = 2;
        else if (!valid)
            status = 1;
        else if (off >= FRAME_BYTES || off % CHUNK)
            status = 3;
        else
        {
            n = FRAME_BYTES - off;

            if (n > CHUNK)
                n = CHUNK;
        }
    }

    /* Заголовок всегда 24 байта; защитный хвост DMA в сеть не отправляется. */
    memcpy(reply, "CAM1", 4);

    write_be32(reply + 4, id);

    write_be32(reply + 8, off);

    write_be32(reply + 12, FRAME_BYTES);

    reply[16] = WIDTH >> 8;

    reply[17] = WIDTH & 255;

    reply[18] = HEIGHT >> 8;

    reply[19] = HEIGHT & 255;

    write_be32(reply + 20, status);

    if (n)
        memcpy(reply + 24, frame + off, n);

    struct pbuf *out = pbuf_alloc(PBUF_TRANSPORT, 24 + n, PBUF_RAM);

    if (out)
    {
        pbuf_take(out, reply, 24 + n);

        if (udp_sendto(pcb, out, addr, port) == ERR_OK)
            ++diagnostic.replies;
        else
            ++diagnostic.send_errors;

        pbuf_free(out);
    }
    else
        ++diagnostic.send_errors;

    /* Retain frame for retransmission, but allow a new ID from this owner. */
    if (n && off + n == FRAME_BYTES)
        touched = HAL_GetTick() - 10000U;
}

/**
 * @brief Инициализирует камеру и привязывает UDP-сервер к порту 5000.
 *
 * Вызывается один раз после MX_LWIP_Init из lwipProc.
 * При отказе камеры сервер остаётся доступным и сообщает статус ошибки.
 */
void camera_stream_init(void)
{
    camera_init_error = 1;

    if (camera_hardware_init())
    {
        uint8_t hi = 0, lo = 0;

        camera_init_error = 2;

        int identified = camera_write_register(BANK_SEL, BANK_SENSOR) &&
                         camera_read_register(0x0a, &hi) && camera_read_register(0x0b, &lo);

        diagnostic.pid = ((uint32_t)hi << 8) | lo;

        if (identified && hi == 0x26 && lo == 0x42)
        {
            camera_init_error = 3;

            if (camera_write_register(COM7, COM7_SRST))
            {
                HAL_Delay(10);

                /* Размеры DSP задаются в единицах четырёх пикселей: 320/4, 240/4. */
                static const uint8_t window[][2] = {{BANK_SEL, BANK_DSP},
                                                    {R_BYPASS, 1},
                                                    {HSIZE, 200},
                                                    {VSIZE, 150},
                                                    {XOFFL, 0},
                                                    {YOFFL, 0},
                                                    {VHYX, 0},
                                                    {TEST, 0},
                                                    {ZMOW, 80},
                                                    {ZMOH, 60},
                                                    {ZMHH, 0},
                                                    {BANK_SEL, BANK_SENSOR},
                                                    {CLKRC, 7},
                                                    {COM10, 0},
                                                    {BANK_SEL, BANK_DSP},
                                                    {R_DVP_SP, 8},
                                                    {R_BYPASS, 0},
                                                    {0, 0}};

                if (camera_write_register_table(ov2640_settings_cif) &&
                    camera_write_register_table(ov2640_settings_to_svga) &&
                    camera_write_register_table(window) &&
                    camera_write_register_table(ov2640_settings_rgb565))
                {
                    HAL_Delay(300);

                    camera_init_error = 0;
                }
            }
        }
    }

    server = udp_new();

    if (!server || udp_bind(server, IP_ADDR_ANY, PORT) != ERR_OK)
        Error_Handler();

    udp_recv(server, camera_udp_receive, NULL);

    init_done = 1;

    publish_diagnostics();
}

/**
 * @brief Обновляет диагностику и завершает асинхронный snapshot.
 *
 * Вызывается в каждом проходе lwipProc. FRAME не означает заполнение
 * защитного хвоста: проверяется только полезный payload 38400 слов.
 */
void camera_stream_poll(void)
{
    if (HAL_GetTick() - snapshot_tick >= 25U)
    {
        snapshot_tick = HAL_GetTick();

        publish_diagnostics();
    }

    if (!capturing)
        return;

    if (failed || HAL_GetTick() - started > 3000U)
    {
        capture_result(failed ? 2U : 3U);

        HAL_DCMI_Stop(&dcmi);

        capturing = 0;

        failed = 1;

        ++camera_capture_errors;

        publish_diagnostics();
    }
    else if (completed)
    {
        /* OV2640 emits trailing DVP bytes after its 320x240 payload. The guard
         * prevents overflow while awaiting actual frame end. FRAME IRQ is armed
         * at start, since normal DMA need not fill the entire guard capacity. */
        uint32_t remaining = __HAL_DMA_GET_COUNTER(&dma);

        unsigned enough = (DMA_WORDS - remaining) >= FRAME_BYTES / 4U;

        capture_result(enough ? 4U : 5U);

        HAL_DCMI_Stop(&dcmi);

        if (SCB->CCR & SCB_CCR_DC_Msk)
            SCB_InvalidateDCache_by_Addr((uint32_t *)frame, DMA_BUFFER_BYTES);

        capturing = 0;

        if (enough)
        {
            valid = 1;

            ++camera_frames;
        }
        else
        {
            failed = 1;

            ++camera_capture_errors;
        }

        publish_diagnostics();
    }
}
