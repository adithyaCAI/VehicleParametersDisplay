/* Vendor-neutral log output contract implemented by a platform adapter. */
#ifndef LOGGER_PLATFORM_H
#define LOGGER_PLATFORM_H

#include <stdint.h>

/**
 * @brief Writes a formatted log record to the configured platform output.
 *
 * The buffer is borrowed during this call and remains owned by the caller.
 *
 * @param[in] data Log bytes to write; must not be NULL when length is nonzero.
 * @param[in] length Number of bytes to write.
 *
 * @return Zero when the record was accepted; nonzero on output failure.
 */
int32_t logger_platform_write(const uint8_t *data, uint16_t length);

#endif /* LOGGER_PLATFORM_H */
