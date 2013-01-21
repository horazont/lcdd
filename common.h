#ifndef COMMON_H
#define COMMON_H

#include "couplet.h"

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
    int fd;
};

struct DisplayState {
    int curr_page;
    unsigned char pages[PAGE_COUNT][PAGE_SIZE];
    int page_cycling;
    xmpp_timed_handler page_cycle_handler;
    unsigned long page_cycle_interval;
};

struct Config {
    char *jid;
    char *pass;
    char *ping_peer;

    char *serial_file;
};

struct State {
    struct XMPPState xmpp_state;
    struct SerialState serial_state;
    struct DisplayState display_state;
    struct Config config;

    int terminated;
};

#endif
