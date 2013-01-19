#ifndef XMPP_UTILS_H
#define XMPP_UTILS_H

#include "common.h"

void set_presence(struct XMPPState *state, int new_status, const char *msg);
xmpp_stanza_t* iq(xmpp_ctx_t *const ctx, const char *type, const char *to, const char *id);
void reply_text(struct XMPPState *state, xmpp_stanza_t *const orig, const char *fmt, ...);

#endif
