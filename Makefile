CFLAGS=-g -Wall -Wextra -pedantic
LIBS=

main: main.c
	$(CC) $(CFLAGS) $(LIBS) -o cemu main.c
