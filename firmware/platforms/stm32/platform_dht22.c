/* STM32 TIM3/GPIO implementation of the DHT22 single-wire climate port. */
#include "platform_dht22.h"

#include "app_config.h"
#include "dht22_decoder.h"
#include "logger.h"
#include "runtime_clock.h"
#include "stm32g0xx_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static TIM_HandleTypeDef platform_dht22_timer;
static bool platform_dht22_is_initialized;
static const char *PLATFORM_DHT22_TAG = "dht22";

/**
 * @brief Changes PB4 between released input and open-drain low drive.
 *
 * The confirmed module pull-up returns the line high while the MCU pin is input.
 *
 * @param[in] drive_low true to pull the data line low; false to release it.
 */
static void platform_dht22_set_line_mode(bool drive_low)
{
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Pin = APP_DHT22_DATA_PIN;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

    if (drive_low)
    {
        gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
        HAL_GPIO_Init(APP_DHT22_GPIO_PORT, &gpio_init);
        HAL_GPIO_WritePin(APP_DHT22_GPIO_PORT, APP_DHT22_DATA_PIN, GPIO_PIN_RESET);
    }
    else
    {
        gpio_init.Mode = GPIO_MODE_INPUT;
        HAL_GPIO_Init(APP_DHT22_GPIO_PORT, &gpio_init);
    }
}

/**
 * @brief Waits for the data input to reach a level until a microsecond deadline.
 *
 * @param[in] level Required input level.
 * @param[in] timeout_us Maximum wait in microseconds.
 *
 * @return true if the requested level was observed before timeout.
 */
static bool platform_dht22_wait_level(GPIO_PinState level, uint32_t timeout_us)
{
    uint16_t start_ticks = (uint16_t)__HAL_TIM_GET_COUNTER(&platform_dht22_timer);
    while (HAL_GPIO_ReadPin(APP_DHT22_GPIO_PORT, APP_DHT22_DATA_PIN) != level)
    {
        uint16_t current_ticks = (uint16_t)__HAL_TIM_GET_COUNTER(&platform_dht22_timer);
        if ((uint16_t)(current_ticks - start_ticks) >= timeout_us)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Captures and classifies one DHT22 data bit.
 *
 * @param[out] bit_value Decoded bit (0 or 1), written only for valid pulse width.
 *
 * @return true when a valid data bit was captured; otherwise false.
 */
static bool platform_dht22_capture_bit(uint8_t *bit_value)
{
    if (bit_value == NULL)
    {
        return false;
    }
    if (!platform_dht22_wait_level(GPIO_PIN_SET, APP_DHT22_EDGE_TIMEOUT_US))
    {
        LOGE(PLATFORM_DHT22_TAG, "data bit rise timeout, PB4=%u",
             (unsigned int)HAL_GPIO_ReadPin(APP_DHT22_GPIO_PORT, APP_DHT22_DATA_PIN));
        return false;
    }

    uint16_t high_start_ticks = (uint16_t)__HAL_TIM_GET_COUNTER(&platform_dht22_timer);
    if (!platform_dht22_wait_level(GPIO_PIN_RESET, APP_DHT22_EDGE_TIMEOUT_US))
    {
        LOGE(PLATFORM_DHT22_TAG, "data bit fall timeout, PB4=%u",
             (unsigned int)HAL_GPIO_ReadPin(APP_DHT22_GPIO_PORT, APP_DHT22_DATA_PIN));
        return false;
    }
    uint16_t high_end_ticks = (uint16_t)__HAL_TIM_GET_COUNTER(&platform_dht22_timer);
    uint16_t high_duration_us = (uint16_t)(high_end_ticks - high_start_ticks);

    if (high_duration_us <= APP_DHT22_ZERO_HIGH_MAX_US)
    {
        *bit_value = 0U;
        return true;
    }
    if ((high_duration_us >= APP_DHT22_ONE_HIGH_MIN_US) &&
        (high_duration_us <= APP_DHT22_ONE_HIGH_MAX_US))
    {
        *bit_value = 1U;
        return true;
    }

    LOGE(PLATFORM_DHT22_TAG, "data bit pulse width invalid, high_us=%u PB4=%u",
         (unsigned int)high_duration_us,
         (unsigned int)HAL_GPIO_ReadPin(APP_DHT22_GPIO_PORT, APP_DHT22_DATA_PIN));
    return false;
}

/**
 * @brief Captures all 40 DHT22 bits and decodes the validated raw frame.
 *
 * @param[out] temperature_tenths_c Temperature output in tenths °C.
 * @param[out] humidity_tenths_percent Relative humidity output in tenths percent.
 *
 * @return Port result distinguishing timeout, checksum, range, and I/O errors.
 */
static climate_port_status_t platform_dht22_read_frame(int16_t *temperature_tenths_c,
                                                       uint16_t *humidity_tenths_percent)
{
    uint8_t frame[APP_DHT22_FRAME_BYTES] = {0U};

    platform_dht22_set_line_mode(true);
    HAL_Delay(APP_DHT22_START_LOW_MS);
    platform_dht22_set_line_mode(false);

    if (!platform_dht22_wait_level(GPIO_PIN_RESET, APP_DHT22_RESPONSE_TIMEOUT_US))
    {
        LOGE(PLATFORM_DHT22_TAG, "response-low timeout, PB4=%u",
             (unsigned int)HAL_GPIO_ReadPin(APP_DHT22_GPIO_PORT, APP_DHT22_DATA_PIN));
        return CLIMATE_PORT_STATUS_TIMEOUT_RESPONSE;
    }
    uint16_t response_low_start = (uint16_t)__HAL_TIM_GET_COUNTER(&platform_dht22_timer);
    if (!platform_dht22_wait_level(GPIO_PIN_SET, APP_DHT22_EDGE_TIMEOUT_US))
    {
        LOGE(PLATFORM_DHT22_TAG, "response-low-end timeout, PB4=%u low_start=%u",
             (unsigned int)HAL_GPIO_ReadPin(APP_DHT22_GPIO_PORT, APP_DHT22_DATA_PIN),
             (unsigned int)response_low_start);
        return CLIMATE_PORT_STATUS_TIMEOUT_RESPONSE;
    }
    uint16_t response_low_end = (uint16_t)__HAL_TIM_GET_COUNTER(&platform_dht22_timer);
    uint16_t response_low_us = (uint16_t)(response_low_end - response_low_start);

    uint16_t response_high_start = (uint16_t)__HAL_TIM_GET_COUNTER(&platform_dht22_timer);
    if (!platform_dht22_wait_level(GPIO_PIN_RESET, APP_DHT22_EDGE_TIMEOUT_US))
    {
        LOGE(PLATFORM_DHT22_TAG, "response-high-end timeout, PB4=%u low_us=%u",
             (unsigned int)HAL_GPIO_ReadPin(APP_DHT22_GPIO_PORT, APP_DHT22_DATA_PIN),
             (unsigned int)response_low_us);
        return CLIMATE_PORT_STATUS_TIMEOUT_RESPONSE;
    }
    uint16_t response_high_end = (uint16_t)__HAL_TIM_GET_COUNTER(&platform_dht22_timer);
    uint16_t response_high_us = (uint16_t)(response_high_end - response_high_start);
    if ((response_low_us < APP_DHT22_RESPONSE_LOW_MIN_US) ||
        (response_low_us > APP_DHT22_RESPONSE_LOW_MAX_US) ||
        (response_high_us < APP_DHT22_RESPONSE_HIGH_MIN_US) ||
        (response_high_us > APP_DHT22_RESPONSE_HIGH_MAX_US))
    {
        LOGE(PLATFORM_DHT22_TAG,
             "response timing invalid, low_us=%u high_us=%u PB4=%u",
             (unsigned int)response_low_us,
             (unsigned int)response_high_us,
             (unsigned int)HAL_GPIO_ReadPin(APP_DHT22_GPIO_PORT, APP_DHT22_DATA_PIN));
        return CLIMATE_PORT_STATUS_IO_ERROR;
    }

    for (uint32_t bit_index = 0U; bit_index < APP_DHT22_FRAME_BITS; ++bit_index)
    {
        uint8_t bit_value = 0U;
        if (!platform_dht22_capture_bit(&bit_value))
        {
            return CLIMATE_PORT_STATUS_TIMEOUT_DATA;
        }

        uint32_t byte_index = bit_index / 8U;
        frame[byte_index] = (uint8_t)((frame[byte_index] << 1U) | bit_value);
    }

    dht22_decoder_status_t decode_status = dht22_decoder_decode(
        frame,
        sizeof(frame),
        temperature_tenths_c,
        humidity_tenths_percent);
    if (decode_status == DHT22_DECODER_STATUS_OK)
    {
        return CLIMATE_PORT_STATUS_OK;
    }
    if (decode_status == DHT22_DECODER_STATUS_CHECKSUM_ERROR)
    {
        return CLIMATE_PORT_STATUS_CHECKSUM_ERROR;
    }
    if (decode_status == DHT22_DECODER_STATUS_OUT_OF_RANGE)
    {
        return CLIMATE_PORT_STATUS_OUT_OF_RANGE;
    }
    return CLIMATE_PORT_STATUS_INVALID_ARG;
}

/**
 * @brief Configures TIM3 as a free-running one-microsecond edge timer.
 *
 * The project uses the 56 MHz core clock configured by the STM32Cube examples;
 * a prescaler of 55 produces a 1 MHz counter. Short edge waits remain within a
 * single 16-bit counter wrap.
 *
 * @param[in,out] timer_handle HAL timer handle to initialize.
 *
 * @return HAL_OK when the timer is configured and started.
 */
static HAL_StatusTypeDef platform_dht22_timer_init(TIM_HandleTypeDef *timer_handle)
{
    timer_handle->Instance = TIM3;
    timer_handle->Init.Prescaler = 55U;
    timer_handle->Init.CounterMode = TIM_COUNTERMODE_UP;
    timer_handle->Init.Period = 0xFFFFU;
    timer_handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    timer_handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(timer_handle) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_TIM_Base_Start(timer_handle);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *timer_handle)
{
    if ((timer_handle != NULL) && (timer_handle->Instance == TIM3))
    {
        __HAL_RCC_TIM3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
}

climate_port_status_t climate_port_init(void)
{
    if (platform_dht22_is_initialized)
    {
        return CLIMATE_PORT_STATUS_OK;
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    platform_dht22_set_line_mode(false);

    if (platform_dht22_timer_init(&platform_dht22_timer) != HAL_OK)
    {
        return CLIMATE_PORT_STATUS_IO_ERROR;
    }

    platform_dht22_is_initialized = true;
    return CLIMATE_PORT_STATUS_OK;
}

climate_port_status_t climate_port_read(int16_t *temperature_tenths_c,
                                        uint16_t *humidity_tenths_percent)
{
    if ((temperature_tenths_c == NULL) || (humidity_tenths_percent == NULL))
    {
        return CLIMATE_PORT_STATUS_INVALID_ARG;
    }
    if (!platform_dht22_is_initialized)
    {
        return CLIMATE_PORT_STATUS_NOT_READY;
    }

    climate_port_status_t status = platform_dht22_read_frame(temperature_tenths_c,
                                                             humidity_tenths_percent);
    platform_dht22_set_line_mode(false);
    return status;
}

