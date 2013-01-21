#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

struct Config* config_new();
void config_init(struct Config *obj);

int config_load(const char *config_file, struct Config *dest);
int config_load_file(FILE *config_file, struct Config *dest);
int config_load_buf(const char *buf, size_t len, struct Config *dest);

void config_burn(struct Config *obj);
void config_free(struct Config *obj);

#endif
