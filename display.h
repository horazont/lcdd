#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

/**
 * Open the display serial port if neccessary.
 *
 * Return 0 if opening succeded, an unspecified, nonzero errorcode
 * otherwise. If the fd is open and valid, it won't reopen it and just
 * return 0.
 */
int display_open(struct State *state);

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
 * Send *buf* to the display directly.
 */
int display_write_raw(struct State *state, const void *buf, size_t len);

/**
 * Close the display fd if it's open right now neccessary.
 */
void display_close(struct State *state);

#endif
