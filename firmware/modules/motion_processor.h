/* Portable acceleration calibration, axis semantics, and motion-event processing. */
#ifndef MOTION_PROCESSOR_H
#define MOTION_PROCESSOR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MOTION_AXIS_LATERAL_X = 0,
    MOTION_AXIS_LONGITUDINAL_Y,
    MOTION_AXIS_VERTICAL_Z
} motion_axis_t;

typedef enum
{
    MOTION_EVENT_NONE = 0,
    MOTION_EVENT_SPEEDING,
    MOTION_EVENT_WIDE_TURN,
    MOTION_EVENT_UNSEEN_SPEED_BREAKER,
    MOTION_EVENT_SUDDEN_BRAKING
} motion_event_t;

typedef enum
{
    MOTION_PROCESSOR_STATUS_OK = 0,
    MOTION_PROCESSOR_STATUS_INVALID_ARG,
    MOTION_PROCESSOR_STATUS_NOT_READY,
    MOTION_PROCESSOR_STATUS_CALIBRATING
} motion_processor_status_t;

typedef struct
{
    int16_t x_millig;
    int16_t y_millig;
    int16_t z_millig;
    uint32_t sampled_at_ms;
    bool is_calibrated;
    motion_event_t event;
} motion_sample_t;

/**
 * @brief Resets one-second startup calibration and event history.
 *
 * The first accepted sample starts the flat/still calibration interval.
 */
void motion_processor_init(void);

/**
 * @brief Converts one MPU6050 raw sample and processes calibration/events.
 *
 * Uses the MPU-6000/MPU-6050 Register Map sensitivity of 8192 LSB/g for ±4 g.
 * During the initial one-second calibration, axes are reported as 0, 0, +1000
 * milli-g and event classification is suppressed. Calibrated X/Y/Z preserve the
 * PRD meanings lateral, longitudinal and vertical respectively.
 *
 * @param[in] raw_x Signed X accelerometer count.
 * @param[in] raw_y Signed Y accelerometer count.
 * @param[in] raw_z Signed Z accelerometer count.
 * @param[in] now_ms Monotonic sample time in milliseconds.
 * @param[out] sample Calibrated axes and event; must not be NULL.
 *
 * @return MOTION_PROCESSOR_STATUS_OK when calibrated output is available.
 * @return MOTION_PROCESSOR_STATUS_CALIBRATING during the one-second calibration.
 * @return MOTION_PROCESSOR_STATUS_INVALID_ARG when sample is NULL.
 *
 * @pre motion_processor_init() has completed or runs on the first call.
 */
motion_processor_status_t motion_processor_process(int16_t raw_x,
                                                   int16_t raw_y,
                                                   int16_t raw_z,
                                                   uint32_t now_ms,
                                                   motion_sample_t *sample);

/**
 * @brief Returns the configured human-readable name for one motion event.
 *
 * @param[in] event Classified motion event.
 *
 * @return Static event label, or an empty string for NONE/invalid values.
 */
const char *motion_processor_event_name(motion_event_t event);

#endif /* MOTION_PROCESSOR_H */
