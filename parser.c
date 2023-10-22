#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "names.h"
#include "parser.h"

key keymap[KEY_MAX];

int nv_cmp(const void *a, const void *b) {
	char *n1 = ((macro *) a)->name;
	char *n2 = ((macro *) b)->name;
	return strcmp(n1, n2);
}

// ignoring preceding whitespace (a ' ' or '\t') write a word to buf, stopping
// after finding a ' ' (the end of a word), an EOF, or a character in terms
int parse_word(FILE *fp, const char *terms) {
	char buf[64];
	size_t pos = 0;

	int ch;

	// trim spaces
	do {
		ch = fgetc(fp);
		if (ch == EOF) {
			return -1;
		}
	} while (ch == ' ' || ch == '\t');

	// get name or number
	do {
		if (pos >= sizeof buf - 1) {
			// out of space in the buffer, since it needs to
			// account for a null-terminating byte too
			return -1;
		}
		buf[pos++] = ch;
		ch = fgetc(fp);
	} while (ch != ' ' && ch != EOF && !strchr(terms, ch));
	buf[pos] = '\0';

	// decode name or number
	if (buf[0] >= '0' && buf[0] <= '9') {
		int res;
		if (sscanf(buf, "%d", &res) == 1) {
			return res;
		}
	} else {
		macro search;
		search.name = buf;
		macro *res = bsearch(&search, macros, sizeof macros / sizeof *macros, sizeof *macros, &nv_cmp);
		if (res) {
			return res->code;
		}
		printf("unknown macro %s\n", buf);
	}
	return -1;
}

// Parsing is very permissive with whitespace and perhaps too permisive overall.
int parse_file(FILE *fp) {
	for (;;) {
		int key = parse_word(fp, ":");
		if (key == -1) {
			if (feof(fp)) {
				return 0;
			}
			printf("failed to read key\n");
			return -1;
		}
		assert((size_t) key < sizeof keymap / sizeof *keymap);

		keymap[key].type = parse_word(fp, "");
		if (keymap[key].type == -1) {
			printf("failed to read type\n");
			return -1;
		}

		keymap[key].code = parse_word(fp, "");
		if (keymap[key].code == -1) {
			printf("failed to read code\n");
			return -1;
		}

		keymap[key].value = parse_word(fp, "\n");
		if (keymap[key].value == -1) {
			printf("failed to read value\n");
			return -1;
		}

		printf("%d: %d %d %d\n", key, keymap[key].type,
		       keymap[key].code, keymap[key].value);
	}

	return 0;
}

int parse(const char *path) {
	FILE *fp = fopen(path, "r");
	if (!fp) {
		printf("failed to open %s\n", path);
		return -1;
	}

	int err = parse_file(fp);
	if (err) {
		printf("failed to parse %s\n", path);
	}

	fclose(fp);
	return err;
}
