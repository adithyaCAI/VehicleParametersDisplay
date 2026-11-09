/* Bridges vendor-neutral logging to the board's ST-LINK Virtual COM Port. */
#include "logger_platform.h"

#include "platform_uart2.h"

int32_t logger_platform_write(const uint8_t *data, uint16_t length)
{
    if (platform_uart2_init() != PLATFORM_UART2_STATUS_OK)
    {
        return -1;
    }

    return (platform_uart2_write(data, length) == PLATFORM_UART2_STATUS_OK) ? 0 : -1;
}
