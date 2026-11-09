/* Nonblocking MPU6050 acquisition service interface. */
#ifndef MOTION_SERVICE_H
#define MOTION_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "motion_processor.h"

typedef enum
{
    MOTION_SERVICE_STATUS_OK = 0,
    MOTION_SERVICE_STATUS_INVALID_ARG,
    MOTION_SERVICE_STATUS_NOT_READY,
    MOTION_SERVICE_STATUS_NOT_DUE,
    MOTION_SERVICE_STATUS_CALIBRATING,
    MOTION_SERVICE_STATUS_ID_MISMATCH,
    MOTION_SERVICE_STATUS_NO_ACK,
    MOTION_SERVICE_STATUS_BUS_ERROR
} motion_service_status_t;

typedef struct
{
    motion_sample_t sample;
    uint32_t sample_sequence;
    motion_event_t pending_events;
    bool is_valid;
} motion_service_reading_t;

/**
 * @brief Initializes the MPU6050 interface and motion-processing state.
 *
 * @return MOTION_SERVICE_STATUS_OK when identity and sensor configuration succeed.
 * @return MOTION_SERVICE_STATUS_ID_MISMATCH when WHO_AM_I is not the MPU identity.
 * @return MOTION_SERVICE_STATUS_NO_ACK when the configured target address NACKs.
 * @return MOTION_SERVICE_STATUS_BUS_ERROR for other I2C/setup failures.
 */
motion_service_status_t motion_service_init(void);

/**
 * @brief Acquires and processes one acceleration sample when the 10 ms period is due.
 *
 * The first one-second interval is reserved for flat/still calibration. During
 * calibration this API returns CALIBRATING and motion events remain suppressed.
 *
 * @param[out] reading Latest calibrated axes, sample sequence and event; not NULL.
 *
 * @return MOTION_SERVICE_STATUS_OK for a processed calibrated sample.
 * @return MOTION_SERVICE_STATUS_CALIBRATING during startup calibration.
 * @return MOTION_SERVICE_STATUS_NOT_DUE before the next 10 ms sample slot.
 * @return MOTION_SERVICE_STATUS_INVALID_ARG when reading is NULL.
 * @return MOTION_SERVICE_STATUS_NOT_READY before initialization.
 * @return MOTION_SERVICE_STATUS_ID_MISMATCH or NO_ACK for sensor identification errors.
 * @return MOTION_SERVICE_STATUS_BUS_ERROR when the shared bus fails.
 *
 * @pre motion_service_init() has completed successfully.
 */
motion_service_status_t motion_service_poll(motion_service_reading_t *reading);

/**
 * @brief Clears the latched motion event after application reporting.
 *
 * @post The next poll carries only subsequently detected events.
 */
void motion_service_acknowledge_event(void);

#endif /* MOTION_SERVICE_H */
