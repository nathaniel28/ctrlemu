CFLAGS=-g -Wall -Wextra -pedantic
LIBS=

main: main.c
	$(CC) $(CFLAGS) $(LIBS) -o ctrlemu main.c
