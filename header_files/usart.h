#ifndef USART_H
#define USART_H

#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void platform_usart_cdc_init(void);

// Enqueue string vector for transmission, returns false if busy
extern bool platform_usart_cdc_send_async(
    const struct platform_ro_buf_desc *vec, unsigned int nr
);

#define PLATFORM_USART_NR_TX_VEC_MAX (16)

extern void platform_usart_cdc_send_abort(void);
extern bool platform_usart_cdc_tx_busy(void);

// Start async reception, returns false if busy
extern bool platform_usart_cdc_recv_async(char *buf, unsigned int len);

extern void platform_usart_cdc_recv_abort(void);
extern bool platform_usart_cdc_rx_busy(void);

struct platform_usart_recv_data_type {
    union {
        const char *buf;
        char *w_buf;
    };
    unsigned int len;
    int err_parity:1;
    int err_framing:1;
    int err_overflow:1;
    int err_collision:1;
    int err_break:1;
    int _err_pad:27;
};

// Check and consume completed receive transaction
extern bool platform_usart_cdc_recv_get(struct platform_usart_recv_data_type *desc);

typedef struct platform_usart_rx_desc_type {
    char *buf;
    uint16_t max_len;
    volatile uint16_t compl_type;
    
#define PLATFORM_USART_RX_COMPL_NONE  0x0000
#define PLATFORM_USART_RX_COMPL_DATA  0x0001
#define PLATFORM_USART_RX_COMPL_BREAK 0x0002

    volatile union {
        uint16_t data_len;
    } compl_info;
} platform_usart_rx_async_desc_t;

typedef struct platform_usart_tx_desc_type {
    const char *buf;
    uint16_t len;
} platform_usart_tx_bufdesc_t;

extern bool platform_usart_cdc_tx_async(const platform_usart_tx_bufdesc_t *desc, unsigned int nr_desc);
extern bool platform_usart_cdc_rx_async(platform_usart_rx_async_desc_t *desc);
extern void platform_usart_tick_handler(const platform_timespec_t *tick);

#ifdef __cplusplus
}
#endif
#endif
