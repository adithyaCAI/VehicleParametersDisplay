/* Periodic climate sampling policy kept independent of STM32 GPIO/HAL details. */
#include "climate_service.h"

#include "app_config.h"
#include "climate_port.h"
#include "runtime_clock.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool climate_service_is_initialized;
static bool climate_service_has_sampled;
static uint32_t climate_service_last_sample_ms;

/**
 * @brief Maps a platform result to the service's public status type.
 *
 * @param[in] port_status DHT22 platform outcome.
 *
 * @return Corresponding service status.
 */
static climate_status_t climate_service_map_status(climate_port_status_t port_status)
{
    switch (port_status)
    {
        case CLIMATE_PORT_STATUS_OK:
            return CLIMATE_STATUS_OK;
        case CLIMATE_PORT_STATUS_INVALID_ARG:
            return CLIMATE_STATUS_INVALID_ARG;
        case CLIMATE_PORT_STATUS_NOT_READY:
            return CLIMATE_STATUS_NOT_READY;
        case CLIMATE_PORT_STATUS_TIMEOUT_RESPONSE:
            return CLIMATE_STATUS_TIMEOUT_RESPONSE;
        case CLIMATE_PORT_STATUS_TIMEOUT_DATA:
            return CLIMATE_STATUS_TIMEOUT_DATA;
        case CLIMATE_PORT_STATUS_CHECKSUM_ERROR:
            return CLIMATE_STATUS_CHECKSUM_ERROR;
        case CLIMATE_PORT_STATUS_OUT_OF_RANGE:
            return CLIMATE_STATUS_OUT_OF_RANGE;
        case CLIMATE_PORT_STATUS_IO_ERROR:
        default:
            return CLIMATE_STATUS_IO_ERROR;
    }
}

climate_status_t climate_service_init(void)
{
    climate_port_status_t port_status = climate_port_init();
    if (port_status != CLIMATE_PORT_STATUS_OK)
    {
        climate_service_is_initialized = false;
        return climate_service_map_status(port_status);
    }

    climate_service_is_initialized = true;
    climate_service_has_sampled = false;
    climate_service_last_sample_ms = 0U;
    return CLIMATE_STATUS_OK;
}

climate_status_t climate_service_poll(climate_reading_t *reading)
{
    if (reading == NULL)
    {
        return CLIMATE_STATUS_INVALID_ARG;
    }
    if (!climate_service_is_initialized)
    {
        reading->is_valid = false;
        return CLIMATE_STATUS_NOT_READY;
    }

    uint32_t now_ms = runtime_clock_now_ms();
    if (climate_service_has_sampled)
    {
        uint32_t elapsed_ms = (uint32_t)(now_ms - climate_service_last_sample_ms);
        if (elapsed_ms < APP_DHT22_SAMPLE_INTERVAL_MS)
        {
            return CLIMATE_STATUS_NOT_DUE;
        }
    }

    climate_service_last_sample_ms = now_ms;
    climate_service_has_sampled = true;

    int16_t temperature_tenths_c = 0;
    uint16_t humidity_tenths_percent = 0U;
    climate_port_status_t port_status = climate_port_read(&temperature_tenths_c,
                                                          &humidity_tenths_percent);
    if (port_status != CLIMATE_PORT_STATUS_OK)
    {
        reading->is_valid = false;
        return climate_service_map_status(port_status);
    }

    reading->temperature_tenths_c = temperature_tenths_c;
    reading->humidity_tenths_percent = humidity_tenths_percent;
    reading->sampled_at_ms = now_ms;
    reading->is_valid = true;
    return CLIMATE_STATUS_OK;
}
