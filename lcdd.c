#define _GNU_SOURCE

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include <libcouplet/couplet.h>

#include "common.h"
#include "config.h"
#include "utils.h"
#include "xmpp-utils.h"
#include "commands.h"
#include "display.h"

static const char *ping_ns = "urn:xmpp:ping";
static const char *sensor_ns = "http://xmpp.sotecware.net/xmlns/sensor";

/**********************************************************************/

static const struct {
    char *config_file;

    unsigned long ping_interval;
    unsigned long ping_timeout_interval;
    unsigned long reconnect_interval;
    unsigned long device_check_interval;
    unsigned long page_cycle_interval;
    unsigned long sensor_check_interval;

    char *authorized_jids[];

} static_config = {
    config_file: "cfg.xml",
    authorized_jids: {
        "jonas@zombofant.net/",
        "dvbbot@hub.sotecware.net/",
        "agent@hub.sotecware.net/",
        NULL
    },
    ping_interval: 15000,
    ping_timeout_interval: 5000,
    reconnect_interval: 300,
    device_check_interval: 5000,
    page_cycle_interval: 5000,
    sensor_check_interval: 1000
};

/** Authorization */

int is_authorized(const char *jid)
{
    for (   char *const*authed_jid = &static_config.authorized_jids[0];
            *authed_jid != NULL;
            authed_jid++) {
        if (strncmp(jid, *authed_jid, strlen(*authed_jid)) == 0) {
            return 1;
        }
    }
    return 0;
}

/** Misc */

void check_device(struct State *state) {
    int err = display_open(state);
    if (err != 0) {
        set_presence(&state->xmpp_state, 0, "LCD is not plugged in.");
    } else {
        set_presence(&state->xmpp_state, 1, "LCD detected");
    }
}

/** Timers */

int handle_device_check(xmpp_conn_t *const conn, void *const userdata) {
    struct State *state = (struct State*)userdata;
    check_device(state);
    return 1;
}

int handle_ping_timeout(xmpp_conn_t *const conn, void *const userdata);
int handle_ping_reply(xmpp_conn_t *const conn, xmpp_stanza_t *const stanza, void *const userdata);

int handle_send_ping(xmpp_conn_t *const conn, void *const userdata) {
    struct State *state = (struct State*)userdata;
    assert(!state->xmpp_state.ping_pending);

    static char pingidbuf[127];

    xmpp_ctx_t *ctx = state->xmpp_state.ctx;

    memset(pingidbuf, 0, 127);
    sprintf(pingidbuf, "ping%d", state->xmpp_state.next_ping_id++);

    xmpp_stanza_t *iq_stanza = iq(ctx,
        "get",
        state->config.ping_peer,
        pingidbuf
    );
    xmpp_stanza_t *ping = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(ping, "ping");
    xmpp_stanza_set_ns(ping, ping_ns);
    xmpp_stanza_add_child(iq_stanza, ping);
    xmpp_stanza_release(ping);

    state->xmpp_state.ping_pending = 1;
    xmpp_id_handler_add(conn, handle_ping_reply, pingidbuf, userdata);
    xmpp_timed_handler_add(conn, handle_ping_timeout, static_config.ping_timeout_interval, userdata);

    xmpp_send(conn, iq_stanza);
    xmpp_stanza_release(iq_stanza);

    return 0;
}

int handle_ping_timeout(xmpp_conn_t *const conn, void *const userdata) {
    struct XMPPState *state = &((struct State*)userdata)->xmpp_state;
    if (!state->ping_pending) {
        return 0;
    }

    debug_msg("ping timeout\n");
    xmpp_disconnect(conn);
    state->ping_pending = 0;

    return 0;
}

int handle_page_cycle(xmpp_conn_t *const conn, void *const userdata) {
    struct State *state = (struct State*)userdata;
    state->display_state.curr_page = (state->display_state.curr_page + 1) % PAGE_COUNT;
    // check_device();
    display_redraw_page(state);
    xmpp_timed_handler_add(conn,
        state->display_state.page_cycle_handler,
        state->display_state.page_cycle_interval,
        userdata);
    return 0;
}

int handle_sensor_check(xmpp_conn_t *const conn, void *const userdata) {
    struct State *state = (struct State*)userdata;
    struct pollfd fd;
    fd.fd = state->serial_state.fd;
    fd.events = POLLIN;

    struct SensorOnTheWire raw;

    while (poll(&fd, 1, 0) == 1) {
        if ((fd.revents & POLLIN) == POLLIN) {
            memset(&raw, 0, sizeof(raw));
            raw.id = 0xff;

            if (block_read(fd.fd, &raw, sizeof(raw)) < sizeof(raw)) {
                debug_msg("block_read for sensor data interrupted early.\n");
                return 1;
            }

            uint8_t checksum = adler8ish((const uint8_t*)&raw, sizeof(raw)-sizeof(uint8_t));
            if (checksum != raw.checksum) {
                debug_msg("checksum mismatch in sensor data, attempting resync.\n");
                fprintf(stderr, "got = %d; expected = %d\n", raw.checksum, checksum);
                fflush(stderr);
                display_resync(state);
                return 1;
            }

            if (raw.id >= SENSOR_COUNT) {
                fprintf(stderr, "DEBUG: sensor id out of bounds: %d\n", raw.id);
                fflush(stderr);
                return 1;
            }

            uint16_t value_int = 0;
            value_int |= ((uint16_t)raw.value[1] << 8) & 0xFF00;
            value_int |= ((uint16_t)raw.value[0]) & 0xFF;
            state->sensors[raw.id].known = 1;
            memcpy(state->sensors[raw.id].addr, raw.addr, sizeof(raw.addr));
            state->sensors[raw.id].value = (int16_t)value_int;
        }
    }
    return 1;
}

int handle_check_signals(xmpp_conn_t *const conn, void *const userdata) {
    struct State *state = (struct State*)userdata;
    if (state->terminated) {
        xmpp_disconnect(conn);
        return 0;
    } else {
        return 1;
    }
}

/** XMPP stanza handlers */

int handle_sensor_request(xmpp_conn_t *const conn,
                          xmpp_stanza_t *const stanza,
                          void *const userdata)
{
    struct State *state = (struct State*)userdata;
    xmpp_ctx_t *const ctx = state->xmpp_state.ctx;

    const char *from = xmpp_stanza_get_attribute(stanza, "from");
    if (!is_authorized(from)) {
        xmpp_stanza_t *error = iq_error(
            ctx,
            stanza,
            "cancel",
            "forbidden",
            NULL
            );
        xmpp_send(conn, error);
        xmpp_stanza_release(error);
        return 1;
    }

    const char *type = xmpp_stanza_get_attribute(stanza, "type");
    if (strcmp(type, "get") != 0) {
        xmpp_stanza_t *error = iq_error(
            ctx,
            stanza,
            "cancel",
            "bad-request",
            NULL
            );
        xmpp_send(conn, error);
        xmpp_stanza_release(error);
        return 1;
    }

    xmpp_stanza_t *response = iq(ctx, "result", from,
                                 xmpp_stanza_get_attribute(stanza, "id"));
    xmpp_stanza_t *query = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(query, "query");
    xmpp_stanza_set_ns(query, sensor_ns);

    for (int i = 0; i < SENSOR_COUNT; i++) {
        struct SensorState *sensor_state = &state->sensors[i];
        if (!sensor_state->known) {
            continue;
        }

        xmpp_stanza_t *sensor = xmpp_stanza_new(ctx);
        xmpp_stanza_set_name(sensor, "sensor");
        xmpp_stanza_set_ns(sensor, sensor_ns);

        xmpp_stanza_set_attribute(sensor, "available", "true");
        char *buf = awesomef("%d", sensor_state->value);
        xmpp_stanza_set_attribute(sensor, "value", buf);
        free(buf);

        char addrbuf[17];
        memset(addrbuf, 0, sizeof(addrbuf));
        encode_hex(addrbuf, sensor_state->addr, 8);
        xmpp_stanza_set_attribute(sensor, "serial", addrbuf);

        xmpp_stanza_add_child(query, sensor);
        xmpp_stanza_release(sensor);
    }

    xmpp_stanza_add_child(response, query);
    xmpp_stanza_release(query);
    xmpp_send(conn, response);
    xmpp_stanza_release(response);

    return 1;
}

int handle_version(xmpp_conn_t *const conn,
    xmpp_stanza_t *const stanza,
    void *const userdata)
{
    struct XMPPState *state = &((struct State*)userdata)->xmpp_state;
    xmpp_ctx_t *ctx = state->ctx;
    const char *from = xmpp_stanza_get_attribute(stanza, "from");

    xmpp_stanza_t *query, *name, *version, *text;
    xmpp_stanza_t *reply = iq(ctx, "result", from, xmpp_stanza_get_id(stanza));

    query = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(query, "query");
    char *ns = xmpp_stanza_get_ns(xmpp_stanza_get_children(stanza));
    if (ns) {
        xmpp_stanza_set_ns(query, ns);
    }

    name = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(name, "name");
    xmpp_stanza_add_child(query, name);
    xmpp_stanza_release(name);

    text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(text, "fritzbox xmpp lcd service");
    xmpp_stanza_add_child(name, text);
    xmpp_stanza_release(text);

    version = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(version, "version");
    xmpp_stanza_add_child(query, version);
    xmpp_stanza_release(version);

    text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(text, "23.42");
    xmpp_stanza_add_child(version, text);
    xmpp_stanza_release(text);

    xmpp_stanza_add_child(reply, query);
    xmpp_stanza_release(query);

    xmpp_send(conn, reply);
    xmpp_stanza_release(reply);
    return 1;
}

int handle_message(xmpp_conn_t *const conn,
    xmpp_stanza_t *const stanza,
    void *const userdata)
{
    struct State *const state = (struct State*)userdata;
    xmpp_ctx_t *ctx = state->xmpp_state.ctx;

    xmpp_stanza_t *body = xmpp_stanza_get_child_by_name(stanza, "body");

    if (!body) {
        // not an actual message
        return 1;
    }

    if (!strcmp(xmpp_stanza_get_attribute(stanza, "type"), "error")) {
        // some error message -- better we don't reply
        return 1;
    }

    const char *from = xmpp_stanza_get_attribute(stanza, "from");

    if (!is_authorized(from)) {
        reply_text(&state->xmpp_state, stanza, "You are not authorized.");
        return 1;
    }

    char *intext = xmpp_stanza_get_text(body);

    if (strncmp(intext, "?OTR:", 5) == 0) {
        goto __cleanup__;
    }

    const size_t textlen = strlen(intext);

    const struct Command *cmd = &commands[0];
    size_t cmdlen;
    for (; cmd->name != NULL; cmd++) {
        cmdlen = strlen(cmd->name);
        if (cmdlen > textlen) {
            continue;
        }

        if (strncmp(cmd->name, intext, cmdlen) == 0) {
            if (textlen == cmdlen || (intext[cmdlen] == ' ')) {
                // we found the matching command
                break;
            }
        }
    }
    if (cmd->name == NULL) {
        reply_text(&state->xmpp_state, stanza, "Unknown command.");
        return 1;
    }

    char *arg_str;
    if (textlen == cmdlen) {
        arg_str = NULL;
    } else {
        arg_str = &intext[cmdlen+1];
    }
    cmd->handler(state, stanza, arg_str);

__cleanup__:
    xmpp_free(ctx, intext);
    return 1;
}

int handle_ping_reply(xmpp_conn_t *const conn,
    xmpp_stanza_t *const stanza,
    void *const userdata)
{
    struct State *state = (struct State*)userdata;
    if (!state->xmpp_state.ping_pending) {
        return 0;
    }

    state->xmpp_state.ping_pending = 0;
    xmpp_timed_handler_delete(conn, handle_ping_timeout);
    xmpp_timed_handler_add(conn, handle_send_ping, static_config.ping_interval, userdata);
    return 0;
}


/** Connection management and main loop */

void conn_state_changed(xmpp_conn_t * const conn,
    const xmpp_conn_event_t status,
    const int error,
    xmpp_stream_error_t * const stream_error,
    void * const userdata)
{
    struct State *state = (struct State*)userdata;

    switch (status) {
    case XMPP_CONN_CONNECT:
        debug_msg("connected\n");
        xmpp_handler_add(
            conn,
            handle_version,
            "jabber:iq:version",
            "iq",
            0,
            userdata
        );
        xmpp_handler_add(
            conn,
            handle_sensor_request,
            sensor_ns,
            "iq",
            "get",
            userdata
        );
        xmpp_handler_add(
            conn,
            handle_message,
            NULL,
            "message",
            0,
            userdata
        );
        xmpp_timed_handler_add(
            conn,
            handle_device_check,
            static_config.device_check_interval,
            userdata
        );
        xmpp_timed_handler_add(
            conn,
            handle_send_ping,
            static_config.ping_interval,
            userdata
        );
        xmpp_timed_handler_add(
            conn,
            handle_check_signals,
            1000,
            userdata
        );
        xmpp_timed_handler_add(
            conn,
            handle_sensor_check,
            static_config.sensor_check_interval,
            userdata
        );
        if (state->display_state.page_cycling) {
            xmpp_timed_handler_add(
                conn,
                state->display_state.page_cycle_handler,
                state->display_state.page_cycle_interval,
                userdata
            );
        }

        state->xmpp_state.available = -1;
        set_presence(&state->xmpp_state, 0, "initializing...");
        check_device((struct State*)userdata);
        break;
    case XMPP_CONN_DISCONNECT:
    case XMPP_CONN_FAIL:
        debug_msg("disconnecting\n");
        xmpp_stop(state->xmpp_state.ctx);
        break;
    default:
        break;
    };
}

int *terminated = 0, sighupd = 0;
struct XMPPState *signal_state;

void handle_sighup(int signum)
{
    fprintf(stderr, "SIGHUP received -- reconnecting\n");
    if (signal_state->conn) {
        xmpp_disconnect(signal_state->conn);
    }
    sighupd = 1;
}

void handle_sigint(int signum)
{
    fprintf(stderr, "SIGTERM/SIGINT received -- terminating\n");
    *terminated = 1;
}

void assert_config(char *ptr, const char *name)
{
    if (!ptr) {
        fprintf(stderr, "ERROR: config key %s not found\n", name);
        exit(2);
    }
}

int main(int argc, char **argv) {
    assert(sizeof(struct SensorOnTheWire) == 12);

    struct State state;
    terminated = &state.terminated;
    signal_state = &state.xmpp_state;

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    signal(SIGHUP, handle_sighup);

    memset(&state, 0, sizeof(struct State));
    config_init(&state.config);

    int err = config_load(static_config.config_file, &state.config);
    if (err != 0) {
        int error_number = errno;
        fprintf(stderr, "ERROR: could not load config (%d:%d)\n", err, error_number);
        exit(2);
    };

    assert_config(state.config.jid, "jid");
    assert_config(state.config.pass, "pass");
    assert_config(state.config.ping_peer, "ping-peer");
    assert_config(state.config.serial_file, "device");

    printf("%s\n", state.config.jid);
    printf("%s\n", state.config.pass);

    state.serial_state.fd = -1;
    state.display_state.page_cycling = 1;
    state.display_state.page_cycle_handler = handle_page_cycle;
    state.display_state.page_cycle_interval = static_config.page_cycle_interval;

    for (int i = 0; i < PAGE_COUNT; i++) {
        display_clear_page(&state, i);
    }

    xmpp_initialize();

    xmpp_log_t *log = xmpp_get_default_logger(XMPP_LEVEL_ERROR);

    xmpp_ctx_t *ctx = xmpp_ctx_new(NULL, log);

    while (1) {
        memset(&state.xmpp_state, 0, sizeof(struct XMPPState));
        state.xmpp_state.ctx = ctx;
        state.xmpp_state.running = 1;

        state.xmpp_state.conn = xmpp_conn_new(ctx);
        xmpp_conn_t *conn = state.xmpp_state.conn;

        xmpp_conn_set_jid(conn, state.config.jid);
        xmpp_conn_set_pass(conn, state.config.pass);

        debug_msg("connecting\n");
        xmpp_connect_client(conn, NULL, 0, conn_state_changed, &state);

        debug_msg("entering main loop\n");
        xmpp_resume(ctx);
        debug_msg("exited main loop\n");

        if (state.xmpp_state.running) {
            xmpp_disconnect(conn);
        }

        xmpp_conn_release(conn);

        // reset_serial(&state.serial_state);
        if (state.terminated) {
            break;
        }
        if (!sighupd) {
            debug_msg("reconnecting soon\n");
            sleep(static_config.reconnect_interval);
            if (state.terminated) {
                break;
            }
        } else {
            sighupd = 0;
            debug_msg("SIGHUP'd, reconnecting immediately\n");
        }
    };

    xmpp_ctx_free(ctx);
    xmpp_shutdown();
    config_burn(&state.config);

    return 0;
}
