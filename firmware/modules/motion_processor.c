/* Portable fixed-point MPU6050 scaling, one-second calibration, and event policy. */
#include "motion_processor.h"

#include "app_config.h"
#include "runtime_clock.h"

#include <stddef.h>
#include <stdint.h>

#define MOTION_PROCESSOR_MILLI_G_PER_G 1000

typedef struct
{
    int64_t x_sum_millig;
    int64_t y_sum_millig;
    int64_t z_sum_millig;
    int32_t x_offset_millig;
    int32_t y_offset_millig;
    int32_t z_offset_millig;
    int32_t window_start_x_millig;
    int32_t window_start_y_millig;
    int32_t window_start_z_millig;
    uint32_t calibration_start_ms;
    uint32_t window_start_ms;
    uint16_t calibration_sample_count;
    bool calibration_started;
    bool calibration_complete;
    bool has_window_start;
} motion_processor_state_t;

static motion_processor_state_t motion_processor_state;

/**
 * @brief Scales raw accelerometer counts to integer milli-g using 8192 LSB/g.
 *
 * @param[in] raw_count Signed MPU6050 accelerometer count.
 *
 * @return Acceleration in milli-g, truncated toward zero.
 */
static int32_t motion_processor_raw_to_millig(int16_t raw_count)
{
    int32_t numerator = (int32_t)raw_count * MOTION_PROCESSOR_MILLI_G_PER_G;
    return numerator / (int32_t)APP_MOTION_SENSITIVITY_LSB_PER_G;
}

/**
 * @brief Saturates a calibrated milli-g result to the sample's int16 range.
 *
 * @param[in] value_millig Calibrated acceleration in milli-g.
 *
 * @return Clamped signed 16-bit axis value.
 */
static int16_t motion_processor_clamp_millig(int32_t value_millig)
{
    if (value_millig > INT16_MAX)
    {
        value_millig = INT16_MAX;
    }
    else if (value_millig < INT16_MIN)
    {
        value_millig = INT16_MIN;
    }
    return (int16_t)value_millig;
}

/**
 * @brief Selects one sudden event with the PRD's highest-first event priority.
 *
 * @param[in] x_delta_millig Lateral X change inside the detection window.
 * @param[in] y_delta_millig Longitudinal Y change inside the detection window.
 * @param[in] z_delta_millig Vertical Z change inside the detection window.
 *
 * @return Highest-priority sudden event or MOTION_EVENT_NONE.
 */
static motion_event_t motion_processor_classify_event(int32_t x_delta_millig,
                                                      int32_t y_delta_millig,
                                                      int32_t z_delta_millig)
{
    if (y_delta_millig <= -APP_MOTION_SUDDEN_BRAKING_THRESHOLD_MILLIG)
    {
        return MOTION_EVENT_SUDDEN_BRAKING;
    }
    if ((z_delta_millig >= APP_MOTION_SPEED_BREAKER_THRESHOLD_MILLIG) ||
        (z_delta_millig <= -APP_MOTION_SPEED_BREAKER_THRESHOLD_MILLIG))
    {
        return MOTION_EVENT_UNSEEN_SPEED_BREAKER;
    }
    if ((x_delta_millig >= APP_MOTION_WIDE_TURN_THRESHOLD_MILLIG) ||
        (x_delta_millig <= -APP_MOTION_WIDE_TURN_THRESHOLD_MILLIG))
    {
        return MOTION_EVENT_WIDE_TURN;
    }
    if (y_delta_millig >= APP_MOTION_SPEEDING_THRESHOLD_MILLIG)
    {
        return MOTION_EVENT_SPEEDING;
    }
    return MOTION_EVENT_NONE;
}

void motion_processor_init(void)
{
    motion_processor_state.x_sum_millig = 0;
    motion_processor_state.y_sum_millig = 0;
    motion_processor_state.z_sum_millig = 0;
    motion_processor_state.x_offset_millig = 0;
    motion_processor_state.y_offset_millig = 0;
    motion_processor_state.z_offset_millig = 0;
    motion_processor_state.window_start_x_millig = 0;
    motion_processor_state.window_start_y_millig = 0;
    motion_processor_state.window_start_z_millig = MOTION_PROCESSOR_MILLI_G_PER_G;
    motion_processor_state.calibration_start_ms = runtime_clock_now_ms();
    motion_processor_state.window_start_ms = 0U;
    motion_processor_state.calibration_sample_count = 0U;
    motion_processor_state.calibration_started = true;
    motion_processor_state.calibration_complete = false;
    motion_processor_state.has_window_start = false;
}

motion_processor_status_t motion_processor_process(int16_t raw_x,
                                                   int16_t raw_y,
                                                   int16_t raw_z,
                                                   uint32_t now_ms,
                                                   motion_sample_t *sample)
{
    if (sample == NULL)
    {
        return MOTION_PROCESSOR_STATUS_INVALID_ARG;
    }
    if (!motion_processor_state.calibration_started)
    {
        motion_processor_init();
    }

    int32_t x_millig = motion_processor_raw_to_millig(raw_x);
    int32_t y_millig = motion_processor_raw_to_millig(raw_y);
    int32_t z_millig = motion_processor_raw_to_millig(raw_z);
    sample->sampled_at_ms = now_ms;
    sample->event = MOTION_EVENT_NONE;

    if (!motion_processor_state.calibration_complete)
    {
        uint32_t elapsed_ms = (uint32_t)(now_ms - motion_processor_state.calibration_start_ms);
        if (elapsed_ms < APP_MOTION_CALIBRATION_DURATION_MS)
        {
            motion_processor_state.x_sum_millig += x_millig;
            motion_processor_state.y_sum_millig += y_millig;
            motion_processor_state.z_sum_millig += z_millig;
            if (motion_processor_state.calibration_sample_count < UINT16_MAX)
            {
                ++motion_processor_state.calibration_sample_count;
            }

            sample->x_millig = 0;
            sample->y_millig = 0;
            sample->z_millig = MOTION_PROCESSOR_MILLI_G_PER_G;
            sample->is_calibrated = false;
            return MOTION_PROCESSOR_STATUS_CALIBRATING;
        }
        if (motion_processor_state.calibration_sample_count == 0U)
        {
            /* Restart the calibration interval when no sample arrived during the first one. */
            motion_processor_state.x_sum_millig = x_millig;
            motion_processor_state.y_sum_millig = y_millig;
            motion_processor_state.z_sum_millig = z_millig;
            motion_processor_state.calibration_sample_count = 1U;
            motion_processor_state.calibration_start_ms = now_ms;
            sample->x_millig = 0;
            sample->y_millig = 0;
            sample->z_millig = MOTION_PROCESSOR_MILLI_G_PER_G;
            sample->is_calibrated = false;
            return MOTION_PROCESSOR_STATUS_CALIBRATING;
        }

        int64_t count = (int64_t)motion_processor_state.calibration_sample_count;
        int32_t mean_x = (int32_t)(motion_processor_state.x_sum_millig / count);
        int32_t mean_y = (int32_t)(motion_processor_state.y_sum_millig / count);
        int32_t mean_z = (int32_t)(motion_processor_state.z_sum_millig / count);
        motion_processor_state.x_offset_millig = mean_x;
        motion_processor_state.y_offset_millig = mean_y;
        motion_processor_state.z_offset_millig = mean_z - MOTION_PROCESSOR_MILLI_G_PER_G;
        motion_processor_state.calibration_complete = true;
        motion_processor_state.has_window_start = false;
    }

    x_millig -= motion_processor_state.x_offset_millig;
    y_millig -= motion_processor_state.y_offset_millig;
    z_millig -= motion_processor_state.z_offset_millig;
    sample->x_millig = motion_processor_clamp_millig(x_millig);
    sample->y_millig = motion_processor_clamp_millig(y_millig);
    sample->z_millig = motion_processor_clamp_millig(z_millig);
    sample->is_calibrated = true;

    if (motion_processor_state.has_window_start)
    {
        uint32_t window_elapsed_ms = (uint32_t)(now_ms - motion_processor_state.window_start_ms);
        if (window_elapsed_ms <= APP_MOTION_EVENT_WINDOW_MS)
        {
            int32_t x_delta = x_millig - motion_processor_state.window_start_x_millig;
            int32_t y_delta = y_millig - motion_processor_state.window_start_y_millig;
            int32_t z_delta = z_millig - motion_processor_state.window_start_z_millig;
            sample->event = motion_processor_classify_event(x_delta, y_delta, z_delta);
        }
    }

    if (!motion_processor_state.has_window_start ||
        ((uint32_t)(now_ms - motion_processor_state.window_start_ms) >= APP_MOTION_EVENT_WINDOW_MS))
    {
        motion_processor_state.window_start_x_millig = x_millig;
        motion_processor_state.window_start_y_millig = y_millig;
        motion_processor_state.window_start_z_millig = z_millig;
        motion_processor_state.window_start_ms = now_ms;
        motion_processor_state.has_window_start = true;
    }

    return MOTION_PROCESSOR_STATUS_OK;
}

const char *motion_processor_event_name(motion_event_t event)
{
    switch (event)
    {
        case MOTION_EVENT_SPEEDING:
            return "Speeding";
        case MOTION_EVENT_WIDE_TURN:
            return "Wide Turn";
        case MOTION_EVENT_UNSEEN_SPEED_BREAKER:
            return "Unseen SpeedBreaker";
        case MOTION_EVENT_SUDDEN_BRAKING:
            return "Sudden Braking";
        case MOTION_EVENT_NONE:
        default:
            return "";
    }
}
