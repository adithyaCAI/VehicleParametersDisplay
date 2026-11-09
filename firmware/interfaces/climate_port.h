/* Vendor-neutral contract between the climate service and DHT22 platform. */
#ifndef CLIMATE_PORT_H
#define CLIMATE_PORT_H

#include <stdint.h>

#include "runtime_clock.h"

typedef enum
{
    CLIMATE_PORT_STATUS_OK = 0,
    CLIMATE_PORT_STATUS_INVALID_ARG,
    CLIMATE_PORT_STATUS_NOT_READY,
    CLIMATE_PORT_STATUS_TIMEOUT_RESPONSE,
    CLIMATE_PORT_STATUS_TIMEOUT_DATA,
    CLIMATE_PORT_STATUS_CHECKSUM_ERROR,
    CLIMATE_PORT_STATUS_OUT_OF_RANGE,
    CLIMATE_PORT_STATUS_IO_ERROR
} climate_port_status_t;

/**
 * @brief Initializes the DHT22 platform resources.
 *
 * @return CLIMATE_PORT_STATUS_OK when resources are ready.
 * @return CLIMATE_PORT_STATUS_IO_ERROR when timer or GPIO setup fails.
 */
climate_port_status_t climate_port_init(void);

/**
 * @brief Reads one DHT22 temperature/humidity frame.
 *
 * Values are returned in tenths of degrees Celsius and tenths of relative
 * humidity percent. Output values are modified only after checksum and range
 * validation succeed.
 *
 * @param[out] temperature_tenths_c Temperature in 0.1 °C units; not NULL.
 * @param[out] humidity_tenths_percent Relative humidity in 0.1% units; not NULL.
 *
 * @return CLIMATE_PORT_STATUS_OK for a validated sensor frame.
 * @return CLIMATE_PORT_STATUS_INVALID_ARG for a null output.
 * @return CLIMATE_PORT_STATUS_NOT_READY before initialization.
 * @return CLIMATE_PORT_STATUS_TIMEOUT when a response/data edge is absent.
 * @return CLIMATE_PORT_STATUS_CHECKSUM_ERROR when the frame checksum is invalid.
 * @return CLIMATE_PORT_STATUS_OUT_OF_RANGE when decoded values exceed DHT22 limits.
 * @return CLIMATE_PORT_STATUS_IO_ERROR for another peripheral failure.
 *
 * @pre climate_port_init() has completed successfully.
 */
climate_port_status_t climate_port_read(int16_t *temperature_tenths_c,
                                        uint16_t *humidity_tenths_percent);

/**
 * @brief Returns the millisecond tick used by the climate sampling schedule.
 *
 * @return Current monotonic 32-bit millisecond tick, wrapping naturally.
 */
uint32_t runtime_clock_now_ms(void);

#endif /* CLIMATE_PORT_H */
