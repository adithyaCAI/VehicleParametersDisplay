/* ST-LINK Virtual COM Port adapter used for firmware diagnostics. */
#ifndef PLATFORM_UART2_H
#define PLATFORM_UART2_H

#include <stdint.h>

typedef enum
{
    PLATFORM_UART2_STATUS_OK = 0,
    PLATFORM_UART2_STATUS_IO_ERROR,
    PLATFORM_UART2_STATUS_NOT_READY
} platform_uart2_status_t;

/**
 * @brief Initializes USART2 for the board's ST-LINK Virtual COM Port.
 *
 * Configures the NUCLEO-G070RB VCP signals on PA2/PA3 at the configured baud
 * rate, 8 data bits, no parity, one stop bit, and no hardware flow control.
 * Repeated successful calls leave USART2 initialized.
 *
 * @return PLATFORM_UART2_STATUS_OK when initialized or already ready.
 * @return PLATFORM_UART2_STATUS_IO_ERROR when HAL initialization fails.
 */
platform_uart2_status_t platform_uart2_init(void);

/**
 * @brief Writes a bounded byte sequence to the ST-LINK Virtual COM Port.
 *
 * The data buffer is borrowed and not modified. This blocking operation is not
 * safe to call from an interrupt context.
 *
 * @param[in] data Bytes to transmit; must not be NULL when length is nonzero.
 * @param[in] length Number of bytes to transmit.
 *
 * @return PLATFORM_UART2_STATUS_OK when all bytes are transmitted.
 * @return PLATFORM_UART2_STATUS_NOT_READY before successful initialization.
 * @return PLATFORM_UART2_STATUS_IO_ERROR for invalid input or HAL failure.
 *
 * @pre platform_uart2_init() has completed successfully.
 */
platform_uart2_status_t platform_uart2_write(const uint8_t *data, uint16_t length);

#endif /* PLATFORM_UART2_H */
