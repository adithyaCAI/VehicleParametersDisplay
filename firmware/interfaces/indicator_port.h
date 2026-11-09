/* Vendor-neutral logical indicator output contract. */
#ifndef INDICATOR_PORT_H
#define INDICATOR_PORT_H

#include <stdbool.h>

typedef enum
{
    INDICATOR_PORT_STATUS_OK = 0,
    INDICATOR_PORT_STATUS_INVALID_ARG,
    INDICATOR_PORT_STATUS_IO_ERROR
} indicator_port_status_t;

/**
 * @brief Initializes the board user LED output in its inactive state.
 *
 * @return INDICATOR_PORT_STATUS_OK when the output is ready.
 * @return INDICATOR_PORT_STATUS_IO_ERROR when platform initialization fails.
 */
indicator_port_status_t indicator_port_init(void);

/**
 * @brief Sets the logical on/off state of the board user LED.
 *
 * @param[in] is_on Desired logical LED state, independent of pin polarity.
 *
 * @return INDICATOR_PORT_STATUS_OK when the state is applied.
 * @return INDICATOR_PORT_STATUS_IO_ERROR when the platform is not initialized.
 *
 * @pre indicator_port_init() completed successfully.
 */
indicator_port_status_t indicator_port_set(bool is_on);

#endif /* INDICATOR_PORT_H */
