/* SH1106 command setup, compact 5x7 text rendering, and chunked framebuffer flush. */
#include "sh1106.h"

#include "app_config.h"
#include "oled_port.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#define SH1106_COMMAND_COUNT 22U
#define SH1106_GLYPH_WIDTH_PIXELS 5U
#define SH1106_CELL_WIDTH_PIXELS 6U
#define SH1106_PAGE_HEIGHT_PIXELS 8U
#define SH1106_CONTROL_COMMAND APP_OLED_COMMAND_CONTROL_BYTE
#define SH1106_CONTROL_DATA APP_OLED_DATA_CONTROL_BYTE
#define SH1106_SET_PAGE_ADDRESS 0xB0U
#define SH1106_SET_LOW_COLUMN 0x00U
#define SH1106_SET_HIGH_COLUMN 0x10U
#define SH1106_DISPLAY_ON 0xAFU
#define SH1106_DISPLAY_OFF 0xAEU
#define SH1106_SET_CONTRAST 0x81U
#define SH1106_SET_SEGMENT_REMAP 0xA1U
#define SH1106_SET_SCAN_DIRECTION 0xC8U
#define SH1106_NORMAL_DISPLAY 0xA6U
#define SH1106_SET_MULTIPLEX 0xA8U
#define SH1106_SET_DISPLAY_OFFSET 0xD3U
#define SH1106_SET_CLOCK_DIVIDE 0xD5U
#define SH1106_SET_PRECHARGE 0xD9U
#define SH1106_SET_COM_PINS 0xDAU
#define SH1106_SET_VCOM_DESELECT 0xDBU
#define SH1106_SET_DC_DC_CONTROL 0xADU
#define SH1106_SET_START_LINE 0x40U
#define SH1106_DISPLAY_START_LINE 0x00U

static const uint8_t sh1106_init_commands[SH1106_COMMAND_COUNT] =
{
    SH1106_DISPLAY_OFF,
    SH1106_SET_CLOCK_DIVIDE, 0x80U,
    SH1106_SET_MULTIPLEX, 0x3FU,
    SH1106_SET_DISPLAY_OFFSET, 0x00U,
    (uint8_t)(SH1106_SET_START_LINE | SH1106_DISPLAY_START_LINE),
    SH1106_SET_DC_DC_CONTROL, 0x8BU,
    SH1106_SET_SEGMENT_REMAP,
    SH1106_SET_SCAN_DIRECTION,
    SH1106_SET_COM_PINS, 0x12U,
    SH1106_SET_CONTRAST, 0x7FU,
    SH1106_SET_PRECHARGE, 0x22U,
    SH1106_SET_VCOM_DESELECT, 0x35U,
    SH1106_NORMAL_DISPLAY,
    SH1106_DISPLAY_ON
};

static const uint8_t sh1106_font[96U][SH1106_GLYPH_WIDTH_PIXELS] =
{
    {0x00U,0x00U,0x00U,0x00U,0x00U}, {0x00U,0x00U,0x5FU,0x00U,0x00U},
    {0x00U,0x07U,0x00U,0x07U,0x00U}, {0x14U,0x7FU,0x14U,0x7FU,0x14U},
    {0x24U,0x2AU,0x7FU,0x2AU,0x12U}, {0x23U,0x13U,0x08U,0x64U,0x62U},
    {0x36U,0x49U,0x56U,0x20U,0x50U}, {0x00U,0x08U,0x07U,0x03U,0x00U},
    {0x00U,0x1CU,0x22U,0x41U,0x00U}, {0x00U,0x41U,0x22U,0x1CU,0x00U},
    {0x2AU,0x1CU,0x7FU,0x1CU,0x2AU}, {0x08U,0x08U,0x3EU,0x08U,0x08U},
    {0x00U,0x80U,0x70U,0x30U,0x00U}, {0x08U,0x08U,0x08U,0x08U,0x08U},
    {0x00U,0x00U,0x60U,0x60U,0x00U}, {0x20U,0x10U,0x08U,0x04U,0x02U},
    {0x3EU,0x51U,0x49U,0x45U,0x3EU}, {0x00U,0x42U,0x7FU,0x40U,0x00U},
    {0x72U,0x49U,0x49U,0x49U,0x46U}, {0x21U,0x41U,0x49U,0x4DU,0x33U},
    {0x18U,0x14U,0x12U,0x7FU,0x10U}, {0x27U,0x45U,0x45U,0x45U,0x39U},
    {0x3CU,0x4AU,0x49U,0x49U,0x31U}, {0x41U,0x21U,0x11U,0x09U,0x07U},
    {0x36U,0x49U,0x49U,0x49U,0x36U}, {0x46U,0x49U,0x49U,0x29U,0x1EU},
    {0x00U,0x00U,0x14U,0x00U,0x00U}, {0x00U,0x80U,0x74U,0x00U,0x00U},
    {0x00U,0x08U,0x14U,0x22U,0x41U}, {0x14U,0x14U,0x14U,0x14U,0x14U},
    {0x00U,0x41U,0x22U,0x14U,0x08U}, {0x02U,0x01U,0x59U,0x09U,0x06U},
    {0x3EU,0x41U,0x5DU,0x59U,0x4EU}, {0x7CU,0x12U,0x11U,0x12U,0x7CU},
    {0x7FU,0x49U,0x49U,0x49U,0x36U}, {0x3EU,0x41U,0x41U,0x41U,0x22U},
    {0x7FU,0x41U,0x41U,0x22U,0x1CU}, {0x7FU,0x49U,0x49U,0x49U,0x41U},
    {0x7FU,0x09U,0x09U,0x09U,0x01U}, {0x3EU,0x41U,0x49U,0x49U,0x7AU},
    {0x7FU,0x08U,0x08U,0x08U,0x7FU}, {0x00U,0x41U,0x7FU,0x41U,0x00U},
    {0x20U,0x40U,0x41U,0x3FU,0x01U}, {0x7FU,0x08U,0x14U,0x22U,0x41U},
    {0x7FU,0x40U,0x40U,0x40U,0x40U}, {0x7FU,0x02U,0x1CU,0x02U,0x7FU},
    {0x7FU,0x04U,0x08U,0x10U,0x7FU}, {0x3EU,0x41U,0x41U,0x41U,0x3EU},
    {0x7FU,0x09U,0x09U,0x09U,0x06U}, {0x3EU,0x41U,0x51U,0x21U,0x5EU},
    {0x7FU,0x09U,0x19U,0x29U,0x46U}, {0x26U,0x49U,0x49U,0x49U,0x32U},
    {0x03U,0x01U,0x7FU,0x01U,0x03U}, {0x3FU,0x40U,0x40U,0x40U,0x3FU},
    {0x1FU,0x20U,0x40U,0x20U,0x1FU}, {0x3FU,0x40U,0x38U,0x40U,0x3FU},
    {0x63U,0x14U,0x08U,0x14U,0x63U}, {0x03U,0x04U,0x78U,0x04U,0x03U},
    {0x61U,0x59U,0x49U,0x4DU,0x43U}, {0x00U,0x7FU,0x41U,0x41U,0x41U},
    {0x02U,0x04U,0x08U,0x10U,0x20U}, {0x00U,0x41U,0x41U,0x41U,0x7FU},
    {0x04U,0x02U,0x01U,0x02U,0x04U}, {0x40U,0x40U,0x40U,0x40U,0x40U},
    {0x00U,0x03U,0x07U,0x08U,0x00U}, {0x20U,0x54U,0x54U,0x78U,0x40U},
    {0x7FU,0x28U,0x44U,0x44U,0x38U}, {0x38U,0x44U,0x44U,0x44U,0x28U},
    {0x38U,0x44U,0x44U,0x28U,0x7FU}, {0x38U,0x54U,0x54U,0x54U,0x18U},
    {0x00U,0x08U,0x7EU,0x09U,0x02U}, {0x18U,0xA4U,0xA4U,0x9CU,0x78U},
    {0x7FU,0x08U,0x04U,0x04U,0x78U}, {0x00U,0x44U,0x7DU,0x40U,0x00U},
    {0x20U,0x40U,0x40U,0x3DU,0x00U}, {0x7FU,0x10U,0x28U,0x44U,0x00U},
    {0x00U,0x41U,0x7FU,0x40U,0x00U}, {0x7CU,0x04U,0x78U,0x04U,0x78U},
    {0x7CU,0x08U,0x04U,0x04U,0x78U}, {0x38U,0x44U,0x44U,0x44U,0x38U},
    {0xFCU,0x18U,0x24U,0x24U,0x18U}, {0x18U,0x24U,0x24U,0x18U,0xFCU},
    {0x7CU,0x08U,0x04U,0x04U,0x08U}, {0x48U,0x54U,0x54U,0x54U,0x24U},
    {0x04U,0x04U,0x3FU,0x44U,0x24U}, {0x3CU,0x40U,0x40U,0x20U,0x7CU},
    {0x1CU,0x20U,0x40U,0x20U,0x1CU}, {0x3CU,0x40U,0x30U,0x40U,0x3CU},
    {0x44U,0x28U,0x10U,0x28U,0x44U}, {0x4CU,0x90U,0x90U,0x90U,0x7CU},
    {0x44U,0x64U,0x54U,0x4CU,0x44U}, {0x00U,0x08U,0x36U,0x41U,0x00U},
    {0x00U,0x00U,0x7FU,0x00U,0x00U}, {0x00U,0x41U,0x36U,0x08U,0x00U},
    {0x02U,0x01U,0x02U,0x04U,0x02U}, {0x3CU,0x26U,0x23U,0x26U,0x3CU}
};

static sh1106_status_t sh1106_map_port_status(oled_port_status_t status)
{
    switch (status)
    {
        case OLED_PORT_STATUS_OK:
            return SH1106_STATUS_OK;
        case OLED_PORT_STATUS_INVALID_ARG:
            return SH1106_STATUS_INVALID_ARG;
        case OLED_PORT_STATUS_BUSY:
            return SH1106_STATUS_BUSY;
        case OLED_PORT_STATUS_NOT_READY:
            return SH1106_STATUS_NOT_READY;
        case OLED_PORT_STATUS_NO_ACK:
            return SH1106_STATUS_NO_ACK;
        case OLED_PORT_STATUS_IO_ERROR:
        default:
            return SH1106_STATUS_IO_ERROR;
    }
}

static sh1106_status_t sh1106_write_commands(sh1106_t *display,
                                             const sh1106_io_t *io,
                                             const uint8_t *commands,
                                             size_t count)
{
    return sh1106_map_port_status(io->write(io->context,
                                            display->address_7bit,
                                            SH1106_CONTROL_COMMAND,
                                            commands,
                                            count));
}

sh1106_status_t sh1106_init(sh1106_t *display,
                            const sh1106_io_t *io,
                            uint8_t address_7bit)
{
    if ((display == NULL) || (io == NULL) || (io->write == NULL) ||
        (address_7bit > 0x7FU))
    {
        return SH1106_STATUS_INVALID_ARG;
    }

    display->address_7bit = address_7bit;
    display->is_initialized = false;
    display->has_pending_frame = false;
    display->next_framebuffer_byte = 0U;
    display->flush_phase = 0U;
    display->flush_event_page_first = false;

    sh1106_status_t status = sh1106_write_commands(display,
                                                   io,
                                                   sh1106_init_commands,
                                                   sizeof(sh1106_init_commands));
    if (status != SH1106_STATUS_OK)
    {
        return status;
    }

    for (size_t index = 0U; index < sizeof(display->framebuffer); ++index)
    {
        display->framebuffer[index] = 0U;
    }
    display->is_initialized = true;
    display->has_pending_frame = true;
    return SH1106_STATUS_OK;
}

sh1106_status_t sh1106_clear(sh1106_t *display)
{
    if (display == NULL)
    {
        return SH1106_STATUS_INVALID_ARG;
    }
    if (!display->is_initialized)
    {
        return SH1106_STATUS_NOT_READY;
    }

    for (size_t index = 0U; index < sizeof(display->framebuffer); ++index)
    {
        display->framebuffer[index] = 0U;
    }
    display->next_framebuffer_byte = 0U;
    display->flush_phase = 0U;
    display->flush_event_page_first = false;
    display->has_pending_frame = true;
    return SH1106_STATUS_OK;
}

sh1106_status_t sh1106_draw_text(sh1106_t *display,
                                 uint8_t row,
                                 uint8_t column,
                                 const char *text)
{
    if ((display == NULL) || (text == NULL) ||
        (row >= APP_OLED_TEXT_ROWS) || (column >= APP_OLED_TEXT_COLUMNS))
    {
        return SH1106_STATUS_INVALID_ARG;
    }
    if (!display->is_initialized)
    {
        return SH1106_STATUS_NOT_READY;
    }

    uint16_t text_column = column;
    size_t character_index = 0U;
    while ((text[character_index] != '\0') &&
           (text_column < APP_OLED_TEXT_COLUMNS))
    {
        unsigned char character = (unsigned char)text[character_index];
        if ((character < 0x20U) || (character > 0x7FU))
        {
            character = (unsigned char)' ';
        }

        size_t font_index = (size_t)(character - 0x20U);
        size_t framebuffer_offset =
            ((size_t)row * APP_OLED_WIDTH_PIXELS) +
            ((size_t)text_column * SH1106_CELL_WIDTH_PIXELS);
        for (size_t glyph_column = 0U; glyph_column < SH1106_GLYPH_WIDTH_PIXELS;
             ++glyph_column)
        {
            display->framebuffer[framebuffer_offset + glyph_column] =
                sh1106_font[font_index][glyph_column];
        }
        display->framebuffer[framebuffer_offset + SH1106_GLYPH_WIDTH_PIXELS] = 0U;
        ++text_column;
        ++character_index;
    }
    display->has_pending_frame = true;
    display->next_framebuffer_byte = 0U;
    display->flush_phase = 0U;
    return SH1106_STATUS_OK;
}

sh1106_status_t sh1106_flush_step(sh1106_t *display, const sh1106_io_t *io)
{
    if ((display == NULL) || (io == NULL) || (io->write == NULL))
    {
        return SH1106_STATUS_INVALID_ARG;
    }
    if (!display->is_initialized)
    {
        return SH1106_STATUS_NOT_READY;
    }
    if (!display->has_pending_frame)
    {
        return SH1106_STATUS_OK;
    }

    if (display->flush_event_page_first)
    {
        uint16_t event_offset = display->flush_event_page_offset;
        uint16_t event_chunk_length =
            (uint16_t)(APP_OLED_WIDTH_PIXELS - event_offset);
        if (event_chunk_length > APP_OLED_FLUSH_CHUNK_BYTES)
        {
            event_chunk_length = APP_OLED_FLUSH_CHUNK_BYTES;
        }

        if (display->flush_phase == 0U)
        {
            uint16_t controller_column = (uint16_t)APP_OLED_COLUMN_OFFSET + event_offset;
            uint8_t page_commands[3] =
            {
                (uint8_t)(SH1106_SET_PAGE_ADDRESS | 7U),
                (uint8_t)(SH1106_SET_LOW_COLUMN | (controller_column & 0x0FU)),
                (uint8_t)(SH1106_SET_HIGH_COLUMN |
                          ((controller_column >> 4U) & 0x0FU))
            };
            sh1106_status_t status = sh1106_write_commands(display,
                                                           io,
                                                           page_commands,
                                                           sizeof(page_commands));
            if (status == SH1106_STATUS_OK)
            {
                display->flush_phase = 2U;
            }
            return status;
        }

        if (display->flush_phase == 2U)
        {
            sh1106_status_t status = io->write(
                io->context,
                display->address_7bit,
                SH1106_CONTROL_DATA,
                &display->framebuffer[(7U * APP_OLED_WIDTH_PIXELS) + event_offset],
                event_chunk_length);
            if (status == SH1106_STATUS_OK)
            {
                display->flush_event_page_offset =
                    (uint16_t)(event_offset + event_chunk_length);
                display->flush_phase = 0U;
                if (display->flush_event_page_offset >= APP_OLED_WIDTH_PIXELS)
                {
                    display->flush_event_page_first = false;
                    display->flush_event_page_offset = 0U;
                }
            }
            return status;
        }

        display->flush_phase = 0U;
        return SH1106_STATUS_IO_ERROR;
    }

    uint16_t start = display->next_framebuffer_byte;
    uint16_t remaining = (uint16_t)(APP_OLED_FRAMEBUFFER_BYTES - start);
    uint16_t chunk_length = (remaining > APP_OLED_FLUSH_CHUNK_BYTES)
                                ? APP_OLED_FLUSH_CHUNK_BYTES
                                : remaining;

    if (display->flush_phase == 0U)
    {
        uint8_t page = (uint8_t)(start / APP_OLED_WIDTH_PIXELS);
        uint8_t column = (uint8_t)(start % APP_OLED_WIDTH_PIXELS);
        uint16_t controller_column = (uint16_t)column + APP_OLED_COLUMN_OFFSET;
        uint8_t page_commands[3] =
        {
            (uint8_t)(SH1106_SET_PAGE_ADDRESS | page),
            (uint8_t)(SH1106_SET_LOW_COLUMN | (controller_column & 0x0FU)),
            (uint8_t)(SH1106_SET_HIGH_COLUMN |
                      ((controller_column >> 4U) & 0x0FU))
        };
        sh1106_status_t status = sh1106_write_commands(display,
                                                       io,
                                                       page_commands,
                                                       sizeof(page_commands));
        if (status == SH1106_STATUS_OK)
        {
            display->flush_phase = 1U;
        }
        return status;
    }

    if (display->flush_phase != 1U)
    {
        display->flush_phase = 0U;
        return SH1106_STATUS_IO_ERROR;
    }

    sh1106_status_t status = io->write(io->context,
                                       display->address_7bit,
                                       SH1106_CONTROL_DATA,
                                       &display->framebuffer[start],
                                       chunk_length);
    if (status != SH1106_STATUS_OK)
    {
        return status;
    }

    display->next_framebuffer_byte = (uint16_t)(start + chunk_length);
    display->flush_phase = 0U;
    if (display->next_framebuffer_byte >= APP_OLED_FRAMEBUFFER_BYTES)
    {
        display->next_framebuffer_byte = 0U;
        display->has_pending_frame = false;
    }
    return SH1106_STATUS_OK;
}
