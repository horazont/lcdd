#include "commands.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#include "common.h"
#include "utils.h"
#include "xmpp-utils.h"
#include "display.h"

void cmd_ping(struct State *state, xmpp_stanza_t *const orig, const char *command_args) {
    reply_text(&state->xmpp_state, orig, "pong");
}

void cmd_update_page(struct State *state, xmpp_stanza_t *const orig, const char *command_args) {
    static const char* usage = "usage: update page PAGEINDEX HEX…";

    char *nextarg;
    if (!command_args) {
        reply_text(&state->xmpp_state, orig, usage);
        return;
    }
    long page_index = strtol(command_args, &nextarg, 10);
    if (nextarg == command_args) {
        reply_text(&state->xmpp_state, orig, "%s\ninvalid pageindex received (not a number)", usage);
        return;
    }
    if (page_index < 0 || page_index >= PAGE_COUNT) {
        reply_text(&state->xmpp_state, orig, "%s\nPAGEINDEX out of bounds (0..%d)", usage, PAGE_COUNT-1);
        return;
    }

    if (*nextarg == '\0') {
        reply_text(&state->xmpp_state, orig, "%s\nno HEX data is given", usage);
        return;
    }
    nextarg++;

    int arglen = strlen(nextarg);
    if (arglen % 2 != 0) {
        reply_text(&state->xmpp_state, orig, "HEX has an odd byte count (remove trailing spaces and newlines)");
        return;
    }

    int pagelen = arglen / 2;
    if (pagelen > PAGE_SIZE) {
        reply_text(&state->xmpp_state, orig, "HEX is too long (max %d bytes)", PAGE_SIZE*2);
        return;
    }

    unsigned char *decoded = malloc(pagelen);
    if (decode_hex(decoded, (unsigned char*)nextarg, arglen) != 0) {
        free(decoded);
        reply_text(&state->xmpp_state, orig, "hex decode failed (invalid hexbytes received)");
        return;
    }

    unsigned char *page = state->display_state.pages[page_index];
    display_clear_page(state, page_index);
    memmove(page, decoded, pagelen);

    reply_text(&state->xmpp_state, orig, "%d bytes written to page", pagelen);
}

void cmd_get_resource_use(struct State *state, xmpp_stanza_t *const orig, const char *command_args) {
    struct rusage usage;
    memset(&usage, 0, sizeof(struct rusage));
    getrusage(RUSAGE_SELF, &usage);

    reply_text(&state->xmpp_state, orig,
        "max resident set size: %ld kB\n"
        "utime: %.2f s\n"
        "stime: %.2f s",
        usage.ru_maxrss,
        (float)(usage.ru_utime.tv_sec) + (float)(usage.ru_utime.tv_usec / 1000000.),
        (float)(usage.ru_stime.tv_sec) + (float)(usage.ru_stime.tv_usec / 1000000.)
    );
}

void cmd_set_page_cycling(struct State *state, xmpp_stanza_t *const orig, const char *command_args) {
    static const char* usage = "usage: set page cycling INT\nIf int is 0, page cycling is turned off";

    char *nextarg;
    if (!command_args) {
        reply_text(&state->xmpp_state, orig, usage);
        return;
    }
    long enable = strtol(command_args, &nextarg, 10);
    if (nextarg == command_args) {
        reply_text(&state->xmpp_state, orig, usage);
        return;
    }

    if ((state->display_state.page_cycling != 0) ^ (enable != 0)) {
        // state change
        if (enable) {
            xmpp_timed_handler_add(
                state->xmpp_state.conn,
                state->display_state.page_cycle_handler,
                state->display_state.page_cycle_interval,
                state);
        } else {
            xmpp_timed_handler_delete(
                state->xmpp_state.conn,
                state->display_state.page_cycle_handler);
        }
    }
    state->display_state.page_cycling = enable;
}

void cmd_show_page(struct State *state, xmpp_stanza_t *const orig, const char *command_args) {
    static const char* usage = "usage: show page PAGEINDEX";

    char *nextarg;
    if (!command_args) {
        reply_text(&state->xmpp_state, orig, usage);
        return;
    }
    long page_index = strtol(command_args, &nextarg, 10);
    if (nextarg == command_args) {
        reply_text(&state->xmpp_state, orig, "%s\ninvalid pageindex received (not a number)", usage);
        return;
    }
    if (page_index < 0 || page_index >= PAGE_COUNT) {
        reply_text(&state->xmpp_state, orig, "%s\nPAGEINDEX out of bounds (0..%d)", usage, PAGE_COUNT-1);
        return;
    }

    state->display_state.curr_page = page_index;
}

void cmd_raw(struct State *state, xmpp_stanza_t *const orig, const char *command_args) {
    if (!command_args) {
        return;
    }

    int arglen = strlen(command_args);
    if (arglen % 2 != 0) {
        reply_text(&state->xmpp_state, orig, "HEX has an odd byte count (remove trailing spaces and newlines)");
        return;
    }

    int pagelen = arglen / 2;
    if (pagelen > PAGE_SIZE) {
        reply_text(&state->xmpp_state, orig, "HEX is too long (max %d bytes)", PAGE_SIZE*2);
        return;
    }

    unsigned char *decoded = malloc(pagelen);
    if (decode_hex(decoded, (unsigned char*)command_args, arglen) != 0) {
        free(decoded);
        reply_text(&state->xmpp_state, orig, "hex decode failed (invalid hexbytes received)");
        return;
    }

    int err = display_write_raw(state, decoded, pagelen);
    if (err != 0) {
        reply_text(&state->xmpp_state, orig, "write failed: %d", err);
    } else {
        reply_text(&state->xmpp_state, orig, "sent %d bytes", pagelen);
    }
}

void cmd_echo(struct State *state, xmpp_stanza_t *const orig, const char *command_args) {
    if (!command_args) {
        return;
    }

    int arglen = strlen(command_args);
    int err = display_write_raw(state, command_args, arglen);
    if (err != 0) {
        reply_text(&state->xmpp_state, orig, "write failed: %d", err);
    } else {
        reply_text(&state->xmpp_state, orig, "sent %d bytes", arglen);
    }
}

void cmd_clear(struct State *state, xmpp_stanza_t *const orig, const char *command_args) {
    int err = display_clear(state);
    if (err != 0) {
        reply_text(&state->xmpp_state, orig, "write failed: %d", err);
    }
}

void cmd_set_backlight(struct State *state, xmpp_stanza_t *const orig, const char *command_args) {
    static const char* usage = "usage: set backlight POWER\nPOWER must be a number between 0 and 255 inclusively, with 255 meaning highest power available.";

    char *nextarg;
    if (!command_args) {
        reply_text(&state->xmpp_state, orig, usage);
        return;
    }
    long power = strtol(command_args, &nextarg, 10);
    if (nextarg == command_args) {
        reply_text(&state->xmpp_state, orig, "%s\nnot a number", usage);
        return;
    }
    if (power < 0 || power > 255) {
        reply_text(&state->xmpp_state, orig, "%s\nPOWER out of bounds (0..255)", usage);
        return;
    }

    int err = display_set_backlight_power(state, power);
    if (err != 0) {
        reply_text(&state->xmpp_state, orig, "write failed: %d", err);
    }

}

void cmd_list(struct State *state, xmpp_stanza_t *const orig, const char *command_args);

const struct Command commands[] = {
    {"ping", cmd_ping},
    {"update page", cmd_update_page},
    {"get resource use", cmd_get_resource_use},
    {"set page cycling", cmd_set_page_cycling},
    {"raw", cmd_raw},
    {"echo", cmd_echo},
    {"clear", cmd_clear},
    {"list", cmd_list},
    {"set backlight", cmd_set_backlight},
    {NULL, NULL},
    //~ {"clear", cmd_clear},
    //~ {"echo ", cmd_echo},
    //~ {"set brightness ", cmd_set_brightness},
    //~ {"hex ", cmd_hex},
    //~ {"get resource use", cmd_get_resource_use}
};

void cmd_list(struct State *state, xmpp_stanza_t *const orig, const char *command_args) {
    const struct Command *cmd = commands;
    for (; cmd->name != NULL; cmd++) {
        reply_text(&state->xmpp_state, orig, "%s", cmd->name);
    }
}
