/* STM32 HAL implementation of the vendor-neutral monotonic clock contract. */
#include "runtime_clock.h"

#include "stm32g0xx_hal.h"

uint32_t runtime_clock_now_ms(void)
{
    return HAL_GetTick();
}

void runtime_clock_delay_ms(uint32_t delay_ms)
{
    HAL_Delay(delay_ms);
}
