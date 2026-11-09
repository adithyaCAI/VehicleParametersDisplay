/* Portable timed LED indication and event-message hold policy. */
#ifndef ALERT_INDICATOR_H
#define ALERT_INDICATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "motion_processor.h"

typedef struct
{
    uint32_t last_event_ms;
    uint32_t indication_start_ms;
    motion_event_t active_event;
    bool is_active;
} alert_indicator_state_t;

typedef struct
{
    motion_event_t active_event;
    bool event_is_active;
    bool led_is_on;
} alert_indicator_output_t;

/**
 * @brief Resets event hold and blink-phase state.
 *
 * @param[out] state Alert state to initialize; NULL is ignored.
 */
void alert_indicator_init(alert_indicator_state_t *state);

/**
 * @brief Updates event priority, post-event hold, and 500 ms blink phase.
 *
 * A new event extends the active interval by the configured hold time. Higher
 * priority events remain selected while the current indication is active.
 * Unsigned elapsed-time arithmetic remains valid across tick-counter wrap.
 *
 * @param[in,out] state Alert state initialized by alert_indicator_init(); NULL
 *                      produces an inactive output.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] event Newly detected event, or MOTION_EVENT_NONE.
 *
 * @return Current event-message and LED output state.
 */
alert_indicator_output_t alert_indicator_update(alert_indicator_state_t *state,
                                                uint32_t now_ms,
                                                motion_event_t event);

#endif /* ALERT_INDICATOR_H */
