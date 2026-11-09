/* Vendor-neutral I2C control/data write contract for the SH1106 interface. */
#ifndef OLED_PORT_H
#define OLED_PORT_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    OLED_PORT_STATUS_OK = 0,
    OLED_PORT_STATUS_INVALID_ARG,
    OLED_PORT_STATUS_BUSY,
    OLED_PORT_STATUS_NOT_READY,
    OLED_PORT_STATUS_NO_ACK,
    OLED_PORT_STATUS_IO_ERROR
} oled_port_status_t;

/**
 * @brief Sends one bounded SH1106 command or data transaction.
 *
 * The controller control byte and payload are combined and sent as one
 * serialized shared-I2C transaction. The payload is borrowed for the call.
 *
 * @param[in] address_7bit Unshifted OLED address in the range 0..0x7F.
 * @param[in] control_byte SH1106 I2C control byte (command or display data).
 * @param[in] data Borrowed payload; must not be NULL.
 * @param[in] length Payload size in bytes; must fit the platform transaction buffer.
 *
 * @return OLED_PORT_STATUS_OK when the transaction completes.
 * @return OLED_PORT_STATUS_INVALID_ARG for an invalid address, pointer or length.
 * @return OLED_PORT_STATUS_BUSY when another shared-bus transaction is active.
 * @return OLED_PORT_STATUS_NOT_READY before I2C1 is initialized.
 * @return OLED_PORT_STATUS_NO_ACK when the OLED address NACKs.
 * @return OLED_PORT_STATUS_IO_ERROR for other I2C failures.
 */
oled_port_status_t oled_port_write(uint8_t address_7bit,
                                   uint8_t control_byte,
                                   const uint8_t *data,
                                   size_t length);

#endif /* OLED_PORT_H */
