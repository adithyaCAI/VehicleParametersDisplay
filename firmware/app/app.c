/* App startup and UART/OLED telemetry for the shared-bus vehicle monitor. */
#include "app.h"

#include "app_config.h"
#include "alert_indicator.h"
#include "climate_service.h"
#include "i2c_bus.h"
#include "indicator_port.h"
#include "logger.h"
#include "motion_processor.h"
#include "motion_service.h"
#include "oled_service.h"
#include "platform_uart2.h"
#include "runtime_clock.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "firmgen_telemetry.h"
UART_HandleTypeDef *firmgen_uart_handle(void);   /* firmgen_telemetry */

static const char *TAG = "app";
static motion_service_reading_t app_motion_reading;
static alert_indicator_state_t app_alert_state;
static oled_service_state_t app_oled_state;
static motion_event_t app_new_event;
static climate_reading_t app_climate_reading;
static uint32_t app_last_motion_telemetry_ms;
static uint32_t app_last_motion_retry_ms;
static bool app_motion_ready;
static bool app_led_is_on;

/**
 * @brief Scans all legal I2C addresses and logs each responding device.
 */
static void app_report_i2c_scan(void)
{
    uint8_t acknowledged_addresses[112];
    size_t address_count = 0U;
    i2c_bus_status_t scan_status = i2c_bus_scan(acknowledged_addresses,
                                                sizeof(acknowledged_addresses),
                                                &address_count);
    if (scan_status != I2C_BUS_STATUS_OK)
    {
        LOGE(TAG, "I2C1 scan failed: status=%u", (unsigned int)scan_status);
        return;
    }

    LOGI(TAG, "I2C1 sweep complete: ACK count=%u, NACK count=%lu",
         (unsigned int)address_count,
         (unsigned long)i2c_bus_nack_probe_count_get());
    for (size_t index = 0U; index < address_count; ++index)
    {
        LOGI(TAG, "I2C1 ACK at 0x%02X", (unsigned int)acknowledged_addresses[index]);
    }
}

/**
 * @brief Emits calibrated, axis-mapped motion telemetry.
 */
static void app_report_motion(void)
{
    uint32_t now_ms = runtime_clock_now_ms();
    if ((uint32_t)(now_ms - app_last_motion_telemetry_ms) < APP_MOTION_TELEMETRY_PERIOD_MS)
    {
        return;
    }
    app_last_motion_telemetry_ms = now_ms;

    if (!app_motion_reading.sample.is_calibrated)
    {
        LOGI(TAG, "MOTION calibration in progress seq=%lu",
             (unsigned long)app_motion_reading.sample_sequence);
        return;
    }

    const char *event_name = motion_processor_event_name(app_alert_state.active_event);
    const char *event_text = (event_name[0] == '\0') ? "none" : event_name;
    LOGI(TAG, "MOTION seq=%lu lateral_x=%d longitudinal_y=%d vertical_z=%d milli-g event=%s",
         (unsigned long)app_motion_reading.sample_sequence,
         (int)app_motion_reading.sample.x_millig,
         (int)app_motion_reading.sample.y_millig,
         (int)app_motion_reading.sample.z_millig,
         event_text);

    char telemetry[144];
    int telemetry_length = snprintf(
        telemetry,
        sizeof(telemetry),
        "MOTION seq=%lu x_mg=%d y_mg=%d z_mg=%d calibrated=%u event=%s\r\n",
        (unsigned long)app_motion_reading.sample_sequence,
        (int)app_motion_reading.sample.x_millig,
        (int)app_motion_reading.sample.y_millig,
        (int)app_motion_reading.sample.z_millig,
        app_motion_reading.sample.is_calibrated ? 1U : 0U,
        event_text);
    if ((telemetry_length > 0) && ((size_t)telemetry_length < sizeof(telemetry)))
    {
        (void)platform_uart2_write((const uint8_t *)telemetry, (uint16_t)telemetry_length);
    }
}

/**
 * @brief Emits the latest valid DHT22 reading after its two-second cadence.
 */
static void app_report_climate(void)
{
    if (!app_climate_reading.is_valid)
    {
        LOGW(TAG, "CLIMATE unavailable; motion telemetry continues");
        return;
    }

    LOGI(TAG, "DHT22 temperature=%d.%d C humidity=%u.%u %%",
         (int)(app_climate_reading.temperature_tenths_c / 10),
         (int)(app_climate_reading.temperature_tenths_c < 0
                   ? -(app_climate_reading.temperature_tenths_c % 10)
                   : app_climate_reading.temperature_tenths_c % 10),
         (unsigned int)(app_climate_reading.humidity_tenths_percent / 10U),
         (unsigned int)(app_climate_reading.humidity_tenths_percent % 10U));

    char telemetry[96];
    int telemetry_length = snprintf(
        telemetry,
        sizeof(telemetry),
        "CLIMATE temperature_tenths_c=%d humidity_tenths_percent=%u\r\n",
        (int)app_climate_reading.temperature_tenths_c,
        (unsigned int)app_climate_reading.humidity_tenths_percent);
    if ((telemetry_length > 0) && ((size_t)telemetry_length < sizeof(telemetry)))
    {
        (void)platform_uart2_write((const uint8_t *)telemetry,
                                   (uint16_t)telemetry_length);
    }
}

/**
 * @brief Attempts motion service setup and schedules a bounded retry on failure.
 */
static void app_try_initialize_motion(void)
{
    motion_service_status_t motion_status = motion_service_init();
    app_last_motion_retry_ms = runtime_clock_now_ms();
    if (motion_status == MOTION_SERVICE_STATUS_OK)
    {
        app_motion_ready = true;
        LOGI(TAG, "MPU6050 configured at 100 Hz; one-second flat/still calibration starts now");
    }
    else
    {
        app_motion_ready = false;
        LOGE(TAG, "MPU6050 init status=%u; climate acquisition remains active",
             (unsigned int)motion_status);
    }
}

/**
 * @brief Builds one OLED input snapshot from the latest service-owned readings.
 *
 * @return Snapshot with validity flags preventing stale or uncalibrated display data.
 */
static oled_dashboard_data_t app_make_oled_snapshot(void)
{
    oled_dashboard_data_t snapshot =
    {
        .temperature_tenths_c = app_climate_reading.temperature_tenths_c,
        .humidity_tenths_percent = app_climate_reading.humidity_tenths_percent,
        .x_millig = app_motion_reading.sample.x_millig,
        .y_millig = app_motion_reading.sample.y_millig,
        .z_millig = app_motion_reading.sample.z_millig,
        .active_event = app_alert_state.active_event,
        .climate_is_valid = app_climate_reading.is_valid,
        .motion_is_calibrated = app_motion_reading.sample.is_calibrated
    };
    return snapshot;
}

/**
 * @brief Polls OLED rendering and leaves sensing active if the display is unavailable.
 */
static void app_poll_oled(void)
{
    oled_dashboard_data_t snapshot = app_make_oled_snapshot();
    oled_service_status_t display_status = oled_service_refresh(
        &app_oled_state,
        &snapshot,
        runtime_clock_now_ms());

    if ((display_status == OLED_SERVICE_STATUS_NO_ACK) ||
        (display_status == OLED_SERVICE_STATUS_IO_ERROR))
    {
        LOGW(TAG, "OLED unavailable: status=%u; sensor acquisition continues",
             (unsigned int)display_status);
    }
}

/**
 * @brief Polls motion, LED alert, climate, and one bounded OLED transfer step.
 */
static void app_poll_services(void)
{
    uint32_t now_ms = runtime_clock_now_ms();
    if (!app_motion_ready &&
        ((uint32_t)(now_ms - app_last_motion_retry_ms) >= APP_MOTION_RETRY_INTERVAL_MS))
    {
        app_try_initialize_motion();
    }

    if (app_motion_ready)
    {
        motion_service_status_t motion_status = motion_service_poll(&app_motion_reading);
        if ((motion_status == MOTION_SERVICE_STATUS_OK) ||
            (motion_status == MOTION_SERVICE_STATUS_CALIBRATING))
        {
            app_report_motion();
            if (app_motion_reading.pending_events != MOTION_EVENT_NONE)
            {
                app_new_event = app_motion_reading.pending_events;
                LOGW(TAG, "EVENT %s",
                     motion_processor_event_name(app_new_event));
                motion_service_acknowledge_event();
            }
        }
        else if ((motion_status != MOTION_SERVICE_STATUS_NOT_DUE) &&
                 (motion_status != MOTION_SERVICE_STATUS_NOT_READY))
        {
            LOGE(TAG, "MPU6050 acquisition failed: status=%u; retry scheduled",
                 (unsigned int)motion_status);
            app_motion_ready = false;
            app_last_motion_retry_ms = now_ms;
        }
    }

    alert_indicator_output_t alert_output = alert_indicator_update(
        &app_alert_state,
        runtime_clock_now_ms(),
        app_new_event);
    app_new_event = MOTION_EVENT_NONE;
    if (alert_output.led_is_on != app_led_is_on)
    {
        app_led_is_on = alert_output.led_is_on;
        if (indicator_port_set(app_led_is_on) != INDICATOR_PORT_STATUS_OK)
        {
            LOGE(TAG, "LD4 output update failed");
        }
    }

    climate_status_t climate_status = climate_service_poll(&app_climate_reading);
    if (climate_status == CLIMATE_STATUS_OK)
    {
        app_report_climate();
    }
    else if ((climate_status != CLIMATE_STATUS_NOT_DUE) &&
             (climate_status != CLIMATE_STATUS_NOT_READY))
    {
        app_climate_reading.is_valid = false;
        LOGE(TAG, "DHT22 read failed: status=%u", (unsigned int)climate_status);
    }

    app_poll_oled();
}

void app_start(void)
{
    firmgen_telemetry_start(firmgen_uart_handle());
    if (i2c_bus_init() != I2C_BUS_STATUS_OK)
    {
        LOGE(TAG, "I2C1 initialization failed");
        for (;;)
        {
        }
    }
    app_report_i2c_scan();

    climate_status_t climate_status = climate_service_init();
    if (climate_status != CLIMATE_STATUS_OK)
    {
        LOGE(TAG, "DHT22 initialization failed: status=%u", (unsigned int)climate_status);
        for (;;)
        {
        }
    }

    if (indicator_port_init() != INDICATOR_PORT_STATUS_OK)
    {
        LOGE(TAG, "LD4 initialization failed");
        for (;;)
        {
        }
    }
    alert_indicator_init(&app_alert_state);
    oled_service_init_state(&app_oled_state);
    app_new_event = MOTION_EVENT_NONE;
    app_led_is_on = false;

    app_motion_ready = false;
    app_last_motion_retry_ms = runtime_clock_now_ms();
    app_try_initialize_motion();
    app_last_motion_telemetry_ms = runtime_clock_now_ms();

    for (;;)
    {
        firmgen_telemetry_poll();
        app_poll_services();
        runtime_clock_delay_ms(APP_MOTION_IDLE_DELAY_MS);
    }
}
