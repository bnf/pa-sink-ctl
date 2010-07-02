CC=gcc
CFLAGS=-pedantic -std=c99 -Wall -Werror -lpulse -lncurses -lform

all: pa-sink-ctl.c
	$(CC) $(CFLAGS) pa-sink-ctl.c -o pa-sink-ctl

test:
	$(CC) $(CFLAGS) nc-test.c -o nc-test
