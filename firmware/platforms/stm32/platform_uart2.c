/* USART2 HAL adapter for the NUCLEO-G070RB ST-LINK Virtual COM Port. */
#include "platform_uart2.h"

#include "app_config.h"
#include "stm32g0xx_hal.h"

#include <stdbool.h>

static UART_HandleTypeDef platform_uart2_handle;
static bool platform_uart2_is_initialized;

void HAL_UART_MspInit(UART_HandleTypeDef *uart_handle)
{
    if ((uart_handle != NULL) && (uart_handle->Instance == USART2))
    {
        GPIO_InitTypeDef gpio_init = {0};

        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        gpio_init.Pin = GPIO_PIN_2 | GPIO_PIN_3;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_PULLUP;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        gpio_init.Alternate = GPIO_AF1_USART2;
        HAL_GPIO_Init(GPIOA, &gpio_init);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *uart_handle)
{
    if ((uart_handle != NULL) && (uart_handle->Instance == USART2))
    {
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
        __HAL_RCC_USART2_CLK_DISABLE();
    }
}

platform_uart2_status_t platform_uart2_init(void)
{
    if (platform_uart2_is_initialized)
    {
        return PLATFORM_UART2_STATUS_OK;
    }

    platform_uart2_handle.Instance = USART2;
    platform_uart2_handle.Init.BaudRate = APP_UART2_BAUD_RATE;
    platform_uart2_handle.Init.WordLength = UART_WORDLENGTH_8B;
    platform_uart2_handle.Init.StopBits = UART_STOPBITS_1;
    platform_uart2_handle.Init.Parity = UART_PARITY_NONE;
    platform_uart2_handle.Init.Mode = UART_MODE_TX_RX;
    platform_uart2_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    platform_uart2_handle.Init.OverSampling = UART_OVERSAMPLING_16;
    platform_uart2_handle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    platform_uart2_handle.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    platform_uart2_handle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&platform_uart2_handle) != HAL_OK)
    {
        return PLATFORM_UART2_STATUS_IO_ERROR;
    }

    if (HAL_UARTEx_SetTxFifoThreshold(&platform_uart2_handle,
                                      UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        (void)HAL_UART_DeInit(&platform_uart2_handle);
        return PLATFORM_UART2_STATUS_IO_ERROR;
    }

    if (HAL_UARTEx_SetRxFifoThreshold(&platform_uart2_handle,
                                      UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        (void)HAL_UART_DeInit(&platform_uart2_handle);
        return PLATFORM_UART2_STATUS_IO_ERROR;
    }

    if (HAL_UARTEx_DisableFifoMode(&platform_uart2_handle) != HAL_OK)
    {
        (void)HAL_UART_DeInit(&platform_uart2_handle);
        return PLATFORM_UART2_STATUS_IO_ERROR;
    }

    platform_uart2_is_initialized = true;
    return PLATFORM_UART2_STATUS_OK;
}

platform_uart2_status_t platform_uart2_write(const uint8_t *data, uint16_t length)
{
    if ((data == NULL) && (length != 0U))
    {
        return PLATFORM_UART2_STATUS_IO_ERROR;
    }
    if (!platform_uart2_is_initialized)
    {
        return PLATFORM_UART2_STATUS_NOT_READY;
    }
    if (length == 0U)
    {
        return PLATFORM_UART2_STATUS_OK;
    }

    HAL_StatusTypeDef hal_status = HAL_UART_Transmit(
        &platform_uart2_handle,
        (uint8_t *)data,
        length,
        APP_UART2_TIMEOUT_MS);
    return (hal_status == HAL_OK) ? PLATFORM_UART2_STATUS_OK
                                  : PLATFORM_UART2_STATUS_IO_ERROR;
}

/* ---- firmgen_telemetry: generated UART accessor ---- */
/* Added by FirmGen so live telemetry can transmit on this already-initialised
 * UART. `platform_uart2_handle` is `static`, i.e. addressable only from inside this file,
 * so a getter here is the only way to hand it to another translation unit.
 * Telemetry only transmits; it never configures the peripheral. Removing
 * telemetry removes this block again. */
UART_HandleTypeDef *firmgen_uart_handle(void)
{
    return &platform_uart2_handle;
}
/* ---- firmgen_telemetry: end generated UART accessor ---- */
