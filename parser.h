#ifndef PARSER_H
#define PARSER_H

#include <linux/uinput.h>

typedef struct {
	int type, code, value;
} key;

extern key keymap[KEY_MAX];

extern int parse(const char *path);

#endif
