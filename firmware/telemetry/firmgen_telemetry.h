/**
 * firmgen_telemetry.h — Firmgen live telemetry component for STM32 (classic HAL).
 *
 * STM32 counterpart of the ESP32 firmgen_telemetry component. Emits the SAME wire
 * format (JSON lines: {"t":"boot",...} once, then {"t":"rtos",...} periodically) so
 * the existing backend parser (rtos_telemetry::parser), schema (RtosSnapshot,
 * BootEvent) and GUI panel (RtosMonitorPanel) work UNCHANGED — this file only needs
 * to speak the same protocol, sourced from HAL/CMSIS instead of ESP-IDF APIs.
 *
 * Debug vs. production (FG_TELEMETRY_PROFILE, set via CMake -D flag — there is no
 * Kconfig on STM32, so the profile is a straight compile-time define):
 *   FG_TELEMETRY_PROFILE_DEBUG      (default) — telemetry ON: samples every 500 ms.
 *   FG_TELEMETRY_PROFILE_PRODUCTION            — telemetry OFF: firmgen_telemetry_start()
 *     becomes a no-op, the sampler and the poll entry point are compiled out
 *     entirely. No serial traffic, no extra RAM/flash cost, same hard on/off as ESP.
 *
 * Usage — two calls, both placed by the injector:
 *
 *     #include "firmgen_telemetry.h"
 *     firmgen_telemetry_start(&huart2);   // once, passing the ST-Link VCP UART handle
 *     firmgen_telemetry_poll();           // every iteration of the forever-loop
 *
 * NOTE — this file no longer overrides HAL_SYSTICK_Callback. It used to, to avoid needing
 * a call in the application loop, but that hook never fired: HAL_IncTick() does not call
 * HAL_SYSTICK_Callback, and CubeMX's generated SysTick_Handler calls only HAL_IncTick, not
 * HAL_SYSTICK_IRQHandler. HAL1 telemetry therefore sampled nothing on a real CubeMX-layout
 * project. Overriding HAL_IncTick instead would have been worse still — CubeMX's
 * "timebase source = TIM" option and the CMSIS-OS ports both define it, so the override
 * would be a duplicate-symbol link error there — and emitting from inside the SysTick ISR
 * made HAL_UART_Transmit's timeout unable to expire, since it compares HAL_GetTick() while
 * uwTick is advanced by the handler it runs inside. Hence the explicit poll, matching HAL2.
 *
 * Free-RAM sampling needs the heap top. It does NOT require any particular linker
 * symbol: it tries `_sbrk(0)` first (the allocator's own view, present in both ST
 * driver generations), then the weak symbols `__end__`, `_end`, `end` and
 * `_heap_start`, and reports free RAM as unavailable if none resolve. An earlier
 * version hard-required `_end` and failed to LINK on a real STM32C5 project, whose
 * linker script defines `__end__` but not `_end`.
 *
 * Metrics reported (JSON key names match ESP's schema so the same Rust parser and
 * GUI apply unchanged):
 *   "heap":{"free":<b>,"min":<b>,"total":<b>}  — free-RAM heuristic (SP vs. heap top),
 *     NOT a real heap allocator's free size — STM32 bare-metal projects do not
 *     reliably have one. "total" is the SRAM size configured at build time
 *     (FG_TELEMETRY_TOTAL_RAM_BYTES); "min" is the lowest free-RAM value observed
 *     since boot.
 *   "stack_peak"  — approximate main-stack usage %, computed against
 *     FG_TELEMETRY_STACK_RESERVE_BYTES (the reserved stack size — match this to your
 *     linker script's `_Min_Stack_Size`, or the number is meaningless). Documented
 *     limitation: unlike ESP's per-task stack watermark, this is a single estimate
 *     for the whole system's one stack, using a caller-provided reservation size
 *     rather than a measured one.
 *   "cpu", "tasks", "top_tasks" — require FreeRTOS with
 *     configGENERATE_RUN_TIME_STATS / configUSE_TRACE_FACILITY enabled (same FreeRTOS
 *     config macros ESP-IDF's own port sets under its Kconfig options — this is not
 *     STM32-specific, it's the standard FreeRTOS API). Report 0 / omitted without it,
 *     exactly like ESP does when its equivalent Kconfig options are unset.
 *
 * Reset/crash reason ("boot" event, emitted once): read from RCC reset-flag registers
 * via the portable __HAL_RCC_GET_FLAG() macros (stable across classic-HAL STM32
 * families), NOT read from a memory-mapped register address, so this does not need
 * per-family porting.
 *
 * Works on any classic-HAL STM32 family (F0/F1/F3/F4/F7/G0/G4/H7/L0/L1/L4/L5/U5/WB/WL)
 * — this file only calls `__HAL_RCC_*`, `HAL_UART_Transmit`, `HAL_GetTick`,
 * `__get_MSP`, and (optionally) FreeRTOS's standard API, none of which are
 * family-specific. NOT compatible with STM32C5's newer driver generation
 * (stm32c5xx_hal_*), which uses a different API shape entirely.
 */

#pragma once

/* The family HAL header is substituted in by the injector (g0 -> e.g. "f4"),
 * the same @-token templating stm32.rs already uses for its own scaffold files.
 *
 * Deliberately NOT `#include "main.h"`: FirmGen's own bare STM32 scaffold generates a
 * main.c that only declares `extern void app_start(void);` and has no main.h at all
 * (HAL_Init/clock config are left as TODO for the CubeMX export), so depending on
 * main.h would compile only in CubeMX-imported projects and fail in freshly
 * scaffolded ones. Including the family header directly works in both. */
#include "stm32g0xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start telemetry. Idempotent — safe to call more than once; only the first call
 * has effect. In the production profile this is a no-op (see file header).
 *
 * @param huart  UART handle already MX_USARTx_Init()'d — telemetry does not
 *               configure the peripheral, only transmits on it.
 */
void firmgen_telemetry_start(UART_HandleTypeDef *huart);

/* Call from the application's forever-loop. Cheap: it returns immediately until
 * FG_TELEMETRY_INTERVAL_MS has elapsed, so calling it every iteration is fine.
 *
 * This replaces an earlier HAL_SYSTICK_Callback override that never actually fired --
 * HAL_IncTick() does not call that callback, and CubeMX's SysTick_Handler does not call
 * HAL_SYSTICK_IRQHandler, so HAL1 telemetry silently never sampled. Both HAL generations
 * are now driven the same way, from the loop the injector locates. */
void firmgen_telemetry_poll(void);

#ifdef __cplusplus
}
#endif
