/* Vendor-neutral raw acceleration acquisition contract. */
#ifndef MOTION_PORT_H
#define MOTION_PORT_H

#include <stdint.h>

typedef enum
{
    MOTION_PORT_STATUS_OK = 0,
    MOTION_PORT_STATUS_INVALID_ARG,
    MOTION_PORT_STATUS_NOT_READY,
    MOTION_PORT_STATUS_ID_MISMATCH,
    MOTION_PORT_STATUS_NO_ACK,
    MOTION_PORT_STATUS_BUS_ERROR
} motion_port_status_t;

typedef struct
{
    int16_t accel_x_raw;
    int16_t accel_y_raw;
    int16_t accel_z_raw;
} motion_raw_sample_t;

/**
 * @brief Initializes and configures the motion sensor platform.
 *
 * @return MOTION_PORT_STATUS_OK when identity and sensor configuration succeed.
 * @return MOTION_PORT_STATUS_ID_MISMATCH when WHO_AM_I is not the expected MPU.
 * @return MOTION_PORT_STATUS_NO_ACK when the configured target NACKs.
 * @return MOTION_PORT_STATUS_BUS_ERROR for other I2C or initialization failures.
 */
motion_port_status_t motion_port_init(void);

/**
 * @brief Reads one fresh 3-axis accelerometer sample from the configured sensor.
 *
 * @param[out] sample Destination for signed raw X/Y/Z axis counts; must not be NULL.
 *
 * @return MOTION_PORT_STATUS_OK when the burst read succeeds.
 * @return MOTION_PORT_STATUS_INVALID_ARG when sample is NULL.
 * @return MOTION_PORT_STATUS_NOT_READY before successful initialization.
 * @return MOTION_PORT_STATUS_NO_ACK when the target NACKs.
 * @return MOTION_PORT_STATUS_BUS_ERROR for another shared-bus failure.
 *
 * @pre motion_port_init() completed successfully.
 */
motion_port_status_t motion_port_read_acceleration(motion_raw_sample_t *sample);

#endif /* MOTION_PORT_H */
