/* Climate service contract: periodic validated DHT22 samples in engineering units. */
#ifndef CLIMATE_SERVICE_H
#define CLIMATE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    CLIMATE_STATUS_OK = 0,
    CLIMATE_STATUS_INVALID_ARG,
    CLIMATE_STATUS_NOT_READY,
    CLIMATE_STATUS_NOT_DUE,
    CLIMATE_STATUS_TIMEOUT_RESPONSE,
    CLIMATE_STATUS_TIMEOUT_DATA,
    CLIMATE_STATUS_CHECKSUM_ERROR,
    CLIMATE_STATUS_OUT_OF_RANGE,
    CLIMATE_STATUS_IO_ERROR
} climate_status_t;

typedef struct
{
    int16_t temperature_tenths_c;
    uint16_t humidity_tenths_percent;
    uint32_t sampled_at_ms;
    bool is_valid;
} climate_reading_t;

/**
 * @brief Initializes the DHT22 service and its platform resources.
 *
 * @return CLIMATE_STATUS_OK when initialized.
 * @return CLIMATE_STATUS_IO_ERROR when platform initialization fails.
 */
climate_status_t climate_service_init(void);

/**
 * @brief Attempts a climate read only after the configured two-second interval.
 *
 * Returns NOT_DUE rather than blocking so a higher-rate motion service can run
 * between climate acquisitions. Failure preserves prior numeric fields but clears
 * is_valid.
 *
 * @param[out] reading Destination for latest measurement and validity; not NULL.
 *
 * @return CLIMATE_STATUS_OK when a new validated sample is available.
 * @return CLIMATE_STATUS_INVALID_ARG when reading is NULL.
 * @return CLIMATE_STATUS_NOT_READY before successful initialization.
 * @return CLIMATE_STATUS_NOT_DUE before the next climate sample time.
 * @return CLIMATE_STATUS_TIMEOUT_RESPONSE or TIMEOUT_DATA for missing sensor edges.
 * @return CLIMATE_STATUS_CHECKSUM_ERROR when the frame checksum is invalid.
 * @return CLIMATE_STATUS_OUT_OF_RANGE when decoded values exceed sensor limits.
 * @return CLIMATE_STATUS_IO_ERROR for another platform failure.
 *
 * @pre climate_service_init() has completed successfully.
 */
climate_status_t climate_service_poll(climate_reading_t *reading);

#endif /* CLIMATE_SERVICE_H */
