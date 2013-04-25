#include "display.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>

int display_write_raw(struct State *state, const void *buf, size_t len);

int display_open(struct State *state) {
    const char *const path = state->config.serial_file;

    if (!path) {
        return -1;
    }

    struct stat stat_results;

    memset(&stat_results, 0, sizeof(struct stat));
    if (stat(path, &stat_results) != 0) {
        display_close(state);
        return -1;
    }
    if (!S_ISCHR(stat_results.st_mode)) {
        display_close(state);
        return -2;
    }

    if (state->serial_state.fd >= 0) {
        return 0;
    }

    int fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        state->serial_state.fd = -1;
        return -3;
    } else {
        struct termios port_settings;
        memset(&port_settings, 0, sizeof(port_settings));
        cfsetispeed(&port_settings, B9600);
        cfsetospeed(&port_settings, B9600);
        port_settings.c_cflag &= ~PARENB;
        port_settings.c_cflag &= ~CSTOPB;
        port_settings.c_cflag &= ~CSIZE;
        port_settings.c_cflag |= CS8;

        tcsetattr(fd, TCSANOW, &port_settings);
        state->serial_state.fd = fd;
        return 0;
    }
}

int display_clear(struct State *state) {
    return display_write_raw(state, &DISPLAY_CMD_CLEAR, 1);
}

void display_clear_page(struct State *state, int page_index) {
    memset(state->display_state.pages[page_index], ' ', PAGE_SIZE);
}

int display_redraw_page(struct State *state) {
    int err = display_clear(state);
    if (err != 0) {
        return err;
    }

    unsigned char *page = state->display_state.pages[state->display_state.curr_page];
    return display_write_text(state, page, PAGE_SIZE, DISPLAY_CMD_WRITE_PAGE);
}

int display_set_backlight_power(struct State *state, unsigned char power) {
    /* unsigned char cmd[2] = "\x7c\x80"; */
    /* unsigned int power_int = (power * 30) / 255; */
    /* if (power_int > 29) { */
    /*     power_int = 29; */
    /* } */
    /* cmd[1] = 0x80 + (unsigned char)power_int; */
    /* return display_write_raw(state, cmd, 2); */
    return -1;
}

int display_write_text(struct State *state, const uint8_t *buf,
                       size_t len, uint8_t write_mode) {
    while (len > 255) {
        display_write_text(state, buf, 255, write_mode);
        buf += 255;
        len -= 255;
    }
    uint8_t *newbuf = malloc(len+2);
    newbuf[0] = write_mode;
    newbuf[1] = len;
    memcpy(&newbuf[2], buf, len);
    int result = display_write_raw(state, newbuf, len+2);
    free(newbuf);
    return result;
}

int display_write_raw(struct State *state, const void *buf, size_t len) {
    int err = display_open(state);
    if (err != 0) {
        return err;
    }

    int written = write(state->serial_state.fd, buf, len);
    while (written < len) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            written += write(state->serial_state.fd, ((uint8_t*)buf)+written, len);
        } else {
            display_close(state);
            return errno;
        }
    }

    return 0;
}

void display_close(struct State *state) {
    if (state->serial_state.fd >= 0) {
        close(state->serial_state.fd);
    }
    state->serial_state.fd = -1;
}
