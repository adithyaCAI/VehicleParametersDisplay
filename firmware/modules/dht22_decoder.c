/* Vendor-neutral DHT22 frame validation and value conversion. */
#include "dht22_decoder.h"

#include <stddef.h>
#include <stdint.h>

#define DHT22_DECODER_FRAME_BYTES 5U
#define DHT22_DECODER_MAX_HUMIDITY_TENTHS 1000U
#define DHT22_DECODER_MIN_TEMPERATURE_TENTHS (-400)
#define DHT22_DECODER_MAX_TEMPERATURE_TENTHS 800

/**
 * @brief Converts two big-endian bytes into an unsigned 16-bit sensor value.
 *
 * @param[in] most_significant Most significant byte.
 * @param[in] least_significant Least significant byte.
 *
 * @return Combined 16-bit value.
 */
static uint16_t dht22_decoder_combine_bytes(uint8_t most_significant,
                                            uint8_t least_significant)
{
    return (uint16_t)(((uint16_t)most_significant << 8U) | least_significant);
}

dht22_decoder_status_t dht22_decoder_decode(const uint8_t *frame,
                                            size_t frame_length,
                                            int16_t *temperature_tenths_c,
                                            uint16_t *humidity_tenths_percent)
{
    if ((frame == NULL) || (temperature_tenths_c == NULL) ||
        (humidity_tenths_percent == NULL) ||
        (frame_length != DHT22_DECODER_FRAME_BYTES))
    {
        return DHT22_DECODER_STATUS_INVALID_ARG;
    }

    uint8_t checksum = (uint8_t)(frame[0] + frame[1] + frame[2] + frame[3]);
    if (checksum != frame[4])
    {
        return DHT22_DECODER_STATUS_CHECKSUM_ERROR;
    }

    uint16_t humidity = dht22_decoder_combine_bytes(frame[0], frame[1]);
    uint16_t temperature_magnitude = dht22_decoder_combine_bytes(
        (uint8_t)(frame[2] & 0x7FU), frame[3]);
    int16_t temperature = (int16_t)temperature_magnitude;
    if ((frame[2] & 0x80U) != 0U)
    {
        temperature = (int16_t)-temperature;
    }

    if ((humidity > DHT22_DECODER_MAX_HUMIDITY_TENTHS) ||
        (temperature < DHT22_DECODER_MIN_TEMPERATURE_TENTHS) ||
        (temperature > DHT22_DECODER_MAX_TEMPERATURE_TENTHS))
    {
        return DHT22_DECODER_STATUS_OUT_OF_RANGE;
    }

    *temperature_tenths_c = temperature;
    *humidity_tenths_percent = humidity;
    return DHT22_DECODER_STATUS_OK;
}
