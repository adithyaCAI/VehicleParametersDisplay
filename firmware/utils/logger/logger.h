#ifndef LOGGER_H
#define LOGGER_H

/* Vendor-neutral log formatters. The application writes formatted text through the platform port. */
#include <stdarg.h>
#include <stddef.h>

/**
 * @brief Emits a formatted, newline-terminated diagnostic record.
 *
 * The formatter is implemented in the platform adapter. The tag and format
 * strings are borrowed for the duration of the call.
 *
 * @param[in] level One-character severity label.
 * @param[in] tag Module label; may be NULL for an untagged record.
 * @param[in] format printf-compatible format string.
 * @param[in] arguments Format arguments matching format.
 */
void logger_vwrite(char level, const char *tag, const char *format, va_list arguments);

/**
 * @brief Emits a formatted diagnostic record at the requested severity.
 *
 * @param[in] level One-character severity label.
 * @param[in] tag Module label; may be NULL.
 * @param[in] format printf-compatible format string.
 * @param[in] ... Arguments matching format.
 */
void logger_write(char level, const char *tag, const char *format, ...);

#define LOGI(tag, fmt, ...) logger_write('I', (tag), (fmt), ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) logger_write('W', (tag), (fmt), ##__VA_ARGS__)
#define LOGE(tag, fmt, ...) logger_write('E', (tag), (fmt), ##__VA_ARGS__)
#define LOGD(tag, fmt, ...) logger_write('D', (tag), (fmt), ##__VA_ARGS__)

#endif /* LOGGER_H */
