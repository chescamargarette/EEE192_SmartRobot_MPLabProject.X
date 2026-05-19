

#include "platform.h"
#include "usart.h"
#include <xc.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

// USART context structure - shared between IRQ handler and application
struct platform_usart_ctx_type {
    volatile const struct platform_ro_buf_desc volatile *txd_next_desc;
    volatile struct platform_ro_buf_desc txd_real_desc[16];
    volatile unsigned int txd_nr_desc;
    volatile struct platform_ro_buf_desc txd_cwd;
    
    volatile unsigned int rx_tick_ctr;  // Half character periods since last RX
    struct platform_usart_recv_data_type rx_apidata;
    volatile unsigned int rx_len_max;
    
    volatile uint8_t tx_state;  // 0=IDLE, 1=ENABLING/DISABLING, 2=ACTIVE
    volatile uint8_t rx_state;  // 0=IDLE, 1=ENABLING/DISABLING, 2=ACTIVE, 3=DONE
    platform_usart_rx_async_desc_t *rx_async_desc;
};

#define SERCOM_CDC_REGS (&(SERCOM3_REGS->USART_INT))
static struct platform_usart_ctx_type ctx_usart_cdc;
#define SERCOM_CDC_CTX (&ctx_usart_cdc)

void platform_usart_cdc_init(void)
{
    // Enable GCLK generator (GEN2 at 4MHz)
    GCLK_REGS->GCLK_PCHCTRL[20] = 0x00000042;
    while ((GCLK_REGS->GCLK_PCHCTRL[20] & 0x00000040) == 0)
        asm("nop");
    
    memset(SERCOM_CDC_CTX, 0, sizeof(*SERCOM_CDC_CTX));
    SERCOM_CDC_REGS->SERCOM_CTRLA = 0x00000001;
    SERCOM_CDC_CTX->rx_async_desc = NULL;
    while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000001) != 0)
        asm("nop");
    
    SERCOM_CDC_REGS->SERCOM_CTRLA = (0x1 << 2);  // USART mode
    SERCOM_CDC_REGS->SERCOM_CTRLA |= (0x0 << 13) | (0x1 << 30) | (0x1 << 24) | (0x0 << 16) | (0x1 << 20);
    
    // LSB first, no parity, 1 stop bit, 8-bit chars, PAD0/PAD1 for TX/RX
    SERCOM_CDC_REGS->SERCOM_CTRLB = (0x0 << 6) | (0x0 << 0) | (0x0 << 13);
    
    // BAUD = 65536 * (1 - (16 * 57600 / 4MHz)) = 50437 (0xC505)
    SERCOM_CDC_REGS->SERCOM_BAUD = 0xC505;
    
    // Enable RX/TX, clear FIFOs
    SERCOM_CDC_REGS->SERCOM_CTRLB |= (0x1 << 17) | (0x1 << 16) | (0x3 << 22);
    while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000004) != 0)
        asm("nop");
    
    // Configure pins PB08 (TX) and PB09 (RX)
    PORT_SEC_REGS->GROUP[1].PORT_DIRCLR = (1 << 8) | (1 << 9);
    PORT_SEC_REGS->GROUP[1].PORT_PINCFG[9] = 0x03;
    PORT_SEC_REGS->GROUP[1].PORT_PINCFG[8] = 0x03;
    PORT_SEC_REGS->GROUP[1].PORT_PMUX[4] = 0x33;
    
    // Enable peripheral
    SERCOM_CDC_REGS->SERCOM_CTRLA |= 0x00000002;
    while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000003) != 0)
        asm("nop");
}

// TX interrupt handler (DRE - Data Register Empty)
void __attribute__((used, interrupt)) SERCOM3_0_Handler(void)
{
    if (SERCOM_CDC_CTX->tx_state != 2)
        return;
    
    while ((SERCOM_CDC_REGS->SERCOM_INTFLAG & 0x01) != 0) {
        while (SERCOM_CDC_CTX->txd_cwd.len == 0) {
            if (SERCOM_CDC_CTX->txd_nr_desc == 0)
                break;
            SERCOM_CDC_CTX->txd_cwd = *(SERCOM_CDC_CTX->txd_next_desc++);
            --(SERCOM_CDC_CTX->txd_nr_desc);
        }
        
        if (SERCOM_CDC_CTX->txd_cwd.len == 0) {
            SERCOM_CDC_REGS->SERCOM_INTENCLR = 0x01;
            SERCOM_CDC_CTX->tx_state = 0;
            return;
        }
        
        SERCOM_CDC_REGS->SERCOM_DATA = *(SERCOM_CDC_CTX->txd_cwd.buf++);
        --SERCOM_CDC_CTX->txd_cwd.len;
    }
}

// RX interrupt handler (RXC - Receive Complete)
void __attribute__((used, interrupt)) SERCOM3_2_Handler(void)
{
    uint16_t sc;
    uint32_t dt;
    
    if (SERCOM_CDC_CTX->rx_state != 2)
        return;
    
    while ((SERCOM_CDC_REGS->SERCOM_INTFLAG & 0x04) != 0) {
        sc = SERCOM_CDC_REGS->SERCOM_STATUS;  // Read STATUS before DATA
        asm("nop");
        dt = SERCOM_CDC_REGS->SERCOM_DATA;
        
        if (SERCOM_CDC_CTX->rx_apidata.len >= SERCOM_CDC_CTX->rx_len_max) {
            SERCOM_CDC_CTX->rx_apidata.err_overflow = 1;
            continue;
        }
        
        // Capture error conditions
        if ((sc & 0x0001) != 0) SERCOM_CDC_CTX->rx_apidata.err_parity = 1;
        if ((sc & 0x0002) != 0) {
            SERCOM_CDC_CTX->rx_apidata.err_framing = 1;
            if (dt == 0) SERCOM_CDC_CTX->rx_apidata.err_break = 1;
        }
        if ((sc & 0x0004) != 0) SERCOM_CDC_CTX->rx_apidata.err_overflow = 1;
        if ((sc & 0x0020) != 0) SERCOM_CDC_CTX->rx_apidata.err_collision = 1;
        
        SERCOM_CDC_CTX->rx_apidata.w_buf[SERCOM_CDC_CTX->rx_apidata.len++] = (char)(dt & 0x000000FF);
        SERCOM_CDC_CTX->rx_tick_ctr = 0;
        
        if (SERCOM_CDC_CTX->rx_async_desc != NULL)
            SERCOM_CDC_CTX->rx_async_desc->compl_info.data_len = SERCOM_CDC_CTX->rx_apidata.len;
        
        if (SERCOM_CDC_CTX->rx_apidata.len >= SERCOM_CDC_CTX->rx_len_max) {
            SERCOM_CDC_REGS->SERCOM_INTENCLR = 0x04;
            SERCOM_CDC_CTX->rx_state = 3;
            if (SERCOM_CDC_CTX->rx_async_desc != NULL)
                SERCOM_CDC_CTX->rx_async_desc->compl_type = PLATFORM_USART_RX_COMPL_DATA;
            return;
        }
    }
}

// SysTick handler for inter-character timeout (3.5 character periods = 7 ticks)
void platform_internal_usart_cdc_systick_handler(void)
{
    if (SERCOM_CDC_CTX->rx_state != 2 || SERCOM_CDC_CTX->rx_apidata.len == 0)
        return;
    
    SERCOM_CDC_CTX->rx_tick_ctr += 1;
    if (SERCOM_CDC_CTX->rx_tick_ctr >= 7) {
        SERCOM_CDC_REGS->SERCOM_INTENCLR = 0x04;
        SERCOM_CDC_CTX->rx_state = 3;
        if (SERCOM_CDC_CTX->rx_async_desc != NULL)
            SERCOM_CDC_CTX->rx_async_desc->compl_type = PLATFORM_USART_RX_COMPL_DATA;
    }
}

void platform_usart_tick_handler(const platform_timespec_t *tick)
{
    platform_internal_usart_cdc_systick_handler();
}

bool platform_usart_cdc_send_async(const struct platform_ro_buf_desc *vec, unsigned int nr)
{
    unsigned int a;
    const struct platform_ro_buf_desc *b;
    
    if (!vec || nr == 0) return true;
    if (SERCOM_CDC_CTX->tx_state != 0) return false;
    
    SERCOM_CDC_CTX->tx_state = 1;
    
    // Clear FIFO
    SERCOM_CDC_REGS->SERCOM_CTRLB |= (1 << 22);
    while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000004) != 0)
        asm("nop");
    
    SERCOM_CDC_CTX->txd_cwd.buf = NULL;
    SERCOM_CDC_CTX->txd_cwd.len = 0;
    SERCOM_CDC_CTX->txd_next_desc = &SERCOM_CDC_CTX->txd_real_desc[0];
    
    for (a = 0, SERCOM_CDC_CTX->txd_nr_desc = 0;
         SERCOM_CDC_CTX->txd_nr_desc < 16 && a < nr; ++a) {
        b = &vec[a];
        if (b->len == 0 || b->buf == NULL) continue;
        SERCOM_CDC_CTX->txd_real_desc[SERCOM_CDC_CTX->txd_nr_desc++] = *b;
    }
    
    SERCOM_CDC_CTX->tx_state = 2;
    SERCOM_CDC_REGS->SERCOM_INTENSET = 0x01;  // Enable DRE interrupt
    return true;
}

void platform_usart_cdc_send_abort(void)
{
    SERCOM_CDC_REGS->SERCOM_INTENCLR = 0x01;
    SERCOM_CDC_CTX->tx_state = 1;
    
    SERCOM_CDC_REGS->SERCOM_CTRLB |= (1 << 22);  // Clear FIFO
    while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000004) != 0)
        asm("nop");
    
    while ((SERCOM_CDC_REGS->SERCOM_INTFLAG & 0x02) == 0)  // Wait for TX complete
        asm("nop");
    
    SERCOM_CDC_CTX->txd_cwd.buf = NULL;
    SERCOM_CDC_CTX->txd_cwd.len = 0;
    SERCOM_CDC_CTX->txd_next_desc = 0;
    SERCOM_CDC_CTX->txd_nr_desc = 0;
    SERCOM_CDC_CTX->tx_state = 0;
}

bool platform_usart_cdc_tx_busy(void)
{
    return (SERCOM_CDC_CTX->tx_state != 0);
}

bool platform_usart_cdc_recv_async(char *buf, unsigned int len)
{
    if (!buf || len == 0) return true;
    if (SERCOM_CDC_CTX->rx_state != 0) return false;
    
    SERCOM_CDC_CTX->rx_state = 1;
    
    memset(&SERCOM_CDC_CTX->rx_apidata, 0, sizeof(SERCOM_CDC_CTX->rx_apidata));
    SERCOM_CDC_CTX->rx_apidata.w_buf = buf;
    SERCOM_CDC_CTX->rx_len_max = len;
    
    SERCOM_CDC_REGS->SERCOM_CTRLB |= (1 << 23);  // Clear FIFO
    while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000004) != 0)
        asm("nop");
    
    SERCOM_CDC_CTX->rx_state = 2;
    SERCOM_CDC_REGS->SERCOM_INTENSET = 0x04;  // Enable RXC interrupt
    return true;
}

void platform_usart_cdc_recv_abort(void)
{
    SERCOM_CDC_REGS->SERCOM_INTENCLR = 0x04;
    SERCOM_CDC_CTX->rx_state = 1;
    
    SERCOM_CDC_REGS->SERCOM_CTRLB |= (1 << 23);  // Clear FIFO
    while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000004) != 0)
        asm("nop");
    
    memset(&SERCOM_CDC_CTX->rx_apidata, 0, sizeof(SERCOM_CDC_CTX->rx_apidata));
    SERCOM_CDC_CTX->rx_state = 0;
}

bool platform_usart_cdc_recv_get(struct platform_usart_recv_data_type *desc)
{
    if (!desc || SERCOM_CDC_CTX->rx_state != 3)
        return false;
    
    *desc = SERCOM_CDC_CTX->rx_apidata;
    memset(&SERCOM_CDC_CTX->rx_apidata, 0, sizeof(SERCOM_CDC_CTX->rx_apidata));
    SERCOM_CDC_CTX->rx_state = 0;
    return true;
}

bool platform_usart_cdc_rx_busy(void)
{
    return (SERCOM_CDC_CTX->rx_state != 0);
}

bool platform_usart_cdc_tx_async(const platform_usart_tx_bufdesc_t *desc, unsigned int nr_desc)
{
    struct platform_ro_buf_desc converted_desc[16];
    unsigned int valid_count = 0;
    
    if (!desc || nr_desc == 0) return true;
    
    for (unsigned int i = 0; i < nr_desc && i < 16; i++) {
        if (desc[i].buf && desc[i].len > 0) {
            converted_desc[valid_count].buf = desc[i].buf;
            converted_desc[valid_count].len = desc[i].len;
            valid_count++;
        }
    }
    
    if (valid_count == 0) return true;
    return platform_usart_cdc_send_async(converted_desc, valid_count);
}

bool platform_usart_cdc_rx_async(platform_usart_rx_async_desc_t *desc)
{
    if (!desc) return false;
    
    if (SERCOM_CDC_CTX->rx_state != 0)
        platform_usart_cdc_recv_abort();
    
    desc->compl_type = PLATFORM_USART_RX_COMPL_NONE;
    desc->compl_info.data_len = 0;
    SERCOM_CDC_CTX->rx_async_desc = desc;
    
    return platform_usart_cdc_recv_async(desc->buf, desc->max_len);
}
