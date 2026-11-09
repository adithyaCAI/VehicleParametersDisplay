/* Portable alert hold and blink-phase policy for motion event indications. */
#include "alert_indicator.h"

#include "app_config.h"

#include <stddef.h>

/**
 * @brief Returns the priority rank used to retain the strongest active event.
 *
 * @param[in] event Motion event identifier.
 *
 * @return Lower values represent higher priority; unknown events have no rank.
 */
static uint8_t alert_indicator_priority(motion_event_t event)
{
    switch (event)
    {
        case MOTION_EVENT_SUDDEN_BRAKING:
            return 0U;
        case MOTION_EVENT_UNSEEN_SPEED_BREAKER:
            return 1U;
        case MOTION_EVENT_WIDE_TURN:
            return 2U;
        case MOTION_EVENT_SPEEDING:
            return 3U;
        case MOTION_EVENT_NONE:
        default:
            return UINT8_MAX;
    }
}

void alert_indicator_init(alert_indicator_state_t *state)
{
    if (state != NULL)
    {
        state->last_event_ms = 0U;
        state->indication_start_ms = 0U;
        state->active_event = MOTION_EVENT_NONE;
        state->is_active = false;
    }
}

alert_indicator_output_t alert_indicator_update(alert_indicator_state_t *state,
                                                uint32_t now_ms,
                                                motion_event_t event)
{
    alert_indicator_output_t output =
    {
        .active_event = MOTION_EVENT_NONE,
        .event_is_active = false,
        .led_is_on = false
    };

    if (state == NULL)
    {
        return output;
    }

    if (event != MOTION_EVENT_NONE)
    {
        if (!state->is_active)
        {
            state->indication_start_ms = now_ms;
        }
        if (!state->is_active ||
            (alert_indicator_priority(event) <
             alert_indicator_priority(state->active_event)))
        {
            state->active_event = event;
        }
        state->last_event_ms = now_ms;
        state->is_active = true;
    }

    if (state->is_active &&
        ((uint32_t)(now_ms - state->last_event_ms) >= APP_ALERT_HOLD_AFTER_EVENT_MS))
    {
        state->active_event = MOTION_EVENT_NONE;
        state->is_active = false;
    }

    output.event_is_active = state->is_active;
    output.active_event = state->active_event;
    if (state->is_active)
    {
        uint32_t indication_elapsed_ms =
            (uint32_t)(now_ms - state->indication_start_ms);
        uint32_t blink_phase_ms = indication_elapsed_ms % APP_ALERT_LED_CYCLE_MS;
        output.led_is_on = (blink_phase_ms < APP_ALERT_LED_ON_MS);
    }

    return output;
}
