/* Portable MPU6050 configuration and big-endian burst decoding. */
#include "mpu6050.h"

#include "app_config.h"

#include <stddef.h>
#include <stdint.h>

#define MPU6050_BURST_BYTES 14U
#define MPU6050_REGISTER_COUNT 4U

/**
 * @brief Checks required I/O callbacks and the legal seven-bit address range.
 *
 * @param[in] io Caller-supplied bus operations.
 * @param[in] address_7bit Unshifted I2C address.
 *
 * @return true when every required callback and address is valid.
 */
static bool mpu6050_io_is_valid(const mpu6050_io_t *io, uint8_t address_7bit)
{
    return (io != NULL) &&
           (io->write_register != NULL) &&
           (io->read_register != NULL) &&
           (io->delay_ms != NULL) &&
           (address_7bit <= 0x7FU);
}

/**
 * @brief Converts two consecutive big-endian bytes to a signed sensor count.
 *
 * @param[in] most_significant High byte.
 * @param[in] least_significant Low byte.
 *
 * @return Signed two's-complement measurement.
 */
static int16_t mpu6050_decode_signed_word(uint8_t most_significant,
                                          uint8_t least_significant)
{
    uint16_t unsigned_word = (uint16_t)(((uint16_t)most_significant << 8U) |
                                        (uint16_t)least_significant);
    int32_t signed_value = (unsigned_word <= INT16_MAX)
                               ? (int32_t)unsigned_word
                               : (int32_t)unsigned_word - 65536;
    return (int16_t)signed_value;
}

mpu6050_status_t mpu6050_init(mpu6050_t *device,
                              const mpu6050_io_t *io,
                              uint8_t address_7bit,
                              uint8_t sample_rate_divider,
                              uint8_t dlpf_configuration)
{
    if ((device == NULL) || !mpu6050_io_is_valid(io, address_7bit) ||
        (address_7bit != APP_MOTION_I2C_ADDRESS_7BIT) ||
        (sample_rate_divider != APP_MOTION_SAMPLE_RATE_DIVIDER_VALUE) ||
        (dlpf_configuration > 6U))
    {
        return MPU6050_STATUS_INVALID_ARG;
    }

    device->address_7bit = address_7bit;
    device->who_am_i = 0U;
    device->is_initialized = false;

    uint8_t who_am_i = 0U;
    mpu6050_status_t bus_status = io->read_register(io->context,
                                                    address_7bit,
                                                    APP_MOTION_REGISTER_WHO_AM_I,
                                                    &who_am_i,
                                                    1U);
    if (bus_status == MPU6050_STATUS_NO_ACK)
    {
        return MPU6050_STATUS_NO_ACK;
    }
    if (bus_status != MPU6050_STATUS_OK)
    {
        return MPU6050_STATUS_BUS_ERROR;
    }
    if ((who_am_i & 0x7EU) != (APP_MOTION_WHO_AM_I_EXPECTED & 0x7EU))
    {
        device->who_am_i = who_am_i;
        return MPU6050_STATUS_ID_MISMATCH;
    }

    uint8_t wake_value = (uint8_t)(APP_MOTION_CLOCK_SELECT_X_GYRO & 0x07U);
    if (io->write_register(io->context,
                           address_7bit,
                           APP_MOTION_REGISTER_POWER_MANAGEMENT_1,
                           &wake_value,
                           1U) != MPU6050_STATUS_OK)
    {
        return MPU6050_STATUS_BUS_ERROR;
    }
    io->delay_ms(io->context, APP_MOTION_POWER_UP_DELAY_MS);

    uint8_t divider_value = APP_MOTION_SAMPLE_RATE_DIVIDER_VALUE;
    uint8_t config_value = (uint8_t)(dlpf_configuration & 0x07U);
    uint8_t gyro_config_value = (uint8_t)((APP_MOTION_GYRO_RANGE_SELECTION & 0x03U) << 3U);
    uint8_t accel_config_value = (uint8_t)((APP_MOTION_ACCEL_RANGE_SELECTION & 0x03U) << 3U);
    const uint8_t register_addresses[MPU6050_REGISTER_COUNT] =
    {
        APP_MOTION_REGISTER_CONFIGURATION,
        APP_MOTION_REGISTER_SAMPLE_RATE_DIVIDER,
        APP_MOTION_REGISTER_GYRO_CONFIGURATION,
        APP_MOTION_REGISTER_ACCEL_CONFIGURATION
    };
    const uint8_t register_values[MPU6050_REGISTER_COUNT] =
    {
        config_value,
        divider_value,
        gyro_config_value,
        accel_config_value
    };
    uint8_t register_readback[MPU6050_REGISTER_COUNT] = {0U};

    for (size_t index = 0U; index < MPU6050_REGISTER_COUNT; ++index)
    {
        if (io->write_register(io->context,
                               address_7bit,
                               register_addresses[index],
                               &register_values[index],
                               1U) != MPU6050_STATUS_OK)
        {
            return MPU6050_STATUS_BUS_ERROR;
        }
    }

    for (size_t index = 0U; index < MPU6050_REGISTER_COUNT; ++index)
    {
        if (io->read_register(io->context,
                              address_7bit,
                              register_addresses[index],
                              &register_readback[index],
                              1U) != MPU6050_STATUS_OK)
        {
            return MPU6050_STATUS_BUS_ERROR;
        }
        if (register_readback[index] != register_values[index])
        {
            return MPU6050_STATUS_CONFIG_MISMATCH;
        }
    }

    device->who_am_i = who_am_i;
    device->is_initialized = true;
    return MPU6050_STATUS_OK;
}

mpu6050_status_t mpu6050_read_motion(const mpu6050_t *device,
                                     const mpu6050_io_t *io,
                                     mpu6050_raw_sample_t *sample)
{
    if ((device == NULL) || (sample == NULL) ||
        !mpu6050_io_is_valid(io, device->address_7bit))
    {
        return MPU6050_STATUS_INVALID_ARG;
    }
    if (!device->is_initialized)
    {
        return MPU6050_STATUS_NOT_READY;
    }

    uint8_t burst[MPU6050_BURST_BYTES];
    if (io->read_register(io->context,
                          device->address_7bit,
                          APP_MOTION_REGISTER_ACCEL_XOUT_HIGH,
                          burst,
                          sizeof(burst)) != MPU6050_STATUS_OK)
    {
        return MPU6050_STATUS_BUS_ERROR;
    }

    sample->accel_x_raw = mpu6050_decode_signed_word(burst[0], burst[1]);
    sample->accel_y_raw = mpu6050_decode_signed_word(burst[2], burst[3]);
    sample->accel_z_raw = mpu6050_decode_signed_word(burst[4], burst[5]);
    sample->gyro_x_raw = mpu6050_decode_signed_word(burst[8], burst[9]);
    sample->gyro_y_raw = mpu6050_decode_signed_word(burst[10], burst[11]);
    sample->gyro_z_raw = mpu6050_decode_signed_word(burst[12], burst[13]);
    return MPU6050_STATUS_OK;
}
