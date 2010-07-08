CC=gcc
CFLAGS=-pedantic -std=c99 -Wall -Werror -lpulse -lncurses -lform

all: pa-sink-ctl.o interface.o sink_input.o sink.o
	$(CC) $(CFLAGS) -o pa-sink-ctl pa-sink-ctl.o interface.o sink_input.o sink.o

pa-sink-ctl.o: pa-sink-ctl.c pa-sink-ctl.h
	$(CC) $(CFLAGS) -c pa-sink-ctl.c -o pa-sink-ctl.o

interface: interface.c interface.h
	$(CC) $(CFLAGS) -c interface.c -o interface.o

sink_input: sink_input.c sink_input.h
	$(CC) $(CFLAGS) -c sink_input.c -o sink_input.o

sink: sink.c sink.h
	$(CC) $(CFLAGS) -c sink.c -o sink.o

clean:
	rm *.o pa-sink-ctl

test:
	$(CC) $(CFLAGS) nc-test.c -o nc-test
