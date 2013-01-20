#ifndef COMMON_H
#define COMMON_H

#include "strophe.h"

#define PAGE_COUNT 3
#define PAGE_SIZE 20*4

struct XMPPState {
    xmpp_conn_t *conn;
    xmpp_ctx_t *ctx;

    int running;

    int ping_pending;
    int next_ping_id;
    int available;
};

struct SerialState {
    const char *path;
    int fd;
};

struct DisplayState {
    int curr_page;
    unsigned char pages[PAGE_COUNT][PAGE_SIZE];
    int page_cycling;
};

struct State {
    struct XMPPState xmpp_state;
    struct SerialState serial_state;
    struct DisplayState display_state;
};

#endif
