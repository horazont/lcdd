#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

static const uint8_t DISPLAY_CMD_CLEAR = 0x00;
static const uint8_t DISPLAY_CMD_WRITE_RAW = 0x01;
static const uint8_t DISPLAY_CMD_WRITE_PAGE = 0x02;

/**
 * Open the display serial port if neccessary.
 *
 * Return 0 if opening succeded, an unspecified, nonzero errorcode
 * otherwise. If the fd is open and valid, it won't reopen it and just
 * return 0.
 */
int display_open(struct State *state);

/**
 * Set the backlight power of the display.
 *
 * Accepts a value from 0 to 255 to indicate the desired power. This is
 * re-mapped to the levels supported by the device.
 *
 * Returns 0 on success, the errno which occured during write otherwise.
 */
int display_set_backlight_power(struct State *state, unsigned char power);

/**
 * Clear the display using a raw code.
 */
int display_clear(struct State *state);

/**
 * Clear the buffer of a page by filling it with spaces.
 *
 * This will not update the display, hence there is no return value.
 */
void display_clear_page(struct State *state, int page_index);

/**
 * Push the current pages contents to the display. Can be used to
 * overdraw anything written by display_write_raw or cycle pages.
 */
int display_redraw_page(struct State *state);

/**
 * Send the given text buffer to the display.
 */
int display_write_text(struct State *state, const uint8_t *buf,
                       size_t len, uint8_t write_mode);

/**
 * Close the display fd if it's open right now neccessary.
 */
void display_close(struct State *state);

#endif
