/* OLED dashboard refresh policy and hardware-independent display data contract. */
#ifndef OLED_SERVICE_H
#define OLED_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "motion_processor.h"

typedef enum
{
    OLED_SERVICE_STATUS_OK = 0,
    OLED_SERVICE_STATUS_INVALID_ARG,
    OLED_SERVICE_STATUS_NOT_READY,
    OLED_SERVICE_STATUS_NOT_DUE,
    OLED_SERVICE_STATUS_BUSY,
    OLED_SERVICE_STATUS_NO_ACK,
    OLED_SERVICE_STATUS_IO_ERROR
} oled_service_status_t;

typedef struct
{
    int16_t temperature_tenths_c;
    uint16_t humidity_tenths_percent;
    int16_t x_millig;
    int16_t y_millig;
    int16_t z_millig;
    motion_event_t active_event;
    bool climate_is_valid;
    bool motion_is_calibrated;
} oled_dashboard_data_t;

typedef struct
{
    bool is_initialized;
    bool has_pending_frame;
    bool has_drawn_dashboard;
    uint32_t last_refresh_ms;
    uint32_t next_retry_ms;
    uint8_t text_row;
} oled_service_state_t;

/**
 * @brief Initializes the display service state before OLED startup.
 *
 * @param[out] state State object to initialize; NULL is ignored.
 */
void oled_service_init_state(oled_service_state_t *state);

/**
 * @brief Renders the dashboard to the SH1106 and sends one bounded I2C chunk.
 *
 * A complete framebuffer is staged only at the refresh cadence or when its
 * meaningful content changes; one shared-bus chunk is transferred per call.
 * Invalid climate and uncalibrated motion values are displayed as unavailable.
 *
 * @param[in,out] state Initialized service state; must not be NULL.
 * @param[in] data Latest sensor values and validity flags; must not be NULL.
 * @param[in] now_ms Monotonic time in milliseconds.
 *
 * @return OLED_SERVICE_STATUS_OK after a chunk is sent or no transfer is due.
 * @return OLED_SERVICE_STATUS_INVALID_ARG for NULL arguments.
 * @return OLED_SERVICE_STATUS_NOT_READY if display initialization is pending.
 * @return OLED_SERVICE_STATUS_BUSY when the shared bus is contended.
 * @return OLED_SERVICE_STATUS_NO_ACK when the OLED address NACKs.
 * @return OLED_SERVICE_STATUS_IO_ERROR for another bus or driver error.
 */
oled_service_status_t oled_service_refresh(oled_service_state_t *state,
                                           const oled_dashboard_data_t *data,
                                           uint32_t now_ms);

#endif /* OLED_SERVICE_H */
