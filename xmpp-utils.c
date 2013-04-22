#include "xmpp-utils.h"

#include "utils.h"
#include <libcouplet/couplet.h>

void set_presence(struct XMPPState *state, int new_status, const char *msg)
{
    // force setting the status if state->available is -1
    // otherwise only set if the new_status is not equal to state->available
    if (state->available != -1 && !((state->available != 0) ^ (new_status != 0))) {
        return;
    }
    xmpp_ctx_t *const ctx = state->ctx;
    xmpp_conn_t *const conn = state->conn;

    xmpp_stanza_t *presence = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(presence, "presence");

    xmpp_stanza_t *node = NULL;
    xmpp_stanza_t *text = NULL;
    if (!new_status) {
        xmpp_stanza_t *node = xmpp_stanza_new(ctx);
        xmpp_stanza_t *text = xmpp_stanza_new(ctx);
        xmpp_stanza_set_name(node, "show");
        xmpp_stanza_set_text(text, "away");
        xmpp_stanza_add_child(node, text);
        xmpp_stanza_release(text);
        xmpp_stanza_add_child(presence, node);
        xmpp_stanza_release(node);
    }

    if (msg) {
        node = xmpp_stanza_new(ctx);
        text = xmpp_stanza_new(ctx);
        xmpp_stanza_set_name(node, "status");
        xmpp_stanza_set_text(text, msg);
        xmpp_stanza_add_child(node, text);
        xmpp_stanza_release(text);
        xmpp_stanza_add_child(presence, node);
        xmpp_stanza_release(node);
    }

    xmpp_send(conn, presence);
    xmpp_stanza_release(presence);

    state->available = new_status;
}

xmpp_stanza_t* iq(xmpp_ctx_t *const ctx, const char *type,
    const char *to, const char *id)
{
    xmpp_stanza_t *node = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(node, "iq");
    xmpp_stanza_set_type(node, type);
    if (id) {
        xmpp_stanza_set_attribute(node, "id", id);
    }
    if (to) {
        xmpp_stanza_set_attribute(node, "to", to);
    }
    return node;
}

void reply_text(struct XMPPState *const state, xmpp_stanza_t *const orig, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *formatted = vawesomef(fmt, args, 0);
    va_end(args);

    xmpp_ctx_t *const ctx = state->ctx;
    char *to = xmpp_stanza_get_attribute(orig, "from");

    xmpp_stanza_t *reply, *body, *text;
    reply = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(reply, "message");
    xmpp_stanza_set_type(reply, "chat");
    xmpp_stanza_set_attribute(reply, "to", to);

    body = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(body, "body");

    text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(text, formatted);
    free(formatted);

    xmpp_stanza_add_child(body, text);
    xmpp_stanza_release(text);
    xmpp_stanza_add_child(reply, body);
    xmpp_stanza_release(body);

    xmpp_send(state->conn, reply);
    xmpp_stanza_release(reply);
}

xmpp_stanza_t *iq_error(xmpp_ctx_t *const ctx,
                        xmpp_stanza_t *const in_reply_to,
                        const char *type,
                        const char *error_condition,
                        const char *text)
{
    xmpp_stanza_t *iq_error = iq(ctx,
                                 "error",
                                 xmpp_stanza_get_attribute(in_reply_to, "from"),
                                 xmpp_stanza_get_attribute(in_reply_to, "id"));
    xmpp_stanza_t *error = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(error, "error");
    xmpp_stanza_set_attribute(error, "type", type);

    xmpp_stanza_t *forbidden = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(forbidden, error_condition);
    xmpp_stanza_set_ns(forbidden, "urn:ietf:params:xml:ns:xmpp-stanzas");
    xmpp_stanza_add_child(error, forbidden);
    xmpp_stanza_release(forbidden);

    if (text) {
        xmpp_stanza_t *text_cont = xmpp_stanza_new(ctx);
        xmpp_stanza_set_name(text_cont, "text");

        xmpp_stanza_t *text_node = xmpp_stanza_new(ctx);
        xmpp_stanza_set_text(text_node, text);
        xmpp_stanza_add_child(text_cont, text_node);
        xmpp_stanza_release(text_node);

        xmpp_stanza_add_child(error, text_cont);
        xmpp_stanza_release(text_cont);
    }

    xmpp_stanza_add_child(iq_error, error);
    xmpp_stanza_release(error);

    return iq_error;
}
