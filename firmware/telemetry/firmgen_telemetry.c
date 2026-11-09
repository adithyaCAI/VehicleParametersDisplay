/*
 * firmgen_telemetry.c — Firmgen live telemetry for STM32 (classic HAL).
 *
 * Speaks the SAME wire format as the ESP32 component (see firmgen_telemetry.h for the
 * full rationale). Debug profile: emits {"t":"boot",...} once, then {"t":"rtos",...}
 * every FG_TELEMETRY_INTERVAL_MS via HAL_UART_Transmit. Production profile: this
 * entire file compiles to nothing observable — no task/hook, no bytes on the wire.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "firmgen_telemetry.h"
#include "fg_telemetry_profile.h"

/* ── Debug/production profile ────────────────────────────────────────────────
 * No Kconfig on STM32, and no CMake flag either: the profile lives in the generated
 * header fg_telemetry_profile.h, which either defines FG_TELEMETRY_PROFILE_PRODUCTION
 * or does not. Switching profile rewrites that one header — the same
 * "rewrite one config file" mechanic as ESP's sdkconfig, and it works identically for
 * a FirmGen-scaffolded project and a CubeMX-imported one, neither of which share a
 * CMake structure worth patching per-layout. */
#ifdef FG_TELEMETRY_PROFILE_PRODUCTION
#define FG_TELEMETRY_ENABLED 0
#else
#define FG_TELEMETRY_ENABLED 1
#endif

#if FG_TELEMETRY_ENABLED

#define FG_VERSION 1u
#ifndef FG_TELEMETRY_INTERVAL_MS
#define FG_TELEMETRY_INTERVAL_MS 500u
#endif

/* Match these two to your project's actual linker script / SRAM size — see the
 * "IMPORTANT — linker symbols" note in firmgen_telemetry.h. Override via CMake
 * -D if your board's values differ (both have safe, conservative Nucleo-F4 defaults). */
#ifndef FG_TELEMETRY_TOTAL_RAM_BYTES
#define FG_TELEMETRY_TOTAL_RAM_BYTES (128u * 1024u)   /* STM32F407 SRAM1+SRAM2 */
#endif
#ifndef FG_TELEMETRY_STACK_RESERVE_BYTES
#define FG_TELEMETRY_STACK_RESERVE_BYTES (2u * 1024u) /* match linker's _Min_Stack_Size */
#endif

#if FG_FREERTOS_PRESENT
#include "FreeRTOS.h"
#include "task.h"
#define FG_TOP_TASKS_MAX 3u
#endif

/* Heap top, resolved without requiring any ONE linker-script symbol.
 *
 * The first version hard-required `_end` and failed to LINK on a real STM32C5 project:
 * ST's new-generation linker script defines `__end__`, `end` and `_heap_start` but NOT
 * `_end`, and its own sysmem.c uses `__end__`. Classic CubeMX scripts PROVIDE `_end`,
 * which is why a vendor-template fixture never caught it. Rather than demand the user
 * edit their linker script, probe what is actually there:
 *
 *   1. `_sbrk(0)` — the allocator's own heap top. Most accurate, because it accounts
 *      for memory already handed to malloc rather than just the static end of .bss,
 *      and it is present in both ST generations. Returns -1 under nosys.specs.
 *   2. the weak linker symbols, in decreasing specificity.
 *   3. nothing found -> report free RAM as 0, which the GUI shows as unavailable. A
 *      missing number is better than a wrong one, and better than a build that fails.
 *
 * All declarations are weak, so an absent symbol links to 0 and is skipped at runtime
 * instead of breaking the link. */
#if defined(__ICCARM__)
/* IAR ARM Compiler: none of the symbols below exist. Its DLIB has no _sbrk and its linker
 * places the heap as an .icf block, so the probe reports free RAM as unavailable (case 3
 * above) rather than declaring GNU symbols, or GNU attribute syntax, this compiler lacks. */
static uint32_t heap_top(void)
{
    return 0u;
}
#else
extern uint8_t __end__      __attribute__((weak));  /* ST new-generation + CMSIS */
extern uint8_t _end         __attribute__((weak));  /* classic CubeMX */
extern uint8_t end          __attribute__((weak));  /* PROVIDE (end = .) */
extern uint8_t _heap_start  __attribute__((weak));  /* ST C5 .heap section */
extern void *_sbrk(int incr) __attribute__((weak));

static uint32_t heap_top(void)
{
    if (_sbrk != NULL) {
        void *p = _sbrk(0);
        if (p != NULL && p != (void *)-1) return (uint32_t)p;
    }
    if (&__end__ != NULL)     return (uint32_t)&__end__;
    if (&_end != NULL)        return (uint32_t)&_end;
    if (&end != NULL)         return (uint32_t)&end;
    if (&_heap_start != NULL) return (uint32_t)&_heap_start;
    return 0u;
}
#endif /* __ICCARM__ */

static UART_HandleTypeDef *s_huart = NULL;
static uint32_t            s_min_free_ram = 0xFFFFFFFFu;
static uint32_t            s_last_ms = 0u;
static bool                s_started = false;

/* The reset reason is READ at start() but TRANSMITTED at the first poll().
 *
 * Both halves of that split are load-bearing. The RCC flags must be read early and
 * cleared immediately (HAL's documented pattern), or the next reset inherits this
 * boot's flags and misreports forever. But start() is injected as the first statement
 * of the application's entry function, which on a real NUCLEO-G071RB project runs
 * BEFORE that function initialises the UART -- so transmitting there hit a handle
 * still in HAL_UART_STATE_RESET, HAL_UART_Transmit returned HAL_BUSY, and the reason
 * was destroyed by the flag clear without ever reaching the host. Capturing into these
 * statics and emitting from the first poll (which by construction runs inside the
 * application's own loop, after its init) makes the boot line survive any layout. */
static const char         *s_boot_reason = NULL;
static bool                s_boot_is_crash = false;
static bool                s_boot_pending = false;

/* -- Transmit -----------------------------------------------------------------
 *
 * Two ways out, because FirmGen-generated projects expose their UART in more than one
 * way. Normally telemetry is handed a HAL handle and uses HAL_UART_Transmit, which
 * respects that handle's busy state and so cannot interleave with the application's own
 * HAL traffic.
 *
 * FG_TELEMETRY_RAW_USART selects the other way: the application configured the peripheral
 * with direct register writes and keeps no HAL handle at all. A real project did exactly
 * that -- USART2->BRR and ->CR1 set by hand, with a printf retarget polling TXE and
 * poking TDR -- so there was no `UART_HandleTypeDef` anywhere to borrow, static or not,
 * and telemetry was refused with "No usable UART found in this project". In that mode
 * telemetry polls TXE and writes TDR itself. It still never configures the peripheral:
 * BRR, CR1 and the pin mux stay entirely the application's business.
 *
 * The trade-off is real and worth stating: without the HAL lock a telemetry line and an
 * application printf can interleave byte-wise. Each side writes one short line at a time
 * so it is uncommon, and the host parser discards a malformed line rather than choking,
 * but a HAL handle remains the better source whenever one exists -- which is why the
 * injector only falls back to this. */
static void fg_uart_write(const uint8_t *data, size_t len)
{
#ifdef FG_TELEMETRY_RAW_USART
    for (size_t i = 0u; i < len; i++) {
#if defined(USART_ISR_TXE_TXFNF)
        while ((FG_TELEMETRY_RAW_USART->ISR & USART_ISR_TXE_TXFNF) == 0u) { }
#else
        while ((FG_TELEMETRY_RAW_USART->ISR & USART_ISR_TXE) == 0u) { }
#endif
        FG_TELEMETRY_RAW_USART->TDR = (uint8_t)data[i];
    }
#else
    if (s_huart == NULL) {
        return;
    }
    (void)HAL_UART_Transmit(s_huart, (uint8_t *)data, (uint16_t)len, 100u);
#endif
}

/* ── Reset / crash reason ─────────────────────────────────────────────────── */

/* Maps the RCC reset-flag register to a stable string — same reason/code/is_crash
 * shape as ESP's reset_reason_str() in firmgen_telemetry.c, so the Rust-side
 * BootEvent::classify() list applies unchanged. Keep the crash set in sync with it. */
static const char *reset_reason_str(bool *is_crash)
{
    *is_crash = false;
    /* Order matters: a watchdog/software reset can also leave PORRST/PINRST clear but
     * several flags set together on some families — check the more specific/severe
     * reasons first so e.g. IWDGRST isn't masked by a coincidentally-set PINRST. */
    /* Every flag is #ifdef-guarded because the classic-HAL families do NOT share one
     * reset-flag set, and assuming they did meant this file simply did not compile on
     * anything but the family it was written against. A real STM32G071RB build failed with
     * "'RCC_FLAG_BORRST' undeclared ... did you mean 'RCC_FLAG_OBLRST'?": the G0 HAL has no
     * BORRST and no PORRST at all, and folds brown-out and power-on into one RCC_FLAG_PWRRST
     * exactly as the C5's HAL2 does. Guarding each flag keeps one component correct across
     * F4/G0/H5/U5/... and degrades to "unknown" rather than to a build error. */
#ifdef RCC_FLAG_IWDGRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))  { *is_crash = true; return "independent_watchdog"; }
#endif
#ifdef RCC_FLAG_WWDGRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST))  { *is_crash = true; return "window_watchdog"; }
#endif
#ifdef RCC_FLAG_LPWRRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST))  { *is_crash = true; return "low_power_reset"; }
#endif
#ifdef RCC_FLAG_BORRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST))   { *is_crash = true; return "brownout"; }
#endif
#ifdef RCC_FLAG_SFTRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))   { return "sw_reset"; }
#endif
#ifdef RCC_FLAG_PORRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))   { return "poweron"; }
#endif
    /* Families without separate BOR/POR flags (G0, C0, ...) report the pair as one.
     * Deliberately NOT a crash: an ordinary power-on lands here too, and calling every
     * power-up a crash would cry wolf on the panel's reset banner. Same reasoning, and the
     * same "power_reset" label, as the HAL2 component uses for HAL_RCC_RESET_FLAG_PWR. */
#ifdef RCC_FLAG_PWRRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PWRRST))   { return "power_reset"; }
#endif
#ifdef RCC_FLAG_OBLRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_OBLRST))   { return "option_byte_reload"; }
#endif
#ifdef RCC_FLAG_PINRST
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))   { return "ext_pin"; }
#endif
    return "unknown";
}

/* Read and clear the RCC flags, keeping the verdict for the first poll to send. */
static void capture_reset_reason(void)
{
    bool is_crash = false;
    s_boot_reason = reset_reason_str(&is_crash);
    s_boot_is_crash = is_crash;
    s_boot_pending = true;
    /* Cleared right after reading, matching HAL's documented pattern: otherwise the
     * next reset of any cause inherits this boot's flags and misreports forever. */
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

static void emit_boot_event(void)
{
    if (!s_boot_pending || s_boot_reason == NULL) return;
    s_boot_pending = false;

    /* static for the same reason as emit_json's buffer: keep telemetry's stack frame
     * negligible on targets with a small main stack. Distinct from that buffer, so a boot
     * line and a snapshot can never share storage. */
    static char line[128];
    int n = snprintf(line, sizeof(line),
        "{\"t\":\"boot\",\"v\":%u,\"reason\":\"%s\",\"code\":0,\"crash\":%s}\r\n",
        (unsigned)FG_VERSION, s_boot_reason, s_boot_is_crash ? "true" : "false");
    if (n > 0) {
        size_t len = ((size_t)n < sizeof(line)) ? (size_t)n : sizeof(line) - 1u;
        fg_uart_write((const uint8_t *)line, len);
    }
}

/* ── Core sampling ────────────────────────────────────────────────────────── */

static uint32_t free_ram_bytes(void)
{
    uint32_t sp = __get_MSP();
    uint32_t heap_end = heap_top();
    if (heap_end == 0u) return 0u;   /* unknown -> reported as unavailable */
    return (sp > heap_end) ? (sp - heap_end) : 0u;
}

static uint32_t stack_used_pct(void)
{
    /* Approximate: (reserved stack - current free-to-heap-end distance), clamped.
     * See the documented limitation in firmgen_telemetry.h — this is a single
     * estimate for the whole system's one stack, not a measured high-water mark. */
    uint32_t free_bytes = free_ram_bytes();
    if (free_bytes >= FG_TELEMETRY_STACK_RESERVE_BYTES) return 0u;
    uint32_t used = FG_TELEMETRY_STACK_RESERVE_BYTES - free_bytes;
    uint32_t pct = (used * 100u) / FG_TELEMETRY_STACK_RESERVE_BYTES;
    return (pct > 100u) ? 100u : pct;
}

#if FG_FREERTOS_PRESENT
typedef struct {
    char     name[configMAX_TASK_NAME_LEN];
    uint32_t cpu_pct;
    uint32_t stack_min;
} fg_task_t;

/* Same algorithm as the ESP component's sample_tasks() — configGENERATE_RUN_TIME_STATS
 * / configUSE_TRACE_FACILITY are standard FreeRTOS config, not ESP-IDF-specific, so
 * this logic ports unchanged. Requires the project's FreeRTOSConfig.h to implement
 * portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()/portGET_RUN_TIME_COUNTER_VALUE() — CubeMX
 * wires this automatically when "Generate Runtime Stats" is enabled in the .ioc. */
static uint32_t sample_tasks(uint32_t *out_cpu_pct, uint32_t *out_task_count,
                              fg_task_t *top, uint32_t top_max, uint32_t *out_top_n)
{
#if configGENERATE_RUN_TIME_STATS && configUSE_TRACE_FACILITY
    uint32_t n = uxTaskGetNumberOfTasks();
    *out_task_count = n;
    TaskStatus_t *buf = pvPortMalloc(n * sizeof(TaskStatus_t));
    if (!buf) { *out_cpu_pct = 0u; *out_top_n = 0u; return 0u; }

    uint32_t total_ticks = 0u;
    uint32_t found = uxTaskGetSystemState(buf, n, &total_ticks);

    uint32_t idle_ticks = 0u;
    for (uint32_t i = 0u; i < found; i++) {
        if (strncmp(buf[i].pcTaskName, "IDLE", 4) == 0) idle_ticks += buf[i].ulRunTimeCounter;
    }

    uint32_t top_n = 0u;
    if (top && top_max > 0u && total_ticks > 0u) {
        for (uint32_t i = 0u; i < found; i++) {
            if (strncmp(buf[i].pcTaskName, "IDLE", 4) == 0) continue;
            uint32_t pct = (uint32_t)(((uint64_t)buf[i].ulRunTimeCounter * 100u) / total_ticks);
            uint32_t insert_at = top_n;
            for (uint32_t j = 0u; j < top_n; j++) {
                if (pct > top[j].cpu_pct) { insert_at = j; break; }
            }
            if (insert_at >= top_max) continue;
            uint32_t shift_end = (top_n < top_max) ? top_n : top_max - 1u;
            for (uint32_t j = shift_end; j > insert_at; j--) top[j] = top[j - 1u];
            strncpy(top[insert_at].name, buf[i].pcTaskName, sizeof(top[insert_at].name) - 1u);
            top[insert_at].name[sizeof(top[insert_at].name) - 1u] = '\0';
            top[insert_at].cpu_pct   = pct;
            top[insert_at].stack_min = (uint32_t)buf[i].usStackHighWaterMark;
            if (top_n < top_max) top_n++;
        }
    }
    *out_top_n = top_n;
    vPortFree(buf);

    uint32_t idle_pct = (total_ticks > 0u) ? ((idle_ticks * 100u) / total_ticks) : 100u;
    *out_cpu_pct = (idle_pct > 100u) ? 0u : (100u - idle_pct);
    return found;
#else
    (void)top; (void)top_max;
    *out_cpu_pct = 0u; *out_task_count = 0u; *out_top_n = 0u;
    return 0u;
#endif
}
#endif /* FG_FREERTOS_PRESENT */

/* ── JSON emitter ─────────────────────────────────────────────────────────── */

static void emit_json(uint32_t cpu_pct, uint32_t task_count, uint32_t free_ram,
                      uint32_t min_free_ram, uint32_t stack_pct, uint32_t uptime_s
#if FG_FREERTOS_PRESENT
                      , const void *top_v, uint32_t top_n
#endif
                      )
{
    /* `static`, NOT a stack array. This buffer is half a kilobyte, and ST's C5 template
     * ships a 1 KB main stack (STACK_SIZE = 0x400 in stm32c5a3xg_flash.ld). A 512-byte
     * frame plus newlib's snprintf frame plus the app's own call depth overflowed it, and
     * the Cortex-M33 stack-limit check turned that into a HardFault at the very first
     * 500 ms emit: the board booted, ran, started telemetry, then died silently. Telemetry
     * must never be the thing that overruns the stack it is measuring, so the buffer lives
     * in .bss and the emit frame stays tiny. Safe because emit_json has a single call site
     * and cannot re-enter itself. */
    static char line[512];
    size_t off = 0;
    int n = snprintf(line, sizeof(line),
        "{\"t\":\"rtos\",\"v\":%u,\"cpu\":%lu,\"tasks\":%lu,"
        "\"heap\":{\"free\":%lu,\"min\":%lu,\"total\":%lu},"
        "\"stack_peak\":%lu,\"uptime\":%lu",
        (unsigned)FG_VERSION,
        (unsigned long)cpu_pct, (unsigned long)task_count,
        (unsigned long)free_ram, (unsigned long)min_free_ram,
        (unsigned long)FG_TELEMETRY_TOTAL_RAM_BYTES,
        (unsigned long)stack_pct, (unsigned long)uptime_s);
    if (n < 0) return;
    off = ((size_t)n < sizeof(line)) ? (size_t)n : sizeof(line) - 1u;

#if FG_FREERTOS_PRESENT
    const fg_task_t *top = (const fg_task_t *)top_v;
    if (top && top_n > 0u) {
        int m = snprintf(line + off, sizeof(line) - off, ",\"top_tasks\":[");
        if (m > 0) off += ((size_t)m < sizeof(line) - off) ? (size_t)m : sizeof(line) - off - 1u;
        for (uint32_t i = 0u; i < top_n; i++) {
            m = snprintf(line + off, sizeof(line) - off,
                "%s{\"name\":\"%s\",\"cpu\":%lu,\"stack_min\":%lu}",
                (i == 0u) ? "" : ",", top[i].name,
                (unsigned long)top[i].cpu_pct, (unsigned long)top[i].stack_min);
            if (m > 0) off += ((size_t)m < sizeof(line) - off) ? (size_t)m : sizeof(line) - off - 1u;
        }
        if (off < sizeof(line) - 1u) line[off++] = ']';
    }
#endif

    if (off < sizeof(line) - 2u) { line[off++] = '}'; line[off++] = '\r'; line[off++] = '\n'; }
    fg_uart_write((const uint8_t *)line, off);
}

/* ── Periodic driver: called from the application's forever-loop ───────────
 *
 * This used to override the weak HAL_SYSTICK_Callback, on the stated assumption that
 * "HAL calls this WEAK callback from HAL_IncTick(), itself called from SysTick_Handler
 * every 1 ms on every classic-HAL project". That assumption is simply false, and it made
 * HAL1 telemetry dead code: HAL_IncTick's entire body is `uwTick += uwTickFreq;`, and the
 * only caller of HAL_SYSTICK_Callback is HAL_SYSTICK_IRQHandler, which CubeMX's generated
 * SysTick_Handler does not call (it calls HAL_IncTick alone). Verified against a real
 * NUCLEO-G071RB project: zero references to HAL_SYSTICK anywhere outside Drivers/. So the
 * hook never fired, and a HAL1 board emitted nothing no matter how well everything else
 * was wired.
 *
 * Driving both generations from the same explicit poll fixes that and removes a whole
 * class of hazard rather than widening it:
 *   - Owning HAL_IncTick instead would be worse: CubeMX's "timebase source = TIM" option
 *     and CMSIS-OS ports both DEFINE it, so the override becomes a duplicate-symbol link
 *     error on those projects.
 *   - Emitting from inside the SysTick ISR meant HAL_UART_Transmit's 100 ms timeout could
 *     never expire, because it compares HAL_GetTick() while uwTick is advanced by the very
 *     handler it is running inside. Reachable for real when telemetry shares its USART
 *     with a printf retarget.
 *   - Poll runs in thread context, after the application's own init, which is what lets
 *     the deferred boot event above work on any project layout.
 * The injector places this call in whichever loop actually runs (see select_poll_target).
 */
void firmgen_telemetry_poll(void)
{
    if (!s_started) return;

    /* Unsigned subtraction, so this stays correct across the 32-bit tick wraparound
     * (~49.7 days) instead of stalling telemetry until the counter catches up. */
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - s_last_ms) < FG_TELEMETRY_INTERVAL_MS) {
        /* The boot line still goes out on the first poll even before the first
         * interval elapses, so a reset reason is never delayed by half a second. */
        emit_boot_event();
        return;
    }
    s_last_ms = now;
    emit_boot_event();

    uint32_t free_ram = free_ram_bytes();
    if (free_ram < s_min_free_ram) s_min_free_ram = free_ram;
    uint32_t stack_pct = stack_used_pct();
    uint32_t uptime_s  = now / 1000u;

    uint32_t cpu_pct = 0u, task_count = 0u, top_n = 0u;
#if FG_FREERTOS_PRESENT
    fg_task_t top[FG_TOP_TASKS_MAX];
    sample_tasks(&cpu_pct, &task_count, top, FG_TOP_TASKS_MAX, &top_n);
    emit_json(cpu_pct, task_count, free_ram, s_min_free_ram, stack_pct, uptime_s, top, top_n);
#else
    (void)top_n;
    emit_json(cpu_pct, task_count, free_ram, s_min_free_ram, stack_pct, uptime_s);
#endif
}

#endif /* FG_TELEMETRY_ENABLED */

#if !FG_TELEMETRY_ENABLED
/* Production: the sampler above is compiled out entirely, but the application still
 * contains the call the injector placed in its loop, so the symbol must still exist.
 * An empty body costs one `bx lr` and emits nothing -- the profile's whole promise. */
void firmgen_telemetry_poll(void)
{
}
#endif

/* ── FreeRTOS run-time-stats timebase (DWT cycle counter) ─────────────────────
 *
 * `configGENERATE_RUN_TIME_STATS` does nothing unless the port also supplies
 * portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() and portGET_RUN_TIME_COUNTER_VALUE(); ST's
 * generated FreeRTOSConfig.h defines neither, so the option alone still yields 0% per
 * task. The injector points those macros at these functions.
 *
 * DWT is used instead of a hardware timer because it takes no peripheral the
 * application may want and exists on Cortex-M4/M7/M33. Limits: it lives in the debug
 * unit (a target that powers that down reports 0), and CYCCNT is 32-bit so after the
 * >>10 scaling it wraps in a few hours, after which FreeRTOS's cumulative percentages
 * drift. Acceptable for a development-only metric — this is compiled into debug only.
 */
#if FG_FREERTOS_PRESENT
void fg_telemetry_init_runtime_stats(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t fg_telemetry_runtime_counter(void)
{
    return (uint32_t)(DWT->CYCCNT >> 10);
}
#endif

/* ── Public API ───────────────────────────────────────────────────────────── */

void firmgen_telemetry_start(UART_HandleTypeDef *huart)
{
#if FG_TELEMETRY_ENABLED
    if (s_started) return; /* idempotent */
    s_huart = huart;
    s_min_free_ram = free_ram_bytes();
    s_last_ms = HAL_GetTick();
    /* Read the reset flags now (they are only valid this early) but leave the transmit
     * to the first poll, which runs after the application has initialised the UART. */
    capture_reset_reason();
    s_started = true;
#else
    /* Production profile: nothing is sampled and nothing is emitted. */
    (void)huart;
#endif
}
