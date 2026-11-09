/* Portable log formatting kept separate from the STM32 UART adapter. */
#include "logger.h"

#include "logger_platform.h"

#include <stdio.h>

void logger_vwrite(char level, const char *tag, const char *format, va_list arguments)
{
    char message[192];
    int formatted_length = 0;

    if (format == NULL)
    {
        return;
    }

    if (tag != NULL)
    {
        formatted_length = snprintf(message, sizeof(message), "%c %s: ", level, tag);
    }
    else
    {
        formatted_length = snprintf(message, sizeof(message), "%c: ", level);
    }

    if ((formatted_length < 0) || ((size_t)formatted_length >= sizeof(message)))
    {
        return;
    }

    size_t prefix_length = (size_t)formatted_length;
    int body_length = vsnprintf(&message[prefix_length],
                                sizeof(message) - prefix_length,
                                format,
                                arguments);
    if ((body_length < 0) ||
        ((size_t)body_length >= (sizeof(message) - prefix_length)))
    {
        return;
    }

    size_t total_length = prefix_length + (size_t)body_length;
    if ((total_length + 1U) < sizeof(message))
    {
        message[total_length++] = '\n';
    }
    (void)logger_platform_write((const uint8_t *)message, (uint16_t)total_length);
}

void logger_write(char level, const char *tag, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    logger_vwrite(level, tag, format, arguments);
    va_end(arguments);
}
