/* STM32 shared-I2C adapter for SH1106 command/data transfers. */
#include "oled_port.h"

#include "app_config.h"
#include "i2c_bus.h"

#include <stddef.h>
#include <stdint.h>

#define PLATFORM_OLED_MAX_PAYLOAD_BYTES APP_OLED_FLUSH_CHUNK_BYTES
#define PLATFORM_OLED_TRANSACTION_BYTES (PLATFORM_OLED_MAX_PAYLOAD_BYTES + 1U)

static oled_port_status_t platform_oled_map_i2c_status(i2c_bus_status_t status)
{
    switch (status)
    {
        case I2C_BUS_STATUS_OK:
            return OLED_PORT_STATUS_OK;
        case I2C_BUS_STATUS_INVALID_ARG:
            return OLED_PORT_STATUS_INVALID_ARG;
        case I2C_BUS_STATUS_BUSY:
            return OLED_PORT_STATUS_BUSY;
        case I2C_BUS_STATUS_NOT_READY:
            return OLED_PORT_STATUS_NOT_READY;
        case I2C_BUS_STATUS_NOT_FOUND:
            return OLED_PORT_STATUS_NO_ACK;
        case I2C_BUS_STATUS_IO_ERROR:
        default:
            return OLED_PORT_STATUS_IO_ERROR;
    }
}

oled_port_status_t oled_port_write(uint8_t address_7bit,
                                   uint8_t control_byte,
                                   const uint8_t *data,
                                   size_t length)
{
    if ((address_7bit > 0x7FU) || (data == NULL) || (length == 0U) ||
        (length > PLATFORM_OLED_MAX_PAYLOAD_BYTES))
    {
        return OLED_PORT_STATUS_INVALID_ARG;
    }

    uint8_t transaction[PLATFORM_OLED_TRANSACTION_BYTES];
    transaction[0] = control_byte;
    for (size_t index = 0U; index < length; ++index)
    {
        transaction[index + 1U] = data[index];
    }

    i2c_bus_status_t bus_status = i2c_bus_write(address_7bit,
                                                transaction,
                                                length + 1U);
    return platform_oled_map_i2c_status(bus_status);
}
