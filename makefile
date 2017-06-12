CC = gcc
CFLAGS = -Wall
CFLAGS += -std=c99
CFLAGS += -D_GNU_SOURCE

smallsh:
	${CC} smallsh.c -o smallsh ${CFLAGS}

.PHONY: clean
clean:
	rm smallsh