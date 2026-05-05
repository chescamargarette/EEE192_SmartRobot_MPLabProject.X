/*
 * usart.c
 *
 * Initial version:  EEE 158 AY2025-26 Sem 1 Handlers
 * Modifications by: <
 * Franchesca Margarette Visitacion
 * 2022-35114
 * Section WUV
 * >
 *
 * USART functionality
 */

#include "include.h"

#include "../init.h"
#include "../usart.h"

#include <xc.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

// ==========================================================================

/*
 * State data for the USART
 *
 * NOTE: Because the members are shared between the IRQ handler and the application
 *       code, they are declared with the 'volatile' qualifier.
 */

struct platform_usart_ctx_type {
    
    /// Next descriptor to transmit
	volatile const struct platform_ro_buf_desc volatile *txd_next_desc;
	volatile struct platform_ro_buf_desc txd_real_desc[16];
    
    /// Number of descriptors that remain to be transmitted
	volatile unsigned int txd_nr_desc;
    
    /// Current working descriptor
	volatile struct platform_ro_buf_desc txd_cwd;
    
    // =================================================================

	/**
	 * Tick since last character was received
	 *
	 * One tick corresponds to half a character period.
	 */
	volatile unsigned int rx_tick_ctr;
    
    /// Exposed to the API
	struct platform_usart_recv_data_type rx_apidata;
    
    /// Maximum number of bytes available in `rx_apidata.buf`
	volatile unsigned int rx_len_max;
    
    // =================================================================
	
	/**
	 * Transmitter state ID
	 * 
	 * 0 = IDLE
	 * 1 = ENABLING / DISABLING
	 * 2 = ACTIVE
	 */
	volatile uint8_t tx_state;
    
    /**
	 * Receiver state ID
	 * 
	 * 0 = IDLE
	 * 1 = ENABLING / DISABLING
	 * 2 = ACTIVE
	 * 3 = DONE, data available
	 */
	volatile uint8_t rx_state;
    
	// Tracking active ASYNCH receiver descriptor
	platform_usart_rx_async_desc_t *rx_async_desc;
};

// ==========================================================================

/*
 * Initialize the USART
 *
 * Per the board schematic, the USART signals on the USB-CDC bridge (used to
 * communicate with your laptop) are connected as follows:
 * 	- PB08 (CDC_RX = PIC32_TX)
 * 	- PB09 (CDC_TX = PIC32_RX)
 * In turn, the function pinout (IC datasheet) indicates that these two pins
 * are on SERCOM3.
 *
 * Like the TC peripheral, SERCOM has differing views depending on operating
 * mode; classic UART corresponds to internally-clocked USART (`USART_INT`)
 * operating in async mode. CTRLA is shared across all modes, so it needs to
 * be set first after reset. For typing convenience, macros `SERCOM_CDC_REGS`
 * and 'SERCOM_CDC_CTX' have been defined here.
 */
#define SERCOM_CDC_REGS (&(SERCOM3_REGS->USART_INT))
static struct platform_usart_ctx_type ctx_usart_cdc;
#define SERCOM_CDC_CTX	(&ctx_usart_cdc)

void platform_usart_cdc_init(void)
{
    uint8_t sc;
    
    /*
	 * Enable the APB clock for this peripheral
	 *
	 * NOTE: The chip resets with it enabled; hence, commented-out.
	 *
	 * WARNING: Incorrect MCLK settings can cause system lockup that can
	 *          only be rectified via a hardware reset/power-cycle.
	 */
	// MCLK_REGS->MCLK_APBCMASK |= (1 << 4);

	/*
	 * Enable the GCLK generator for this peripheral
	 *
	 * NOTE: GEN2 (4 MHz) is used, as GEN0 (24 MHz) is too fast for
	 *       USART mode.
	 */
    GCLK_REGS->GCLK_PCHCTRL[20] = 0x00000042;
    while ((GCLK_REGS->GCLK_PCHCTRL[20] & 0x00000040) == 0)
        asm("nop");
    
    // Initialize both the peripheral and its associated context structure
    memset(SERCOM_CDC_CTX, 0, sizeof(*SERCOM_CDC_CTX));
    SERCOM_CDC_REGS->SERCOM_CTRLA = 0x00000001;
    // Initializing the new field.
    SERCOM_CDC_CTX->rx_async_desc = NULL; 
    while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000001) != 0) {
        asm("nop");
    }
    SERCOM_CDC_REGS->SERCOM_CTRLA = (0x1 << 2);     // USART mode first...
    SERCOM_CDC_REGS->SERCOM_CTRLA |= (0x0 << 13) | (0x1 << 30) | (0x1 << 24) | (0x0 << 16) | (0x1 << 20); // ... then async mode.
    
    /*
	 * Select further settings compatible with the 16550 UART:
	 *
	 * - LSB first
	 * - No parity
	 * - One stop bit
	 * - 8-bit character size
	 * - No break detection
	 * - With FIFO, 32-bit extensions disabled
	 *
	 * - Use PAD[0] for data transmission (only available setting for
	 *   TXD/PB08)
	 * - Use PAD[1] for data reception (must not conflict with TXD;
	 *   fixed for PB09)
	 * 
	 * NOTE: Do not enable the RXC and DRE interrupts during initialization,
	 *       as they will only clear when the buffer is empty and filled,
	 *       respectively.
	 */
    // SERCOM_CDC_REGS->SERCOM_CTRLA |= 0x00100000;
    SERCOM_CDC_REGS->SERCOM_CTRLB = (0x0 << 6) | (0x0 << 0) | (0x0 << 13);
    // SERCOM_CDC_REGS->SERCOM_CTRLC  = 0x08000000;
    
    /*
	 * In asynchronous USART mode (USART_EXT or USART_INT, CMODE=0), the baud rate
	 * for the bitstream is determined according to the following formula:
	 * (IC datasheet, § 33.6.2.3, async-arithmetic mode)
	 *
	 * f_{baud} = (f_{ref}/S)(1 - BAUD/65536)
	 *
	 * NOTE: For USART, one baud = one bit; thus, the bit period is also
	 *       the baud period.
	 *
	 * `S` is the number samples per bit; for best noise immunity, 16 is
	 * used. f_{ref} is the reference clock, and can be internally-
	 * generated (USART_INT) or externally-supplied (USART_EXT). In the
	 * former, f_{ref} is the same as the module's GCLK_SERCOM_CORE.
	 *
	 * More commonly, f_{baud} is specified according to the application,
	 * and BAUD is to be determined. Re-arranging the previous equation
	 * yields the following:
	 *
	 * BAUD = 65536 * (1 - (S*f_{baud}/f_{ref}))
	 *
	 * Oftentimes, the resulting value is not an integer. In such a case,
	 * the closest one should be selected to minimize the resulting bit-
	 * error rate.
     * 
     * My calculation:
     * 
     * BAUD = 65536 * (1 - (16 * 57,600 / 4MHz)) = 50,436.5056...
     * BAUD = 50,437
     * BAUD (in hex) = 0xC505
	 */
    SERCOM_CDC_REGS->SERCOM_BAUD = 0xC505;

    /*
	 * Third-to-the-last setup:
	 *
	 * - Enable receiver and transmitter
	 * - Clear the FIFOs (even though they're disabled)
	 * 
	 * NOTE: Do not enable the RXC and DRE interrupts during initialization,
	 *       as they will only clear when the buffer is empty and filled,
	 *       respectively.
	 */
    SERCOM_CDC_REGS->SERCOM_CTRLB |= (0x1 << 17) | (0x1 << 16) | (0x3 << 22);
    while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000004) != 0) {
        asm("nop");
    }

    /*
	 * Second-to-last: Configure the physical pins.
	 *
	 * - RX must have INEN enabled as well.
	 * - TX should have DRVSTR enabled to help overcome stray capacitance.
	 */
    //PORT_SEC_REGS->GROUP[1].PORT_OUTCLR = 0x00000200;
	//PORT_SEC_REGS->GROUP[1].PORT_DIRCLR = 0x00000200;
	//PORT_SEC_REGS->GROUP[1].PORT_OUTSET = 0x00000100;
	//PORT_SEC_REGS->GROUP[1].PORT_DIRSET = 0x00000100;
    PORT_SEC_REGS->GROUP[1].PORT_DIRCLR = (1 << 8) | (1 << 9);
    //PORT_SEC_REGS->GROUP[1].PORT_PMUX[(8 >> 1)] = 0x33;
    PORT_SEC_REGS->GROUP[1].PORT_PINCFG[9] = 0x03;
    //PORT_SEC_REGS->GROUP[1].PORT_PINCFG[8] = 0x41;
    PORT_SEC_REGS->GROUP[1].PORT_PINCFG[8] = 0x03;
    PORT_SEC_REGS->GROUP[1].PORT_PMUX[4] = 0x33;

    /*
	 * Last: enable the peripheral, after resetting the state machine
	 *
	 * NOTE: `CTRLB` is also included in the SYNCBUSY check, since TXEN and
	 *       RXEN internally get cleared (and `CTRLB` in SYNCBUSY set)
	 *       while the peripheral is being enabled. When done, TXEN and
	 *       RXEN are set again to 1 and `CTRLB` in SYNCBUSY gets cleared.
	 */
    SERCOM_CDC_REGS->SERCOM_CTRLA |= 0x00000002;
    while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000003) != 0) {
        asm("nop");
    }
    return;
}

// ==========================================================================

/*
 * Unlike most peripherals, each SERCOM contains 4 interrupt handlers:
 * 	- SERCOMx_0_Handler -- for Bit 0 of INTFLAG
 * 	- SERCOMx_1_Handler -- for Bit 1 of INTFLAG
 * 	- SERCOMx_2_Handler -- for Bit 2 of INTFLAG
 * 	- SERCOMx_OTHER_Handler -- for all remaining bits of INTFLAG
 *
 * NOTE: INTFLAG meaning depends on SERCOM operating mode.
 *
 * In USART operating mode:
 * 	- Bit 0 (DRE) indicates that the transmit buffer is empty. Cleared by
 *	  writing new data.
 * 	- Bit 2 (RXC) indicates that the receive buffer contains something.
 *	  Cleared when the buffer is empty.
 */

// IRQ for TX-empty
void __attribute__((used, interrupt)) SERCOM3_0_Handler(void)
{
    /*
	 * Per the datasheet, the TX buffer should only be written to while
	 * Bit 0 is set. Said bit is automatically cleared when the FIFO is
	 * full.
	 *
	 * Effectively, this loop isn't run if the interrupt was spurious.
	 */
	if (SERCOM_CDC_CTX->tx_state != 2) {
		return;
	}
	while ((SERCOM_CDC_REGS->SERCOM_INTFLAG & 0x01) != 0) {

        // Is there something to write to start with...
		while (SERCOM_CDC_CTX->txd_cwd.len == 0) {
			if (SERCOM_CDC_CTX->txd_nr_desc == 0) {
                // No more to write
				break;
			}
			SERCOM_CDC_CTX->txd_cwd = *(SERCOM_CDC_CTX->txd_next_desc++);
			--(SERCOM_CDC_CTX->txd_nr_desc);
		}
		if (SERCOM_CDC_CTX->txd_cwd.len == 0) {
            /*
			 * Really no more to write; clear the interrupt so that
			 * this IRQ does not re-trigger until a new request
			 * comes along.
			 */
			SERCOM_CDC_REGS->SERCOM_INTENCLR = 0x01;
			SERCOM_CDC_CTX->tx_state = 0;
			return;
		}

        // Fill in the FIFO.
		SERCOM_CDC_REGS->SERCOM_DATA = *(SERCOM_CDC_CTX->txd_cwd.buf++);
		--SERCOM_CDC_CTX->txd_cwd.len;
	}
}

// IRQ for RX-nonempty
void __attribute__((used, interrupt)) SERCOM3_2_Handler(void)
{
	uint16_t sc;
	uint32_t dt;
    
    /*
	 * Per the datasheet, the RX buffer should only be read from while
	 * Bit 2 is set. Said bit is automatically cleared when the FIFO is
	 * empty.
	 *
	 * Effectively, this loop isn't run if the interrupt was spurious.
	 */
	if (SERCOM_CDC_CTX->rx_state != 2) {
		return;
	}
	while ((SERCOM_CDC_REGS->SERCOM_INTFLAG & 0x04) != 0) {
        
        /*
		 * STATUS must be read **before** DATA to avoid losing
		 * important information.
		 */
		sc = SERCOM_CDC_REGS->SERCOM_STATUS;
		asm("nop");
		dt = SERCOM_CDC_REGS->SERCOM_DATA;

        // If there is nothing to store the data into, simply discard it.
		if (SERCOM_CDC_CTX->rx_apidata.len >= SERCOM_CDC_CTX->rx_len_max) {
			SERCOM_CDC_CTX->rx_apidata.err_overflow = 1;
			continue;
		}
        
        // Capture any error conditions.
		if ((sc & 0x0001) != 0) {
			SERCOM_CDC_CTX->rx_apidata.err_parity = 1;
		}
		if ((sc & 0x0002) != 0) {
			SERCOM_CDC_CTX->rx_apidata.err_framing = 1;
			if (dt == 0) {
				SERCOM_CDC_CTX->rx_apidata.err_break = 1;
			}
		}
		if ((sc & 0x0004) != 0) {
			SERCOM_CDC_CTX->rx_apidata.err_overflow = 1;
		}
		if ((sc & 0x0020) != 0) {
			SERCOM_CDC_CTX->rx_apidata.err_collision = 1;
		}

		SERCOM_CDC_CTX->rx_apidata.w_buf[SERCOM_CDC_CTX->rx_apidata.len++] = (char)(dt & 0x000000FF);
		SERCOM_CDC_CTX->rx_tick_ctr = 0;
		
		// Updating the async descriptor data length, tracking which is active.
		if (SERCOM_CDC_CTX->rx_async_desc != NULL) {
			SERCOM_CDC_CTX->rx_async_desc->compl_info.data_len = SERCOM_CDC_CTX->rx_apidata.len;
		}
		
		if (SERCOM_CDC_CTX->rx_apidata.len >= SERCOM_CDC_CTX->rx_len_max) {
			/*
			 * Got the number of bytes we have requested
			 *
			 * At this point, we indicate that we are done reading.
			 */
            SERCOM_CDC_REGS->SERCOM_INTENCLR = 0x04;
			SERCOM_CDC_CTX->rx_state = 3;
            
			// Setting completion type when buffer is full.
			if (SERCOM_CDC_CTX->rx_async_desc != NULL) {
				SERCOM_CDC_CTX->rx_async_desc->compl_type = PLATFORM_USART_RX_COMPL_DATA;
			}
			return;
		}
	}
}

/*
 * SysTick sub-handler for processing timeouts
 *
 * In order to support cases where the actual number of bytes in a message is
 * less than the buffer size, it is necessary to have a mechanism to force a
 * timeout when no further bytes are received after the first. This is done by
 * having a counter incremented via SysTick at regular intervals.
 * 
 * The character period is calculated using the following formula:
 *
 * t_{char} = (f_{baud}^{-1})*(1 + D + P + S)
 *
 * Here, `D` is the number of data bits (here, 8), and `S` is the number of
 * stop bits (1 or 2; "1.5" is rounded up to 2). `P` covers for the parity bit;
 * if present, P=1; otherwise, P=0. For the configuration specified in this
 * example, we have
 *
 * t_{char} = ((38400)^{-1}) * (1 + 8 + 0 + 1) = 260.416667 us
 *
 * Adding an extra bit or two (cue Nyquist) yields
 *
 * t_{char} = ((38400)^{-1}) * (1 + 8 + 0 + 1 + 2) = 312.5 us
 *
 * An internal counter is incremented twice every character period; thus, 50%
 * of the period is used as the value indicated in systick.c. When the counter
 * corresponds to 3.5 times character period (=7), a timeout is indicated and
 * whatever data has been received is made available to the application.
 */
void platform_internal_usart_cdc_systick_handler(void)
{
	if (SERCOM_CDC_CTX->rx_state != 2) {
        // No reception on-going
		return;
	} else if (SERCOM_CDC_CTX->rx_apidata.len == 0) {
        // No character received yet
		return;
	}

	SERCOM_CDC_CTX->rx_tick_ctr += 1;
	if (SERCOM_CDC_CTX->rx_tick_ctr >= 7) {
        // Inter-character timeout
		SERCOM_CDC_REGS->SERCOM_INTENCLR = 0x04;
		SERCOM_CDC_CTX->rx_state = 3;
        
		// Setting completion type on timeout.
		if (SERCOM_CDC_CTX->rx_async_desc != NULL) {
			SERCOM_CDC_CTX->rx_async_desc->compl_type = PLATFORM_USART_RX_COMPL_DATA;
		}
        
		return;
	}
}

// Adding call function for time manipulation/ tracking structure
void platform_usart_tick_handler(const platform_timespec_t *tick)
{
    platform_internal_usart_cdc_systick_handler();
}

// ==========================================================================

// Enqueue something for transmission
bool platform_usart_cdc_send_async(
	const struct platform_ro_buf_desc *vec, unsigned int nr
) {
	unsigned int a;
	const struct platform_ro_buf_desc *b;
	
    // Parameter check first
	if (!vec || nr == 0) {
        // Nothing to transmit
		return true;
	} else if (SERCOM_CDC_CTX->tx_state != 0) {
        // Busy
		return false;
	}
	SERCOM_CDC_CTX->tx_state = 1;

    // Clear the FIFO
	SERCOM_CDC_REGS->SERCOM_CTRLB |= (1 << 22);
	while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000004) != 0) {
		asm("nop");
	}

	SERCOM_CDC_CTX->txd_cwd.buf = NULL;
	SERCOM_CDC_CTX->txd_cwd.len = 0;
	SERCOM_CDC_CTX->txd_next_desc = &SERCOM_CDC_CTX->txd_real_desc[0];
	
	for (a = 0, SERCOM_CDC_CTX->txd_nr_desc = 0;
	    SERCOM_CDC_CTX->txd_nr_desc < 16 && a < nr; ++a) {
		b = &vec[a];
		if (b->len == 0 || b->buf == NULL) {
            // Skip over an empty descriptor
			continue;
		}
		SERCOM_CDC_CTX->txd_real_desc[SERCOM_CDC_CTX->txd_nr_desc++] = *b;
	}

    /*
	 * Last -- enable the DRE interrupt; the IRQ handler will be
	 * responsible for transmitting the data.
	 */
	SERCOM_CDC_CTX->tx_state = 2;
	SERCOM_CDC_REGS->SERCOM_INTENSET = 0x01;
	return true;
}

// Abort an on-going transmission
void platform_usart_cdc_send_abort(void)
{
	SERCOM_CDC_REGS->SERCOM_INTENCLR = 0x01;
	SERCOM_CDC_CTX->tx_state = 1;

    // Clear the FIFO
	SERCOM_CDC_REGS->SERCOM_CTRLB |= (1 << 22);
	while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000004) != 0) {
		asm("nop");
	}

    // Wait for any transmission to complete
	while ((SERCOM_CDC_REGS->SERCOM_INTFLAG & 0x02) == 0) {
		asm("nop");
	}

	SERCOM_CDC_CTX->txd_cwd.buf = NULL;
	SERCOM_CDC_CTX->txd_cwd.len = 0;
	SERCOM_CDC_CTX->txd_next_desc = 0;
	SERCOM_CDC_CTX->txd_nr_desc = 0;
	SERCOM_CDC_CTX->tx_state = 0;
	return;
}

// Transmission active?
bool platform_usart_cdc_tx_busy(void)
{
	return (SERCOM_CDC_CTX->tx_state != 0);
}

// ==========================================================================

// Enqueue something for reception
bool platform_usart_cdc_recv_async(char *buf, unsigned int len)
{
    // Parameter check first
	if (!buf || len == 0) {
        // Nothing to receive
		return true;
	} else if (SERCOM_CDC_CTX->rx_state != 0) {
        // Busy
		return false;
	}
	SERCOM_CDC_CTX->rx_state = 1;

    // Clean up state data
	memset(&SERCOM_CDC_CTX->rx_apidata, 0, sizeof(SERCOM_CDC_CTX->rx_apidata));
	SERCOM_CDC_CTX->rx_apidata.w_buf = buf;
	SERCOM_CDC_CTX->rx_len_max = len;

    // Second-to-the-last: Clear the FIFO
	SERCOM_CDC_REGS->SERCOM_CTRLB |= (1 << 23);
	while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000004) != 0) {
		asm("nop");
	}

    /*
	 * Last -- enable the RXC interrupt; the IRQ handler will be
	 * responsible for processing received data.
	 */
	SERCOM_CDC_CTX->rx_state = 2;
	SERCOM_CDC_REGS->SERCOM_INTENSET = 0x04;
	return true;
}

// Abort an on-going reception
void platform_usart_cdc_recv_abort(void)
{
	SERCOM_CDC_REGS->SERCOM_INTENCLR = 0x04;
	SERCOM_CDC_CTX->rx_state = 1;

    // Clear the FIFO
	SERCOM_CDC_REGS->SERCOM_CTRLB |= (1 << 23);
	while ((SERCOM_CDC_REGS->SERCOM_SYNCBUSY & 0x00000004) != 0) {
		asm("nop");
	}

	memset(&SERCOM_CDC_CTX->rx_apidata, 0, sizeof(SERCOM_CDC_CTX->rx_apidata));
	SERCOM_CDC_CTX->rx_state = 0;
	return;
}

// Check if there is any data to be read from the USART
bool platform_usart_cdc_recv_get(struct platform_usart_recv_data_type *desc)
{
	if (!desc) {
		return false;
	} else if (SERCOM_CDC_CTX->rx_state != 3) {
        // No completed transaction present
		return false;
	}

    // Copy, then clear.
	*desc = SERCOM_CDC_CTX->rx_apidata;
	memset(&SERCOM_CDC_CTX->rx_apidata, 0, sizeof(SERCOM_CDC_CTX->rx_apidata));
	SERCOM_CDC_CTX->rx_state = 0;
	return true;
}

// Reception active?
bool platform_usart_cdc_rx_busy(void)
{
	return (SERCOM_CDC_CTX->rx_state != 0);
}

// This function handles data transmission asynchronously.
bool platform_usart_cdc_tx_async(const platform_usart_tx_bufdesc_t *desc, unsigned int nr_desc)
{
    // Check input parameters
    if (!desc || nr_desc == 0) return true; // Nothing to send considered success
    
    // Convert from tx_bufdesc_t to ro_buf_desc format
    struct platform_ro_buf_desc converted_desc[16]; // Temp conversion buffer
    unsigned int valid_count = 0; // Count of valid buffer
    
    // Processing each input buffer descriptor.
    for (unsigned int i = 0; i < nr_desc && i < 16; i++) {
        if (desc[i].buf && desc[i].len > 0) {
            converted_desc[valid_count].buf = desc[i].buf;
            converted_desc[valid_count].len = desc[i].len;
            valid_count++;
        }
    }
    
    // If no valid buffers found, return success.
    if (valid_count == 0) return true;
    
    // Sending converted buffers.
    return platform_usart_cdc_send_async(converted_desc, valid_count);
}

// This function starts data reception asynchronously.
bool platform_usart_cdc_rx_async(platform_usart_rx_async_desc_t *desc)
{
    // Check input parameters
    if (!desc) return false; // NULL is error
    
    // Checking for existing reception, abort first if true
    if (SERCOM_CDC_CTX->rx_state != 0) {
        platform_usart_cdc_recv_abort();
    }
    
    // Reset completion state
    desc->compl_type = PLATFORM_USART_RX_COMPL_NONE; // No data received yet
    desc->compl_info.data_len = 0; // Zero bytes received
    
    // Storing descriptor for interrupts
    SERCOM_CDC_CTX->rx_async_desc = desc;
    
    // Starting asynchronous reception.
    return platform_usart_cdc_recv_async(desc->buf, desc->max_len);
}
