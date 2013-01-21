#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define CONFIG_CHUNK_SIZE 1024

struct Config* config_new() {
    struct Config* ret = malloc(sizeof(struct Config));
    if (!ret)
        return ret;
    config_init(ret);
    return ret;
}

void config_init(struct Config *obj) {
    memset(obj, 0, sizeof(struct Config));
}

int config_load(const char *config_file, struct Config *dest) {
    FILE *config = fopen(config_file, "r");
    if (config == NULL) {
        return -1;
    }
    int err = config_load_file(config, dest);
    int error_number = (err != 0?errno:0);
    fclose(config);

    if (err != 0) {
        errno = error_number;
        return err;
    }
    return 0;
}

int config_parse_text_stanza(char **const dest,
    xmpp_stanza_t *stanza)
{
    xmpp_stanza_t *const text_child = xmpp_stanza_get_children(stanza);
    if (!xmpp_stanza_is_text(text_child)) {
        return 1;
    }
    const char *text = xmpp_stanza_get_text_ptr(text_child);
    *dest = strdup(text);
    return 0;
}

void config_parse_stanza(xmpp_stanza_t *stanza, void * const userdata) {
    struct Config *const dest = (struct Config*)userdata;

    const char *name = xmpp_stanza_get_name(stanza);
    if (strcmp(name, "jid") == 0) {
        if (config_parse_text_stanza(&dest->jid, stanza) != 0) {
            fprintf(stderr, "CONFIG: invalid tag: %s", name);
        }
    } else if (strcmp(name, "pass") == 0) {
        if (config_parse_text_stanza(&dest->pass, stanza) != 0) {
            fprintf(stderr, "CONFIG: invalid tag: %s", name);
        }
    } else if (strcmp(name, "ping-peer") == 0) {
        if (config_parse_text_stanza(&dest->ping_peer, stanza) != 0) {
            fprintf(stderr, "CONFIG: invalid tag: %s", name);
        }
    } else if (strcmp(name, "device") == 0) {
        if (config_parse_text_stanza(&dest->serial_file, stanza) != 0) {
            fprintf(stderr, "CONFIG: invalid tag: %s", name);
        }
    } else {
        fprintf(stderr, "CONFIG: unknown tag: %s", name);
    }
}

int config_load_file(FILE *config_file, struct Config *dest) {
    char *rdbuf = malloc(CONFIG_CHUNK_SIZE);
    int ret = -1;
    xmpp_ctx_t *ctx = xmpp_ctx_new(NULL, NULL);
    parser_t *parser = parser_new(ctx,
        NULL,
        NULL,
        config_parse_stanza,
        dest);

    size_t read_bytes;
    do {
        read_bytes = fread(rdbuf, 1, CONFIG_CHUNK_SIZE, config_file);
        if (!parser_feed(parser, rdbuf, read_bytes)) {
            ret = 2;
            goto __cleanup__;
        }
    } while (read_bytes == CONFIG_CHUNK_SIZE);
    if (errno != 0) {
        ret = 1;
    } else {
        ret = 0;
    }

__cleanup__:
    free(rdbuf);
    parser_free(parser);
    xmpp_ctx_free(ctx);
    return ret;
}

void config_burn(struct Config *obj) {
    if (obj->jid)
        free(obj->jid);
    if (obj->pass)
        free(obj->pass);
    if (obj->ping_peer)
        free(obj->ping_peer);
}

void config_free(struct Config *obj) {
    config_burn(obj);
    free(obj);
}
