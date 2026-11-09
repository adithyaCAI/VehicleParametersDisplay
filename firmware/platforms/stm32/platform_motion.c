/* STM32 shared-I2C adapter for MPU6050 register access and sensor bring-up. */
#include "motion_port.h"

#include "app_config.h"
#include "i2c_bus.h"
#include "logger.h"
#include "mpu6050.h"
#include "runtime_clock.h"
#include "stm32g0xx_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static const char *PLATFORM_MOTION_TAG = "mpu6050";
static mpu6050_t platform_motion_device;
static bool platform_motion_is_initialized;

/**
 * @brief Converts a shared I2C transaction result to the portable driver status.
 *
 * @param[in] bus_status Shared I2C bus result.
 *
 * @return MPU6050_STATUS_OK, INVALID_ARG, NOT_READY or BUS_ERROR.
 */
static mpu6050_status_t platform_motion_map_bus_status(i2c_bus_status_t bus_status)
{
    if (bus_status == I2C_BUS_STATUS_OK)
    {
        return MPU6050_STATUS_OK;
    }
    if (bus_status == I2C_BUS_STATUS_INVALID_ARG)
    {
        return MPU6050_STATUS_INVALID_ARG;
    }
    if (bus_status == I2C_BUS_STATUS_NOT_READY)
    {
        return MPU6050_STATUS_NOT_READY;
    }
    if (bus_status == I2C_BUS_STATUS_NOT_FOUND)
    {
        return MPU6050_STATUS_NO_ACK;
    }
    return MPU6050_STATUS_BUS_ERROR;
}

/**
 * @brief Writes one MPU6050 register through the shared PB8/PB9 bus lock.
 *
 * @param[in] context Unused adapter context.
 * @param[in] address_7bit Unshifted MPU6050 address.
 * @param[in] register_address Device register address.
 * @param[in] data Borrowed register payload.
 * @param[in] length Payload length in bytes.
 *
 * @return Driver status describing the shared-bus transaction result.
 */
static mpu6050_status_t platform_motion_write_register(void *context,
                                                       uint8_t address_7bit,
                                                       uint8_t register_address,
                                                       const uint8_t *data,
                                                       size_t length)
{
    (void)context;
    i2c_bus_status_t bus_status = i2c_bus_write_register(address_7bit,
                                                          register_address,
                                                          data,
                                                          length);
    return platform_motion_map_bus_status(bus_status);
}

/**
 * @brief Reads one or more MPU6050 registers through the shared bus lock.
 *
 * @param[in] context Unused adapter context.
 * @param[in] address_7bit Unshifted MPU6050 address.
 * @param[in] register_address First register address to read.
 * @param[out] data Destination buffer for register contents.
 * @param[in] length Number of bytes to read.
 *
 * @return Driver status describing the shared-bus transaction result.
 */
static mpu6050_status_t platform_motion_read_register(void *context,
                                                      uint8_t address_7bit,
                                                      uint8_t register_address,
                                                      uint8_t *data,
                                                      size_t length)
{
    (void)context;
    i2c_bus_status_t bus_status = i2c_bus_read_register(address_7bit,
                                                         register_address,
                                                         data,
                                                         length);
    return platform_motion_map_bus_status(bus_status);
}

/**
 * @brief Provides the portable driver with its bounded millisecond wait.
 *
 * @param[in] context Unused adapter context.
 * @param[in] delay_ms Wait duration in milliseconds.
 */
static void platform_motion_delay(void *context, uint32_t delay_ms)
{
    (void)context;
    runtime_clock_delay_ms(delay_ms);
}

/**
 * @brief Creates the callback table for the portable MPU6050 driver.
 *
 * @return Bus and delay operations owned by this STM32 adapter.
 */
static mpu6050_io_t platform_motion_make_io(void)
{
    mpu6050_io_t io =
    {
        .context = NULL,
        .write_register = platform_motion_write_register,
        .read_register = platform_motion_read_register,
        .delay_ms = platform_motion_delay
    };
    return io;
}

motion_port_status_t motion_port_init(void)
{
    if (platform_motion_is_initialized)
    {
        return MOTION_PORT_STATUS_OK;
    }

    i2c_bus_status_t bus_status = i2c_bus_init();
    if (bus_status != I2C_BUS_STATUS_OK)
    {
        return MOTION_PORT_STATUS_BUS_ERROR;
    }

    mpu6050_io_t io = platform_motion_make_io();
    mpu6050_status_t driver_status = mpu6050_init(
        &platform_motion_device,
        &io,
        APP_MOTION_I2C_ADDRESS_7BIT,
        APP_MOTION_SAMPLE_RATE_DIVIDER_VALUE,
        APP_MOTION_DLPF_CONFIGURATION);

    if (driver_status == MPU6050_STATUS_ID_MISMATCH)
    {
        LOGE(PLATFORM_MOTION_TAG, "WHO_AM_I=0x%02X; expected 0x%02X",
             (unsigned int)platform_motion_device.who_am_i,
             (unsigned int)APP_MOTION_WHO_AM_I_EXPECTED);
        return MOTION_PORT_STATUS_ID_MISMATCH;
    }
    if (driver_status == MPU6050_STATUS_NO_ACK)
    {
        LOGE(PLATFORM_MOTION_TAG, "configured I2C address 0x%02X did not ACK",
             (unsigned int)APP_MOTION_I2C_ADDRESS_7BIT);
        return MOTION_PORT_STATUS_NO_ACK;
    }
    if (driver_status != MPU6050_STATUS_OK)
    {
        LOGE(PLATFORM_MOTION_TAG, "MPU6050 init failed: status=%u",
             (unsigned int)driver_status);
        return MOTION_PORT_STATUS_BUS_ERROR;
    }

    platform_motion_is_initialized = true;
    LOGI(PLATFORM_MOTION_TAG,
         "ready addr=0x%02X WHO_AM_I=0x%02X accel=+/-4g rate=%uHz divider=%u",
         (unsigned int)platform_motion_device.address_7bit,
         (unsigned int)platform_motion_device.who_am_i,
         (unsigned int)APP_MOTION_SAMPLE_RATE_HZ,
         (unsigned int)APP_MOTION_SAMPLE_RATE_DIVIDER_VALUE);
    return MOTION_PORT_STATUS_OK;
}

motion_port_status_t motion_port_read_acceleration(motion_raw_sample_t *sample)
{
    if (sample == NULL)
    {
        return MOTION_PORT_STATUS_INVALID_ARG;
    }
    if (!platform_motion_is_initialized)
    {
        return MOTION_PORT_STATUS_NOT_READY;
    }

    mpu6050_io_t io = platform_motion_make_io();
    mpu6050_raw_sample_t raw_sample;
    mpu6050_status_t driver_status = mpu6050_read_motion(&platform_motion_device,
                                                         &io,
                                                         &raw_sample);
    if (driver_status != MPU6050_STATUS_OK)
    {
        LOGE(PLATFORM_MOTION_TAG, "acceleration burst read failed: status=%u",
             (unsigned int)driver_status);
        return MOTION_PORT_STATUS_BUS_ERROR;
    }

    sample->accel_x_raw = raw_sample.accel_x_raw;
    sample->accel_y_raw = raw_sample.accel_y_raw;
    sample->accel_z_raw = raw_sample.accel_z_raw;
    return MOTION_PORT_STATUS_OK;
}
