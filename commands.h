#ifndef COMMANDS_H
#define COMMANDS_H

#include "common.h"

typedef void (*CommandHandler) (
    struct State *const state,
    xmpp_stanza_t *const orig,
    const char *command_args);

struct Command {
    const char *name;
    CommandHandler handler;
};

extern const struct Command commands[];

#endif
