/* Vendor-neutral contract for the serialized shared I2C bus. */
#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    I2C_BUS_STATUS_OK = 0,
    I2C_BUS_STATUS_INVALID_ARG,
    I2C_BUS_STATUS_BUSY,
    I2C_BUS_STATUS_NOT_FOUND,
    I2C_BUS_STATUS_IO_ERROR,
    I2C_BUS_STATUS_NOT_READY
} i2c_bus_status_t;

/**
 * @brief Initializes the shared I2C bus for 7-bit master communication.
 *
 * Repeated successful calls leave the initialized bus unchanged.
 *
 * @return I2C_BUS_STATUS_OK when initialized or already ready.
 * @return I2C_BUS_STATUS_IO_ERROR when platform initialization fails.
 */
i2c_bus_status_t i2c_bus_init(void);

/**
 * @brief Sends bytes to a 7-bit target as one serialized transaction.
 *
 * The address is unshifted. The input buffer is borrowed for the duration of
 * this blocking call and is not modified. This function is not ISR-safe.
 *
 * @param[in] address_7bit Unshifted target address in the range 0x00..0x7F.
 * @param[in] data Borrowed bytes to transmit; must not be NULL.
 * @param[in] length Number of bytes; must be 1..UINT16_MAX.
 *
 * @return I2C_BUS_STATUS_OK when the transfer completes.
 * @return I2C_BUS_STATUS_INVALID_ARG for an invalid address, buffer, or length.
 * @return I2C_BUS_STATUS_BUSY when another bus transaction owns the bus.
 * @return I2C_BUS_STATUS_NOT_READY before successful initialization.
 * @return I2C_BUS_STATUS_NOT_FOUND when the target NACKs its address.
 * @return I2C_BUS_STATUS_IO_ERROR when the transfer fails for another reason.
 *
 * @pre i2c_bus_init() has completed successfully.
 */
i2c_bus_status_t i2c_bus_write(uint8_t address_7bit,
                               const uint8_t *data,
                               size_t length);

/**
 * @brief Writes bytes to an 8-bit register on one 7-bit target.
 *
 * The payload is borrowed and not modified. The address and payload transfer
 * are serialized as one blocking memory-write transaction.
 *
 * @param[in] address_7bit Unshifted target address in the range 0x00..0x7F.
 * @param[in] register_address 8-bit device register to write.
 * @param[in] data Borrowed register payload; must not be NULL.
 * @param[in] length Number of bytes; must be 1..UINT16_MAX.
 *
 * @return I2C_BUS_STATUS_OK when the transfer completes.
 * @return I2C_BUS_STATUS_INVALID_ARG for an invalid address, buffer, or length.
 * @return I2C_BUS_STATUS_BUSY when another bus transaction owns the bus.
 * @return I2C_BUS_STATUS_NOT_READY before successful initialization.
 * @return I2C_BUS_STATUS_NOT_FOUND when the target NACKs its address.
 * @return I2C_BUS_STATUS_IO_ERROR when the transfer fails for another reason.
 *
 * @pre i2c_bus_init() has completed successfully.
 */
i2c_bus_status_t i2c_bus_write_register(uint8_t address_7bit,
                                        uint8_t register_address,
                                        const uint8_t *data,
                                        size_t length);

/**
 * @brief Reads bytes from an 8-bit register on one 7-bit target.
 *
 * The register-address write and data read form one serialized blocking
 * operation. The destination buffer is modified only on success.
 *
 * @param[in] address_7bit Unshifted target address in the range 0x00..0x7F.
 * @param[in] register_address 8-bit device register to read.
 * @param[out] data Destination buffer; must not be NULL.
 * @param[in] length Number of bytes; must be 1..UINT16_MAX.
 *
 * @return I2C_BUS_STATUS_OK when the transfer completes.
 * @return I2C_BUS_STATUS_INVALID_ARG for an invalid address, buffer, or length.
 * @return I2C_BUS_STATUS_BUSY when another bus transaction owns the bus.
 * @return I2C_BUS_STATUS_NOT_READY before successful initialization.
 * @return I2C_BUS_STATUS_NOT_FOUND when the target NACKs its address.
 * @return I2C_BUS_STATUS_IO_ERROR when the transfer fails for another reason.
 *
 * @pre i2c_bus_init() has completed successfully.
 */
i2c_bus_status_t i2c_bus_read_register(uint8_t address_7bit,
                                       uint8_t register_address,
                                       uint8_t *data,
                                       size_t length);

/**
 * @brief Probes legal 7-bit target addresses from 0x08 through 0x77.
 *
 * A single bus lock covers the complete sweep. Acknowledging addresses are
 * copied in ascending order up to output_capacity; additional ACKs are counted
 * but not stored.
 *
 * @param[out] addresses Destination for unshifted addresses; may be NULL only
 *                       when output_capacity is zero.
 * @param[in] output_capacity Number of entries available in addresses.
 * @param[out] address_count Total number of acknowledging addresses.
 *
 * @return I2C_BUS_STATUS_OK when the complete sweep runs.
 * @return I2C_BUS_STATUS_INVALID_ARG for inconsistent output pointers/capacity.
 * @return I2C_BUS_STATUS_BUSY when another transaction owns the bus.
 * @return I2C_BUS_STATUS_NOT_READY before successful initialization.
 * @return I2C_BUS_STATUS_IO_ERROR when a probe fails for a reason other than NACK.
 *
 * @pre i2c_bus_init() has completed successfully.
 */
i2c_bus_status_t i2c_bus_scan(uint8_t *addresses,
                              size_t output_capacity,
                              size_t *address_count);

/**
 * @brief Returns the number of NACK responses from the most recent bus sweep.
 *
 * The count excludes ACKed targets and is reset at the start of each scan. It is
 * intended for diagnosing whether the completed scan reached the end of range.
 *
 * @return Number of NACK responses observed by the most recent completed scan.
 */
uint32_t i2c_bus_nack_probe_count_get(void);

#endif /* I2C_BUS_H */
