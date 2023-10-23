#ifndef PARSER_H
#define PARSER_H

#include <linux/uinput.h>

typedef struct {
	int type, code, value;

	int opposite;
	_Bool holding;
	// Holding is only read from keys of type EV_ABS (but written for all
	// keys), because since EV_ABSs are analog, they have an opposite
	// direction, and when a user unholds an EV_ABS, the program must
	// determine if the user is still holding the opposite direction. If
	// they are, output that direction instead of outputting an unhold
	// event. See handle_code() in main.c
} key;

extern key keymap[KEY_MAX];

extern int parse(const char *path);

#endif
