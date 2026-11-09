/* Portable DHT22 checksum, range validation, and raw-frame conversion. */
#ifndef DHT22_DECODER_H
#define DHT22_DECODER_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    DHT22_DECODER_STATUS_OK = 0,
    DHT22_DECODER_STATUS_INVALID_ARG,
    DHT22_DECODER_STATUS_CHECKSUM_ERROR,
    DHT22_DECODER_STATUS_OUT_OF_RANGE
} dht22_decoder_status_t;

/**
 * @brief Decodes one five-byte DHT22 response into tenths of engineering units.
 *
 * The wire frame is checksum-validated before output values are changed.
 *
 * @param[in] frame Five raw DHT22 bytes; must not be NULL.
 * @param[in] frame_length Must equal 5 bytes.
 * @param[out] temperature_tenths_c Temperature in tenths of degrees Celsius.
 * @param[out] humidity_tenths_percent Relative humidity in tenths of percent.
 *
 * @return DHT22_DECODER_STATUS_OK when checksum and sensor ranges are valid.
 * @return DHT22_DECODER_STATUS_INVALID_ARG for invalid pointers or frame size.
 * @return DHT22_DECODER_STATUS_CHECKSUM_ERROR for a checksum mismatch.
 * @return DHT22_DECODER_STATUS_OUT_OF_RANGE when values exceed DHT22 limits.
 */
dht22_decoder_status_t dht22_decoder_decode(const uint8_t *frame,
                                            size_t frame_length,
                                            int16_t *temperature_tenths_c,
                                            uint16_t *humidity_tenths_percent);

#endif /* DHT22_DECODER_H */
