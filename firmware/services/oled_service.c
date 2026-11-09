/* SH1106 dashboard assembly, threshold markers, event labels and paced service retries. */
#include "oled_service.h"

#include "app_config.h"
#include "oled_port.h"
#include "sh1106.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static sh1106_t oled_service_display;
static sh1106_io_t oled_service_io;
static oled_dashboard_data_t oled_service_last_data;
static oled_dashboard_data_t oled_service_staged_data;
static bool oled_service_has_last_data;

/**
 * @brief Adapts the SH1106 callbacks to the platform's shared I2C OLED port.
 *
 * @param[in] context Unused service context.
 * @param[in] address_7bit Unshifted display address.
 * @param[in] control_byte SH1106 command/data control byte.
 * @param[in] data Borrowed command or data payload.
 * @param[in] length Payload length in bytes.
 *
 * @return SH1106-compatible result for the platform transfer.
 */
static sh1106_status_t oled_service_port_write(void *context,
                                               uint8_t address_7bit,
                                               uint8_t control_byte,
                                               const uint8_t *data,
                                               size_t length)
{
    (void)context;
    oled_port_status_t port_status = oled_port_write(address_7bit,
                                                     control_byte,
                                                     data,
                                                     length);
    switch (port_status)
    {
        case OLED_PORT_STATUS_OK:
            return SH1106_STATUS_OK;
        case OLED_PORT_STATUS_INVALID_ARG:
            return SH1106_STATUS_INVALID_ARG;
        case OLED_PORT_STATUS_BUSY:
            return SH1106_STATUS_BUSY;
        case OLED_PORT_STATUS_NOT_READY:
            return SH1106_STATUS_NOT_READY;
        case OLED_PORT_STATUS_NO_ACK:
            return SH1106_STATUS_NO_ACK;
        case OLED_PORT_STATUS_IO_ERROR:
        default:
            return SH1106_STATUS_IO_ERROR;
    }
}

/**
 * @brief Compares only dashboard-visible fields to detect meaningful changes.
 *
 * @param[in] left First current dashboard snapshot.
 * @param[in] right Second dashboard snapshot.
 *
 * @return true when the visible readings or event state differ.
 */
static bool oled_service_data_changed(const oled_dashboard_data_t *left,
                                      const oled_dashboard_data_t *right)
{
    return (left->temperature_tenths_c != right->temperature_tenths_c) ||
           (left->humidity_tenths_percent != right->humidity_tenths_percent) ||
           (left->x_millig != right->x_millig) ||
           (left->y_millig != right->y_millig) ||
           (left->z_millig != right->z_millig) ||
           (left->active_event != right->active_event) ||
           (left->climate_is_valid != right->climate_is_valid) ||
           (left->motion_is_calibrated != right->motion_is_calibrated);
}

/**
 * @brief Writes temperature and humidity values with independent threshold markers.
 *
 * @param[in] data Current dashboard readings.
 */
static void oled_service_draw_climate(const oled_dashboard_data_t *data)
{
    char text[APP_OLED_TEXT_COLUMNS + 1U];
    if (data->climate_is_valid)
    {
        int temperature_integer = (int)(data->temperature_tenths_c / 10);
        int temperature_fraction = (int)(data->temperature_tenths_c % 10);
        if (temperature_fraction < 0)
        {
            temperature_fraction = -temperature_fraction;
        }
        int temperature_high =
            (data->temperature_tenths_c > APP_ALERT_TEMPERATURE_HIGH_TENTHS_C) ? 1 : 0;
        if (data->temperature_tenths_c < 0)
        {
            (void)snprintf(text,
                           sizeof(text),
                           "Temp:-%d.%dC%s",
                           -temperature_integer,
                           temperature_fraction,
                           (temperature_high != 0) ? "!" : "");
        }
        else
        {
            (void)snprintf(text,
                           sizeof(text),
                           "Temp:%d.%dC%s",
                           temperature_integer,
                           temperature_fraction,
                           (temperature_high != 0) ? "!" : "");
        }
    }
    else
    {
        (void)snprintf(text, sizeof(text), "Temp:--.-C");
    }
    (void)sh1106_draw_text(&oled_service_display, 2U, 0U, text);

    if (data->climate_is_valid)
    {
        int humidity_high =
            (data->humidity_tenths_percent > APP_ALERT_HUMIDITY_HIGH_TENTHS_PERCENT)
                ? 1
                : 0;
        (void)snprintf(text,
                       sizeof(text),
                       "Humidity:%u.%u%%%s",
                       (unsigned int)(data->humidity_tenths_percent / 10U),
                       (unsigned int)(data->humidity_tenths_percent % 10U),
                       (humidity_high != 0) ? "!" : "");
    }
    else
    {
        (void)snprintf(text, sizeof(text), "Humidity:--.-%%");
    }
    (void)sh1106_draw_text(&oled_service_display, 3U, 0U, text);
}

/**
 * @brief Draws a calibrated axis in milli-g or an unavailable marker.
 *
 * @param[in] text Destination text buffer.
 * @param[in] capacity Destination capacity in bytes.
 * @param[in] label One-character axis label.
 * @param[in] value_millig Axis value in milli-g.
 * @param[in] is_valid Whether motion calibration has completed.
 */
static void oled_service_format_axis(char *text,
                                     size_t capacity,
                                     char label,
                                     int16_t value_millig,
                                     bool is_valid)
{
    if (is_valid)
    {
        int integer_g = (int)(value_millig / 1000);
        int fraction_millig = (int)(value_millig % 1000);
        if (fraction_millig < 0)
        {
            fraction_millig = -fraction_millig;
        }
        (void)snprintf(text,
                       capacity,
                       "%c:%d.%02dg",
                       label,
                       integer_g,
                       fraction_millig / 10);
    }
    else
    {
        (void)snprintf(text, capacity, "%c:--.--g", label);
    }
}

/**
 * @brief Stages all dashboard text, retaining climate rows while events are shown.
 *
 * @param[in] data Dashboard values and validity flags.
 */
static void oled_service_render(const oled_dashboard_data_t *data)
{
    char text[APP_OLED_TEXT_COLUMNS + 1U];
    (void)sh1106_clear(&oled_service_display);
    (void)sh1106_draw_text(&oled_service_display, 0U, 0U, "Vehicle Parameters");
    (void)sh1106_draw_text(&oled_service_display, 1U, 0U, "Display");
    oled_service_draw_climate(data);

    oled_service_format_axis(text,
                             sizeof(text),
                             'X',
                             data->x_millig,
                             data->motion_is_calibrated);
    (void)sh1106_draw_text(&oled_service_display, 4U, 0U, text);
    oled_service_format_axis(text,
                             sizeof(text),
                             'Y',
                             data->y_millig,
                             data->motion_is_calibrated);
    (void)sh1106_draw_text(&oled_service_display, 5U, 0U, text);
    oled_service_format_axis(text,
                             sizeof(text),
                             'Z',
                             data->z_millig,
                             data->motion_is_calibrated);
    (void)sh1106_draw_text(&oled_service_display, 6U, 0U, text);

    const char *event_name = motion_processor_event_name(data->active_event);
    if (event_name[0] != '\0')
    {
        (void)sh1106_draw_text(&oled_service_display, 7U, 0U, event_name);
    }
}

/**
 * @brief Initializes the module-specific I2C link and stages the first dashboard.
 *
 * @param[in,out] state OLED service state.
 * @param[in] data First available sensor snapshot.
 * @param[in] now_ms Current time for retry scheduling.
 *
 * @return Service status mapped from the SH1106 initialization operation.
 */
static oled_service_status_t oled_service_start(oled_service_state_t *state,
                                                const oled_dashboard_data_t *data,
                                                uint32_t now_ms)
{
    oled_service_io.context = NULL;
    oled_service_io.write = oled_service_port_write;
    sh1106_status_t driver_status = sh1106_init(&oled_service_display,
                                                &oled_service_io,
                                                APP_OLED_I2C_ADDRESS_7BIT);
    state->next_retry_ms = now_ms + APP_OLED_RETRY_INTERVAL_MS;
    if (driver_status != SH1106_STATUS_OK)
    {
        if (driver_status == SH1106_STATUS_BUSY)
        {
            return OLED_SERVICE_STATUS_BUSY;
        }
        if (driver_status == SH1106_STATUS_NO_ACK)
        {
            return OLED_SERVICE_STATUS_NO_ACK;
        }
        if (driver_status == SH1106_STATUS_NOT_READY)
        {
            return OLED_SERVICE_STATUS_NOT_READY;
        }
        return OLED_SERVICE_STATUS_IO_ERROR;
    }

    state->is_initialized = true;
    state->text_row = 0U;
    oled_service_render(data);
    oled_service_staged_data = *data;
    oled_service_last_data = *data;
    oled_service_has_last_data = true;
    state->has_drawn_dashboard = true;
    state->has_pending_frame = true;
    state->last_refresh_ms = now_ms;
    return OLED_SERVICE_STATUS_OK;
}

void oled_service_init_state(oled_service_state_t *state)
{
    if (state != NULL)
    {
        state->is_initialized = false;
        state->has_pending_frame = false;
        state->has_drawn_dashboard = false;
        state->last_refresh_ms = 0U;
        state->next_retry_ms = 0U;
        state->text_row = 0U;
    }
    oled_service_has_last_data = false;
}

oled_service_status_t oled_service_refresh(oled_service_state_t *state,
                                           const oled_dashboard_data_t *data,
                                           uint32_t now_ms)
{
    if ((state == NULL) || (data == NULL))
    {
        return OLED_SERVICE_STATUS_INVALID_ARG;
    }

    if (!state->is_initialized)
    {
        if ((state->next_retry_ms != 0U) &&
            ((int32_t)(now_ms - state->next_retry_ms) < 0))
        {
            return OLED_SERVICE_STATUS_NOT_DUE;
        }
        return oled_service_start(state, data, now_ms);
    }

    bool data_changed = !oled_service_has_last_data ||
                        oled_service_data_changed(data, &oled_service_last_data);
    if (data_changed && !state->has_pending_frame)
    {
        oled_service_render(data);
        oled_service_staged_data = *data;
        oled_service_last_data = *data;
        oled_service_has_last_data = true;
        state->has_pending_frame = true;
    }

    uint32_t elapsed_ms = (uint32_t)(now_ms - state->last_refresh_ms);
    if (elapsed_ms < APP_OLED_REFRESH_INTERVAL_MS)
    {
        return OLED_SERVICE_STATUS_NOT_DUE;
    }

    sh1106_status_t driver_status = sh1106_flush_step(&oled_service_display,
                                                      &oled_service_io);
    state->last_refresh_ms = now_ms;
    if (driver_status == SH1106_STATUS_OK)
    {
        state->has_pending_frame = oled_service_display.has_pending_frame;
        if (!state->has_pending_frame)
        {
            state->has_drawn_dashboard = true;
        }
        else if ((data->active_event != MOTION_EVENT_NONE) &&
                 (data->active_event != oled_service_staged_data.active_event))
        {
            (void)sh1106_draw_text(&oled_service_display, 7U, 0U,
                                   motion_processor_event_name(data->active_event));
            oled_service_staged_data.active_event = data->active_event;
            oled_service_last_data.active_event = data->active_event;
            oled_service_display.flush_event_page_first = true;
            oled_service_display.flush_event_page_offset = 0U;
            oled_service_display.next_framebuffer_byte = 0U;
            oled_service_display.flush_phase = 0U;
        }
        return OLED_SERVICE_STATUS_OK;
    }

    if (driver_status == SH1106_STATUS_BUSY)
    {
        return OLED_SERVICE_STATUS_BUSY;
    }

    state->is_initialized = false;
    state->has_pending_frame = false;
    state->next_retry_ms = now_ms + APP_OLED_RETRY_INTERVAL_MS;
    if (driver_status == SH1106_STATUS_NO_ACK)
    {
        return OLED_SERVICE_STATUS_NO_ACK;
    }
    if (driver_status == SH1106_STATUS_NOT_READY)
    {
        return OLED_SERVICE_STATUS_NOT_READY;
    }
    return OLED_SERVICE_STATUS_IO_ERROR;
}
