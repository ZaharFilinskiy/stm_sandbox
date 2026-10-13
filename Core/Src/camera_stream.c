#include "main.h"
#include "camera_stream.h"
#include "ov2640_settings.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include <string.h>

#define WIDTH 320U
#define HEIGHT 240U
#define FRAME_BYTES (WIDTH * HEIGHT * 2U)
#define CHUNK 1200U
#define PORT 5000U
static uint8_t frame[FRAME_BYTES] __attribute__((aligned(32)));
static DCMI_HandleTypeDef dcmi;
static DMA_HandleTypeDef dma;
static I2C_HandleTypeDef sccb;
static struct udp_pcb *server;
static volatile unsigned completed, failed;
/* Inspect these in debugger: 0=ready, 1=hardware init, 2=ID, 3=register writes. */
volatile uint32_t camera_init_error, camera_frames, camera_capture_errors;
static unsigned capturing, valid, owner_set;
static uint32_t frame_id, started, touched;
static ip_addr_t owner;
static u16_t owner_port;
static uint32_t get32(const uint8_t *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static void put32(uint8_t *p, uint32_t v) {
    p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v;
}
static int wr(uint8_t reg, uint8_t value) {
    uint8_t b[2]={reg,value};
    return HAL_I2C_Master_Transmit(&sccb,0x60,b,2,30)==HAL_OK;
}
static int rd(uint8_t reg, uint8_t *value) {
    /* SCCB reads use a STOP between subaddress write and data read. */
    return HAL_I2C_Master_Transmit(&sccb,0x60,&reg,1,30)==HAL_OK &&
           HAL_I2C_Master_Receive(&sccb,0x60,value,1,30)==HAL_OK;
}
static int table(const uint8_t t[][2]) {
    for (unsigned i=0;t[i][0] || t[i][1];++i)
        if (!wr(t[i][0],t[i][1])) return 0;
    return 1;
}
static void pins(GPIO_TypeDef *port, uint32_t mask, uint32_t af, uint32_t mode) {
    GPIO_InitTypeDef g={0}; g.Pin=mask; g.Mode=mode; g.Pull=GPIO_NOPULL;
    g.Speed=GPIO_SPEED_FREQ_VERY_HIGH; g.Alternate=af; HAL_GPIO_Init(port,&g);
}
static int hardware(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE(); __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_I2C2_CLK_ENABLE(); __HAL_RCC_DCMI_CLK_ENABLE(); __HAL_RCC_DMA2_CLK_ENABLE();
    pins(GPIOA,GPIO_PIN_8,GPIO_AF0_MCO,GPIO_MODE_AF_PP);
    HAL_RCC_MCOConfig(RCC_MCO1,RCC_MCO1SOURCE_HSI,RCC_MCODIV_4); /* 16 MHz */
    pins(GPIOB,GPIO_PIN_10|GPIO_PIN_11,GPIO_AF4_I2C2,GPIO_MODE_AF_OD);
    pins(GPIOD,GPIO_PIN_4|GPIO_PIN_5,0,GPIO_MODE_OUTPUT_PP);
    HAL_GPIO_WritePin(GPIOD,GPIO_PIN_4|GPIO_PIN_5,GPIO_PIN_RESET);
    HAL_Delay(5); HAL_GPIO_WritePin(GPIOD,GPIO_PIN_4,GPIO_PIN_SET); HAL_Delay(10);
    sccb.Instance=I2C2; sccb.Init.Timing=0x70421F2F;
    sccb.Init.AddressingMode=I2C_ADDRESSINGMODE_7BIT;
    if (HAL_I2C_Init(&sccb)!=HAL_OK) return 0;
    if (HAL_I2CEx_ConfigAnalogFilter(&sccb,I2C_ANALOGFILTER_ENABLE)!=HAL_OK) return 0;
    pins(GPIOA,GPIO_PIN_4|GPIO_PIN_6,GPIO_AF13_DCMI,GPIO_MODE_AF_PP);
    pins(GPIOB,GPIO_PIN_7,GPIO_AF13_DCMI,GPIO_MODE_AF_PP);
    pins(GPIOC,GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9,GPIO_AF13_DCMI,GPIO_MODE_AF_PP);
    pins(GPIOD,GPIO_PIN_3,GPIO_AF13_DCMI,GPIO_MODE_AF_PP);
    pins(GPIOE,GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6,GPIO_AF13_DCMI,GPIO_MODE_AF_PP);
    dma.Instance=DMA2_Stream0; dma.Init.Request=DMA_REQUEST_DCMI;
    dma.Init.Direction=DMA_PERIPH_TO_MEMORY; dma.Init.PeriphInc=DMA_PINC_DISABLE;
    dma.Init.MemInc=DMA_MINC_ENABLE; dma.Init.PeriphDataAlignment=DMA_PDATAALIGN_WORD;
    dma.Init.MemDataAlignment=DMA_MDATAALIGN_WORD; dma.Init.Mode=DMA_NORMAL;
    dma.Init.Priority=DMA_PRIORITY_HIGH; dma.Init.FIFOMode=DMA_FIFOMODE_ENABLE;
    dma.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
    dma.Init.MemBurst=DMA_MBURST_SINGLE; dma.Init.PeriphBurst=DMA_PBURST_SINGLE;
    if(HAL_DMA_Init(&dma)!=HAL_OK) return 0;
    dcmi.Instance=DCMI; dcmi.Init.SynchroMode=DCMI_SYNCHRO_HARDWARE;
    dcmi.Init.PCKPolarity=DCMI_PCKPOLARITY_RISING;
    dcmi.Init.VSPolarity=DCMI_VSPOLARITY_HIGH; dcmi.Init.HSPolarity=DCMI_HSPOLARITY_LOW;
    dcmi.Init.CaptureRate=DCMI_CR_ALL_FRAME; dcmi.Init.ExtendedDataMode=DCMI_EXTEND_DATA_8B;
    dcmi.Init.JPEGMode=DCMI_JPEG_DISABLE; dcmi.Init.ByteSelectMode=DCMI_BSM_ALL;
    dcmi.Init.LineSelectMode=DCMI_LSM_ALL;
    __HAL_LINKDMA(&dcmi,DMA_Handle,dma);
    if(HAL_DCMI_Init(&dcmi)!=HAL_OK) return 0;
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn,6,0); HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    HAL_NVIC_SetPriority(DCMI_IRQn,6,0); HAL_NVIC_EnableIRQ(DCMI_IRQn);
    return 1;
}
void DMA2_Stream0_IRQHandler(void) { HAL_DMA_IRQHandler(&dma); }
void DCMI_IRQHandler(void) { HAL_DCMI_IRQHandler(&dcmi); }
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *h) { if(h==&dcmi) completed=1; }
void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *h) { if(h==&dcmi) failed=1; }

static void receive(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                    const ip_addr_t *addr,u16_t port) {
    (void)arg;
    uint8_t req[12], reply[24+CHUNK];
    if(p->tot_len!=12) { pbuf_free(p); return; }
    pbuf_copy_partial(p,req,12,0); pbuf_free(p);
    if(memcmp(req,"CAM1",4)) return;
    uint32_t id=get32(req+4), off=get32(req+8), status=0, n=0;
    unsigned same=owner_set && ip_addr_cmp(&owner,addr) && owner_port==port;
    if(camera_init_error) status=2;
    else if(owner_set && (!same || id!=frame_id) && HAL_GetTick()-touched<10000U) {
        /* The owner may start its next frame after reading the final chunk. */
        status=1;
    } else if(!owner_set || !same || id!=frame_id) {
        if(off!=0) status=3;
        else {
            HAL_DCMI_Stop(&dcmi);
            owner=*addr; owner_port=port; owner_set=1; frame_id=id;
            valid=0; completed=failed=0; started=touched=HAL_GetTick(); capturing=1;
            if(SCB->CCR & SCB_CCR_DC_Msk) SCB_CleanInvalidateDCache_by_Addr((uint32_t *)frame,FRAME_BYTES);
            if(HAL_DCMI_Start_DMA(&dcmi,DCMI_MODE_SNAPSHOT,(uint32_t)frame,FRAME_BYTES/4)!=HAL_OK) {
                capturing=0; failed=1; ++camera_capture_errors;
            }
            status=1;
        }
    }
    if(!status) {
        touched=HAL_GetTick();
        if(failed) status=2;
        else if(!valid) status=1;
        else if(off>=FRAME_BYTES || off%CHUNK) status=3;
        else { n=FRAME_BYTES-off; if(n>CHUNK)n=CHUNK; }
    }
    memcpy(reply,"CAM1",4); put32(reply+4,id); put32(reply+8,off);
    put32(reply+12,FRAME_BYTES); reply[16]=WIDTH>>8; reply[17]=WIDTH&255;
    reply[18]=HEIGHT>>8; reply[19]=HEIGHT&255; put32(reply+20,status);
    if(n) memcpy(reply+24,frame+off,n);
    struct pbuf *out=pbuf_alloc(PBUF_TRANSPORT,24+n,PBUF_RAM);
    if(out) { pbuf_take(out,reply,24+n); udp_sendto(pcb,out,addr,port); pbuf_free(out); }
    /* Retain frame for retransmission, but allow a new ID from this owner. */
    if(n && off+n==FRAME_BYTES) touched=HAL_GetTick()-10000U;
}
void camera_stream_init(void) {
    camera_init_error=1;
    if(hardware()) {
        uint8_t hi=0,lo=0; camera_init_error=2;
        if(wr(BANK_SEL,BANK_SENSOR) && rd(0x0a,&hi) && rd(0x0b,&lo) && hi==0x26 && lo==0x42) {
            camera_init_error=3;
            if(wr(COM7,COM7_SRST)) {
                HAL_Delay(10);
                static const uint8_t window[][2]={
                    {BANK_SEL,BANK_DSP},{R_BYPASS,1},
                    {HSIZE,200},{VSIZE,150},{XOFFL,0},{YOFFL,0},{VHYX,0},{TEST,0},
                    {ZMOW,80},{ZMOH,60},{ZMHH,0},
                    {BANK_SEL,BANK_SENSOR},{CLKRC,7},{COM10,0},
                    {BANK_SEL,BANK_DSP},{R_DVP_SP,8},{R_BYPASS,0},{0,0}};
                if(table(ov2640_settings_cif) && table(ov2640_settings_to_svga) &&
                   table(window) && table(ov2640_settings_rgb565)) {
                    HAL_Delay(300); camera_init_error=0;
                }
            }
        }
    }
    server=udp_new();
    if(!server || udp_bind(server,IP_ADDR_ANY,PORT)!=ERR_OK) Error_Handler();
    udp_recv(server,receive,NULL);
}
void camera_stream_poll(void) {
    if(!capturing) return;
    if(failed || HAL_GetTick()-started>3000U) {
        HAL_DCMI_Stop(&dcmi); capturing=0; failed=1; ++camera_capture_errors;
    } else if(completed) {
        /* HAL enables FRAME IRQ after DMA has transferred all 38400 words. */
        HAL_DCMI_Stop(&dcmi);
        if(SCB->CCR & SCB_CCR_DC_Msk) SCB_InvalidateDCache_by_Addr((uint32_t *)frame,FRAME_BYTES);
        valid=1; capturing=0; ++camera_frames;
    }
}
