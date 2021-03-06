#include "commands.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"
#include "utils.h"
#include "xmpp-utils.h"
#include "display.h"

void cmd_ping(struct State *state,
              xmpp_stanza_t *const orig,
              const char *command_args)
{
    reply_text(&state->xmpp_state, orig, "pong");
}

void cmd_update_page(struct State *state,
                     xmpp_stanza_t *const orig,
                     const char *command_args)
{
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
    free(decoded);
}

void cmd_get_resource_use(struct State *state,
                          xmpp_stanza_t *const orig,
                          const char *command_args)
{
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

void cmd_set_page_cycling(struct State *state,
                          xmpp_stanza_t *const orig,
                          const char *command_args)
{
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

void cmd_show_page(struct State *state,
                   xmpp_stanza_t *const orig,
                   const char *command_args)
{
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

void cmd_raw(struct State *state,
             xmpp_stanza_t *const orig,
             const char *command_args)
{
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

    int err = display_write_text(state, decoded,
                                 pagelen, DISPLAY_CMD_WRITE_RAW);
    if (err != 0) {
        reply_text(&state->xmpp_state, orig, "write failed: %d", err);
    }
}

void cmd_echo(struct State *state,
              xmpp_stanza_t *const orig,
              const char *command_args)
{
    if (!command_args) {
        return;
    }

    int arglen = strlen(command_args);
    int err = display_write_text(state, (const uint8_t*)command_args,
                                 arglen, DISPLAY_CMD_WRITE_RAW);
    if (err != 0) {
        reply_text(&state->xmpp_state, orig, "write failed: %d", err);
    }
}

void cmd_clear(struct State *state,
               xmpp_stanza_t *const orig,
               const char *command_args)
{
    int err = display_clear(state);
    if (err != 0) {
        reply_text(&state->xmpp_state, orig, "write failed: %d", err);
    }
}

void cmd_clear_all(struct State *state,
                   xmpp_stanza_t *const orig,
                   const char *command_args)
{
    for (int i = 0; i < PAGE_COUNT; i++) {
        display_clear_page(state, i);
    }
}

void cmd_set_backlight(struct State *state,
                       xmpp_stanza_t *const orig,
                       const char *command_args)
{
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

void cmd_set_page_cycle_interval(struct State *state,
                                 xmpp_stanza_t *const orig,
                                 const char *command_args)
{
    static const char* usage = "usage: set page cycle interval MILLISECS";

    char *nextarg;
    if (!command_args) {
        reply_text(&state->xmpp_state, orig, usage);
        return;
    }
    long interval = strtol(command_args, &nextarg, 10);
    if (nextarg == command_args) {
        reply_text(&state->xmpp_state, orig, "%s\nnot a number", usage);
        return;
    }
    if (interval < 100) {
        reply_text(&state->xmpp_state, orig, "%s\nMILLISECS out of bounds (must be >= 100)", usage);
        return;
    }

    state->display_state.page_cycle_interval = interval;

}

void cmd_read_sensors(struct State *state,
                      xmpp_stanza_t *const orig,
                      const char *command_args)
{
    int bufsize = 1024;
    int offset = 0;
    char *msg = malloc(1024);

    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (i > 0) {
            msg = appendf(msg, &offset, &bufsize,
                          "\n");
        }
        msg = appendf(msg, &offset, &bufsize,
                      "%d: ", i);

        struct SensorState *sensor = &state->sensors[i];
        if (sensor->known) {
            msg = appendf(msg, &offset, &bufsize,
                          " id=%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x value=%d",
                          sensor->addr[0],
                          sensor->addr[1],
                          sensor->addr[2],
                          sensor->addr[3],
                          sensor->addr[4],
                          sensor->addr[5],
                          sensor->addr[6],
                          sensor->addr[7],
                          sensor->value);
        } else {
            msg = appendf(msg, &offset, &bufsize,
                          " ???");
        }
    }

    reply_text(&state->xmpp_state,
               orig,
               "%s", msg);
    free(msg);
}

void cmd_resync(struct State *state, xmpp_stanza_t *const orig, const char *command_args)
{
    debug_msg("resync requested via xmpp...\n");
    if (display_resync(state) != 0) {
        reply_text(&state->xmpp_state, orig, "Resync failed: errno = %d", errno);
    }
}

void cmd_desync(struct State *state, xmpp_stanza_t *const orig, const char *command_args)
{
    const int fd = state->serial_state.fd;
    if (fd >= 0) {
        uint8_t buf = 0;
        read(fd, &buf, 1);
    }
}

void cmd_list(struct State *state, xmpp_stanza_t *const orig, const char *command_args);

const struct Command commands[] = {
    {"ping", cmd_ping},
    {"update page", cmd_update_page},
    {"get resource use", cmd_get_resource_use},
    {"set page cycling", cmd_set_page_cycling},
    {"set page cycle interval", cmd_set_page_cycle_interval},
    {"raw", cmd_raw},
    {"echo", cmd_echo},
    {"clear all", cmd_clear_all},
    {"clear", cmd_clear},
    {"list", cmd_list},
    {"set backlight", cmd_set_backlight},
    {"read sensors", cmd_read_sensors},
    {"resync", cmd_resync},
    {"desync", cmd_desync},
    {NULL, NULL},
};

void cmd_list(struct State *state,
              xmpp_stanza_t *const orig,
              const char *command_args)
{
    const struct Command *cmd = commands;
    for (; cmd->name != NULL; cmd++) {
        reply_text(&state->xmpp_state, orig, "%s", cmd->name);
    }
}
