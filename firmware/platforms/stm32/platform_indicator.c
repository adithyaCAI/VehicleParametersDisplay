/* STM32 platform adapter for the NUCLEO-G070RB LD4 user indicator. */
#include "indicator_port.h"

#include "app_config.h"
#include "board.h"
#include "stm32g0xx_hal.h"

#include <stdbool.h>

static bool platform_indicator_is_initialized;

indicator_port_status_t indicator_port_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio_init.Pin = APP_LED4_GPIO_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(APP_LED4_GPIO_PORT, &gpio_init);
    HAL_GPIO_WritePin(APP_LED4_GPIO_PORT,
                      APP_LED4_GPIO_PIN,
                      (GPIO_PinState)board_led_level(false));
    platform_indicator_is_initialized = true;
    return INDICATOR_PORT_STATUS_OK;
}

indicator_port_status_t indicator_port_set(bool is_on)
{
    if (!platform_indicator_is_initialized)
    {
        return INDICATOR_PORT_STATUS_IO_ERROR;
    }

    HAL_GPIO_WritePin(APP_LED4_GPIO_PORT,
                      APP_LED4_GPIO_PIN,
                      (GPIO_PinState)board_led_level(is_on));
    return INDICATOR_PORT_STATUS_OK;
}
