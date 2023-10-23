CFLAGS=-g -Wall -Wextra -pedantic
LIBS=

OBJS=main.o parser.o

main: $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) $(OBJS) -o ctrlemu

names:
	python gen.py > names.h

all:
	rm -f *.o
	make names
	make depend
	make main

SRCS=main.c parser.c
depend Makefile.d:
	$(CC) -MM $(SRCS) | grep : > Makefile.d
