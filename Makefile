BIN=pa-sink-ctl
OBJS=pa-sink-ctl.o interface.o sink.o sink_input.o

CC=gcc
CFLAGS=-std=c99 -Wall -Werror -pedantic
LDFLAGS=-lncurses -lpulse -lform

all: $(BIN)
.PHONY: all

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BIN) $(OBJS)
