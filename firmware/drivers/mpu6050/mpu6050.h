/* Portable MPU6050 register-level driver using a caller-supplied bus interface. */
#ifndef MPU6050_H
#define MPU6050_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    MPU6050_STATUS_OK = 0,
    MPU6050_STATUS_INVALID_ARG,
    MPU6050_STATUS_NOT_READY,
    MPU6050_STATUS_BUS_ERROR,
    MPU6050_STATUS_ID_MISMATCH,
    MPU6050_STATUS_CONFIG_MISMATCH,
    MPU6050_STATUS_NO_ACK
} mpu6050_status_t;

typedef struct
{
    void *context;
    mpu6050_status_t (*write_register)(void *context,
                                        uint8_t address_7bit,
                                        uint8_t register_address,
                                        const uint8_t *data,
                                        size_t length);
    mpu6050_status_t (*read_register)(void *context,
                                      uint8_t address_7bit,
                                      uint8_t register_address,
                                      uint8_t *data,
                                      size_t length);
    void (*delay_ms)(void *context, uint32_t delay_ms);
} mpu6050_io_t;

typedef struct
{
    uint8_t address_7bit;
    uint8_t who_am_i;
    bool is_initialized;
} mpu6050_t;

typedef struct
{
    int16_t accel_x_raw;
    int16_t accel_y_raw;
    int16_t accel_z_raw;
    int16_t gyro_x_raw;
    int16_t gyro_y_raw;
    int16_t gyro_z_raw;
} mpu6050_raw_sample_t;

/**
 * @brief Initializes an MPU6050 and verifies its motion sampling configuration.
 *
 * Reads WHO_AM_I, wakes the sensor with its X-gyro PLL clock, configures DLPF,
 * sample divider, ±250 °/s gyro range and ±4 g accelerometer range, and reads
 * the configuration registers back before marking the device ready. The register
 * addresses, identity value and sensitivity are grounded in sources/MPU-6000.PDF,
 * revision 4.2, sections 3, 4.2–4.5 and 4.32.
 *
 * @param[in,out] device Device state to initialize; must not be NULL.
 * @param[in] io Caller-owned bus callbacks; all callbacks must be set.
 * @param[in] address_7bit Unshifted MPU6050 address; AD0-low is 0x68.
 * @param[in] sample_rate_divider SMPLRT_DIV register value; for 100 Hz with
 *                                DLPF enabled, use 9 (1 kHz / (1 + 9)).
 * @param[in] dlpf_configuration CONFIG DLPF_CFG field, 0..6.
 *
 * @return MPU6050_STATUS_OK when identity and configuration readback succeed.
 * @return MPU6050_STATUS_INVALID_ARG for null/missing callbacks or invalid setup.
 * @return MPU6050_STATUS_BUS_ERROR when a bus transaction fails.
 * @return MPU6050_STATUS_ID_MISMATCH when WHO_AM_I is not the expected 0x68.
 * @return MPU6050_STATUS_CONFIG_MISMATCH when a configuration register differs.
 */
mpu6050_status_t mpu6050_init(mpu6050_t *device,
                              const mpu6050_io_t *io,
                              uint8_t address_7bit,
                              uint8_t sample_rate_divider,
                              uint8_t dlpf_configuration);

/**
 * @brief Reads one coherent 14-byte accelerometer/temperature/gyroscope burst.
 *
 * The returned motion axes are signed big-endian counts; the temperature bytes
 * in the burst are skipped. The destination is updated only after a complete read.
 *
 * @param[in] device Initialized MPU6050 context.
 * @param[in] io Bus callback table used for transactions.
 * @param[out] sample Raw motion sample destination; must not be NULL.
 *
 * @return MPU6050_STATUS_OK when the entire burst is read.
 * @return MPU6050_STATUS_INVALID_ARG for invalid pointers or callback table.
 * @return MPU6050_STATUS_NOT_READY before successful initialization.
 * @return MPU6050_STATUS_BUS_ERROR when the shared I2C transfer fails.
 */
mpu6050_status_t mpu6050_read_motion(const mpu6050_t *device,
                                     const mpu6050_io_t *io,
                                     mpu6050_raw_sample_t *sample);

#endif /* MPU6050_H */
