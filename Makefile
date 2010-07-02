CC=gcc
CFLAGS=-pedantic -std=c99 -Wall -Werror -lpulse

all: pa-sink-ctl.c
	$(CC) $(CFLAGS) pa-sink-ctl.c -o pa-sink-ctl
