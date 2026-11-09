/* Nonblocking 100 Hz MPU6050 acquisition service with priority-latched events. */
#include "motion_service.h"

#include "app_config.h"
#include "motion_port.h"
#include "motion_processor.h"
#include "runtime_clock.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool motion_service_is_initialized;
static bool motion_service_has_sampled;
static uint32_t motion_service_last_sample_ms;
static uint32_t motion_service_sample_sequence;
static motion_event_t motion_service_pending_events;

/**
 * @brief Latches a newly observed event without demoting a higher-priority event.
 *
 * Priority is Sudden Braking, Unseen SpeedBreaker, Wide Turn, then Speeding.
 * Repeated same/lower-priority events leave the currently pending event intact.
 *
 * @param[in] event Newly classified event.
 */
static void motion_service_latch_event(motion_event_t event)
{
    if ((event == MOTION_EVENT_SUDDEN_BRAKING) ||
        ((event == MOTION_EVENT_UNSEEN_SPEED_BREAKER) &&
         (motion_service_pending_events != MOTION_EVENT_SUDDEN_BRAKING)) ||
        ((event == MOTION_EVENT_WIDE_TURN) &&
         (motion_service_pending_events != MOTION_EVENT_SUDDEN_BRAKING) &&
         (motion_service_pending_events != MOTION_EVENT_UNSEEN_SPEED_BREAKER)) ||
        ((event == MOTION_EVENT_SPEEDING) &&
         (motion_service_pending_events == MOTION_EVENT_NONE)))
    {
        motion_service_pending_events = event;
    }
}

motion_service_status_t motion_service_init(void)
{
    motion_port_status_t port_status = motion_port_init();
    if (port_status == MOTION_PORT_STATUS_ID_MISMATCH)
    {
        motion_service_is_initialized = false;
        return MOTION_SERVICE_STATUS_ID_MISMATCH;
    }
    if (port_status == MOTION_PORT_STATUS_NO_ACK)
    {
        motion_service_is_initialized = false;
        return MOTION_SERVICE_STATUS_NO_ACK;
    }
    if (port_status != MOTION_PORT_STATUS_OK)
    {
        motion_service_is_initialized = false;
        return MOTION_SERVICE_STATUS_BUS_ERROR;
    }

    motion_processor_init();
    motion_service_has_sampled = false;
    motion_service_last_sample_ms = 0U;
    motion_service_sample_sequence = 0U;
    motion_service_pending_events = MOTION_EVENT_NONE;
    motion_service_is_initialized = true;
    return MOTION_SERVICE_STATUS_OK;
}

motion_service_status_t motion_service_poll(motion_service_reading_t *reading)
{
    if (reading == NULL)
    {
        return MOTION_SERVICE_STATUS_INVALID_ARG;
    }
    if (!motion_service_is_initialized)
    {
        reading->is_valid = false;
        return MOTION_SERVICE_STATUS_NOT_READY;
    }

    uint32_t now_ms = runtime_clock_now_ms();
    if (motion_service_has_sampled)
    {
        uint32_t elapsed_ms = (uint32_t)(now_ms - motion_service_last_sample_ms);
        if (elapsed_ms < APP_MOTION_SAMPLE_PERIOD_MS)
        {
            reading->is_valid = false;
            reading->pending_events = motion_service_pending_events;
            return MOTION_SERVICE_STATUS_NOT_DUE;
        }
    }

    motion_service_last_sample_ms = now_ms;
    motion_service_has_sampled = true;

    motion_raw_sample_t raw_sample;
    motion_port_status_t port_status = motion_port_read_acceleration(&raw_sample);
    if (port_status == MOTION_PORT_STATUS_ID_MISMATCH)
    {
        reading->is_valid = false;
        reading->pending_events = motion_service_pending_events;
        return MOTION_SERVICE_STATUS_ID_MISMATCH;
    }
    if (port_status == MOTION_PORT_STATUS_NO_ACK)
    {
        reading->is_valid = false;
        reading->pending_events = motion_service_pending_events;
        return MOTION_SERVICE_STATUS_NO_ACK;
    }
    if (port_status != MOTION_PORT_STATUS_OK)
    {
        reading->is_valid = false;
        reading->pending_events = motion_service_pending_events;
        return MOTION_SERVICE_STATUS_BUS_ERROR;
    }

    motion_processor_status_t processor_status = motion_processor_process(
        raw_sample.accel_x_raw,
        raw_sample.accel_y_raw,
        raw_sample.accel_z_raw,
        now_ms,
        &reading->sample);
    ++motion_service_sample_sequence;
    reading->sample_sequence = motion_service_sample_sequence;

    if (processor_status == MOTION_PROCESSOR_STATUS_INVALID_ARG)
    {
        reading->is_valid = false;
        reading->pending_events = motion_service_pending_events;
        return MOTION_SERVICE_STATUS_INVALID_ARG;
    }

    reading->is_valid = true;
    if (processor_status == MOTION_PROCESSOR_STATUS_CALIBRATING)
    {
        reading->pending_events = motion_service_pending_events;
        return MOTION_SERVICE_STATUS_CALIBRATING;
    }

    motion_service_latch_event(reading->sample.event);
    reading->pending_events = motion_service_pending_events;
    return MOTION_SERVICE_STATUS_OK;
}

void motion_service_acknowledge_event(void)
{
    motion_service_pending_events = MOTION_EVENT_NONE;
}
