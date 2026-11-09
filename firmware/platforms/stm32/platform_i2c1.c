/* STM32 HAL adapter for the serialized I2C1 shared peripheral bus. */
#include "platform_i2c1.h"

#include "app_config.h"
#include "logger.h"
#include "stm32g0xx_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static I2C_HandleTypeDef platform_i2c1_handle;
static bool platform_i2c1_is_initialized;
static bool platform_i2c1_is_locked;
static uint32_t platform_i2c1_nack_probe_count;

/**
 * @brief Atomically claims the shared bus lock without waiting.
 *
 * The interrupt mask only covers the lock test/update; it is restored to its
 * prior state before returning.
 *
 * @return true if the lock was acquired; otherwise false.
 */
static bool platform_i2c1_try_lock(void)
{
    uint32_t previous_primask = __get_PRIMASK();
    __disable_irq();

    bool lock_acquired = !platform_i2c1_is_locked;
    if (lock_acquired)
    {
        platform_i2c1_is_locked = true;
    }

    __set_PRIMASK(previous_primask);
    return lock_acquired;
}

/**
 * @brief Releases the bus lock after a complete HAL transfer.
 */
static void platform_i2c1_unlock(void)
{
    uint32_t previous_primask = __get_PRIMASK();
    __disable_irq();
    platform_i2c1_is_locked = false;
    __set_PRIMASK(previous_primask);
}

/**
 * @brief Validates an unshifted address, data pointer, and 16-bit HAL length.
 *
 * @param[in] address_7bit Unshifted target address.
 * @param[in] data Payload pointer.
 * @param[in] length Payload size in bytes.
 *
 * @return true if all arguments are valid.
 */
static bool platform_i2c1_arguments_valid(uint8_t address_7bit,
                                          const void *data,
                                          size_t length)
{
    return (address_7bit <= 0x7FU) &&
           (data != NULL) &&
           (length > 0U) &&
           (length <= UINT16_MAX);
}

/**
 * @brief Maps HAL transfer results into the vendor-neutral bus status type.
 *
 * @param[in] hal_status Status returned by the STM32 HAL operation.
 *
 * @return Matching OK/BUSY/NOT_FOUND status, or IO_ERROR for other failures.
 */
static i2c_bus_status_t platform_i2c1_map_hal_status(HAL_StatusTypeDef hal_status)
{
    i2c_bus_status_t result = I2C_BUS_STATUS_IO_ERROR;

    if (hal_status == HAL_OK)
    {
        result = I2C_BUS_STATUS_OK;
    }
    else if (hal_status == HAL_BUSY)
    {
        result = I2C_BUS_STATUS_BUSY;
    }
    else if ((hal_status == HAL_ERROR) &&
             (HAL_I2C_GetError(&platform_i2c1_handle) == HAL_I2C_ERROR_AF))
    {
        result = I2C_BUS_STATUS_NOT_FOUND;
    }

    return result;
}

/**
 * @brief Configures PB8/PB9 and the I2C1 peripheral clock for HAL_I2C_Init.
 *
 * Pins are open-drain alternate function 6. External breakout pull-ups are
 * present per user confirmation, so MCU internal pull-ups remain disabled.
 *
 * @param[in,out] i2c_handle I2C handle being initialized by the HAL.
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef *i2c_handle)
{
    if ((i2c_handle != NULL) && (i2c_handle->Instance == I2C1))
    {
        GPIO_InitTypeDef gpio_init = {0};
        RCC_PeriphCLKInitTypeDef peripheral_clock_config = {0};

        peripheral_clock_config.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
        peripheral_clock_config.I2c1ClockSelection = RCC_I2C1CLKSOURCE_SYSCLK;
        if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clock_config) != HAL_OK)
        {
            return;
        }

        __HAL_RCC_GPIOB_CLK_ENABLE();
        gpio_init.Pin = APP_I2C1_SCL_PIN | APP_I2C1_SDA_PIN;
        gpio_init.Mode = GPIO_MODE_AF_OD;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        gpio_init.Alternate = GPIO_AF6_I2C1;
        HAL_GPIO_Init(APP_I2C1_GPIO_PORT, &gpio_init);
        __HAL_RCC_I2C1_CLK_ENABLE();
    }
}

/**
 * @brief Deinitializes PB8/PB9 and the I2C1 clock when HAL tears down the handle.
 *
 * @param[in,out] i2c_handle I2C handle being deinitialized by the HAL.
 */
void HAL_I2C_MspDeInit(I2C_HandleTypeDef *i2c_handle)
{
    if ((i2c_handle != NULL) && (i2c_handle->Instance == I2C1))
    {
        HAL_GPIO_DeInit(APP_I2C1_GPIO_PORT,
                        APP_I2C1_SCL_PIN | APP_I2C1_SDA_PIN);
        __HAL_RCC_I2C1_CLK_DISABLE();
    }
}

i2c_bus_status_t i2c_bus_init(void)
{
    if (platform_i2c1_is_initialized)
    {
        return I2C_BUS_STATUS_OK;
    }

    platform_i2c1_handle.Instance = I2C1;
    platform_i2c1_handle.Init.Timing = APP_I2C1_TIMING;
    platform_i2c1_handle.Init.OwnAddress1 = 0U;
    platform_i2c1_handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    platform_i2c1_handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    platform_i2c1_handle.Init.OwnAddress2 = 0U;
    platform_i2c1_handle.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    platform_i2c1_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    platform_i2c1_handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&platform_i2c1_handle) != HAL_OK)
    {
        return I2C_BUS_STATUS_IO_ERROR;
    }

    if (HAL_I2CEx_ConfigAnalogFilter(&platform_i2c1_handle,
                                     I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    {
        (void)HAL_I2C_DeInit(&platform_i2c1_handle);
        return I2C_BUS_STATUS_IO_ERROR;
    }

    if (HAL_I2CEx_ConfigDigitalFilter(&platform_i2c1_handle, 0U) != HAL_OK)
    {
        (void)HAL_I2C_DeInit(&platform_i2c1_handle);
        return I2C_BUS_STATUS_IO_ERROR;
    }

    platform_i2c1_is_initialized = true;
    return I2C_BUS_STATUS_OK;
}

/**
 * @brief Claims the initialized bus or returns its readiness/contention status.
 *
 * @return NOT_READY if initialization has not completed, BUSY if contended,
 *         otherwise OK with the shared lock held.
 */
static i2c_bus_status_t platform_i2c1_begin_transfer(void)
{
    if (!platform_i2c1_is_initialized)
    {
        return I2C_BUS_STATUS_NOT_READY;
    }
    if (!platform_i2c1_try_lock())
    {
        return I2C_BUS_STATUS_BUSY;
    }
    return I2C_BUS_STATUS_OK;
}

i2c_bus_status_t i2c_bus_write(uint8_t address_7bit,
                               const uint8_t *data,
                               size_t length)
{
    if (!platform_i2c1_arguments_valid(address_7bit, data, length))
    {
        return I2C_BUS_STATUS_INVALID_ARG;
    }

    i2c_bus_status_t begin_status = platform_i2c1_begin_transfer();
    if (begin_status != I2C_BUS_STATUS_OK)
    {
        return begin_status;
    }

    HAL_StatusTypeDef hal_status = HAL_I2C_Master_Transmit(
        &platform_i2c1_handle,
        (uint16_t)((uint16_t)address_7bit << 1U),
        (uint8_t *)data,
        (uint16_t)length,
        APP_I2C1_TIMEOUT_MS);
    platform_i2c1_unlock();
    return platform_i2c1_map_hal_status(hal_status);
}

i2c_bus_status_t i2c_bus_write_register(uint8_t address_7bit,
                                        uint8_t register_address,
                                        const uint8_t *data,
                                        size_t length)
{
    if (!platform_i2c1_arguments_valid(address_7bit, data, length))
    {
        return I2C_BUS_STATUS_INVALID_ARG;
    }

    i2c_bus_status_t begin_status = platform_i2c1_begin_transfer();
    if (begin_status != I2C_BUS_STATUS_OK)
    {
        return begin_status;
    }

    HAL_StatusTypeDef hal_status = HAL_I2C_Mem_Write(
        &platform_i2c1_handle,
        (uint16_t)((uint16_t)address_7bit << 1U),
        register_address,
        I2C_MEMADD_SIZE_8BIT,
        (uint8_t *)data,
        (uint16_t)length,
        APP_I2C1_TIMEOUT_MS);
    platform_i2c1_unlock();
    return platform_i2c1_map_hal_status(hal_status);
}

i2c_bus_status_t i2c_bus_read_register(uint8_t address_7bit,
                                       uint8_t register_address,
                                       uint8_t *data,
                                       size_t length)
{
    if (!platform_i2c1_arguments_valid(address_7bit, data, length))
    {
        return I2C_BUS_STATUS_INVALID_ARG;
    }

    i2c_bus_status_t begin_status = platform_i2c1_begin_transfer();
    if (begin_status != I2C_BUS_STATUS_OK)
    {
        return begin_status;
    }

    HAL_StatusTypeDef hal_status = HAL_I2C_Mem_Read(
        &platform_i2c1_handle,
        (uint16_t)((uint16_t)address_7bit << 1U),
        register_address,
        I2C_MEMADD_SIZE_8BIT,
        data,
        (uint16_t)length,
        APP_I2C1_TIMEOUT_MS);
    platform_i2c1_unlock();
    return platform_i2c1_map_hal_status(hal_status);
}

i2c_bus_status_t i2c_bus_scan(uint8_t *addresses,
                              size_t output_capacity,
                              size_t *address_count)
{
    if ((address_count == NULL) ||
        ((addresses == NULL) && (output_capacity != 0U)))
    {
        return I2C_BUS_STATUS_INVALID_ARG;
    }

    i2c_bus_status_t begin_status = platform_i2c1_begin_transfer();
    if (begin_status != I2C_BUS_STATUS_OK)
    {
        return begin_status;
    }

    *address_count = 0U;
    platform_i2c1_nack_probe_count = 0U;
    i2c_bus_status_t result = I2C_BUS_STATUS_OK;

    for (uint16_t address = 0x08U; address <= 0x77U; ++address)
    {
        HAL_StatusTypeDef hal_status = HAL_I2C_IsDeviceReady(
            &platform_i2c1_handle,
            (uint16_t)(address << 1U),
            1U,
            APP_I2C1_TIMEOUT_MS);

        if (hal_status == HAL_OK)
        {
            if (*address_count < output_capacity)
            {
                addresses[*address_count] = (uint8_t)address;
            }
            ++(*address_count);
        }
        else if (hal_status == HAL_ERROR)
        {
            /* HAL_I2C_IsDeviceReady reports an exhausted NACK probe as HAL_ERROR
             * and may leave HAL_I2C_ERROR_TIMEOUT set after clearing the AF flag.
             */
            ++platform_i2c1_nack_probe_count;
        }
        else
        {
            result = platform_i2c1_map_hal_status(hal_status);
            break;
        }
    }

    platform_i2c1_unlock();
    return result;
}

uint32_t i2c_bus_nack_probe_count_get(void)
{
    return platform_i2c1_nack_probe_count;
}
