/* Vendor-neutral monotonic time and millisecond delay contract. */
#ifndef RUNTIME_CLOCK_H
#define RUNTIME_CLOCK_H

#include <stdint.h>

/**
 * @brief Returns the monotonic 32-bit millisecond counter.
 *
 * The counter wraps naturally; elapsed-time comparisons use unsigned subtraction.
 *
 * @return Current tick count in milliseconds.
 */
uint32_t runtime_clock_now_ms(void);

/**
 * @brief Delays the caller for a bounded millisecond interval.
 *
 * @param[in] delay_ms Delay duration in milliseconds; zero returns immediately.
 */
void runtime_clock_delay_ms(uint32_t delay_ms);

#endif /* RUNTIME_CLOCK_H */
