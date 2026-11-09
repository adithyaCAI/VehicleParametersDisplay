/* Portable SH1106 controller interface and incremental display framebuffer. */
#ifndef SH1106_H
#define SH1106_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    SH1106_STATUS_OK = 0,
    SH1106_STATUS_INVALID_ARG,
    SH1106_STATUS_NOT_READY,
    SH1106_STATUS_BUSY,
    SH1106_STATUS_NO_ACK,
    SH1106_STATUS_IO_ERROR
} sh1106_status_t;

typedef struct
{
    void *context;
    sh1106_status_t (*write)(void *context,
                             uint8_t address_7bit,
                             uint8_t control_byte,
                             const uint8_t *data,
                             size_t length);
} sh1106_io_t;

typedef struct
{
    uint8_t address_7bit;
    uint16_t next_framebuffer_byte;
    uint8_t framebuffer[1024U];
    uint8_t flush_phase;
    uint16_t flush_event_page_offset;
    bool flush_event_page_first;
    bool is_initialized;
    bool has_pending_frame;
} sh1106_t;

/**
 * @brief Initializes the SH1106 and clears its internal display RAM.
 *
 * @param[in,out] display SH1106 context to initialize; must not be NULL.
 * @param[in] io Caller-owned I2C write callback; must be set.
 * @param[in] address_7bit Unshifted OLED address, 0..0x7F.
 *
 * @return SH1106_STATUS_OK when initialization completes.
 * @return SH1106_STATUS_INVALID_ARG for invalid pointers or address.
 * @return SH1106_STATUS_NO_ACK when the display does not acknowledge.
 * @return SH1106_STATUS_BUSY when the shared bus is contended.
 * @return SH1106_STATUS_IO_ERROR for another transfer failure.
 */
sh1106_status_t sh1106_init(sh1106_t *display,
                            const sh1106_io_t *io,
                            uint8_t address_7bit);

/**
 * @brief Replaces the monochrome 128x64 framebuffer with a blank image.
 *
 * @param[in,out] display Initialized SH1106 context; must not be NULL.
 *
 * @return SH1106_STATUS_OK when the image is staged for transfer.
 * @return SH1106_STATUS_INVALID_ARG for a NULL context.
 * @return SH1106_STATUS_NOT_READY before successful initialization.
 */
sh1106_status_t sh1106_clear(sh1106_t *display);

/**
 * @brief Draws one bounded ASCII text row into the pending display image.
 *
 * Rows are 8 pixels high and columns are 6 pixels wide; unsupported characters
 * are rendered as spaces. The framebuffer is borrowed by the next flush call.
 *
 * @param[in,out] display Initialized SH1106 context; must not be NULL.
 * @param[in] row Text row index in the range 0..7.
 * @param[in] column Text column in the range 0..20.
 * @param[in] text NUL-terminated ASCII string; must not be NULL.
 *
 * @return SH1106_STATUS_OK when glyphs are written into the framebuffer.
 * @return SH1106_STATUS_INVALID_ARG for NULL input or out-of-range coordinates.
 * @return SH1106_STATUS_NOT_READY before successful initialization.
 */
sh1106_status_t sh1106_draw_text(sh1106_t *display,
                                 uint8_t row,
                                 uint8_t column,
                                 const char *text);

/**
 * @brief Transfers one bounded chunk of the pending framebuffer to the OLED.
 *
 * Each call sends at most the configured chunk length, allowing other shared
 * I2C clients to run between display chunks.
 *
 * @param[in,out] display Initialized SH1106 context; must not be NULL.
 * @param[in] io Callback table used for the serialized I2C transaction.
 *
 * @return SH1106_STATUS_OK when a chunk is sent or no frame is pending.
 * @return SH1106_STATUS_INVALID_ARG for invalid arguments.
 * @return SH1106_STATUS_NOT_READY before initialization.
 * @return SH1106_STATUS_BUSY when another client owns the shared bus.
 * @return SH1106_STATUS_NO_ACK when the display NACKs.
 * @return SH1106_STATUS_IO_ERROR for another transfer failure.
 */
sh1106_status_t sh1106_flush_step(sh1106_t *display, const sh1106_io_t *io);

#endif /* SH1106_H */
