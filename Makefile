BIN=pa-sink-ctl
SRCS=interface.c sink.c sink_input.c pa-sink-ctl.c

OBJS=$(SRCS:%.c=%.o)
HEADS=$(SRCS:%.c=%.h)

CC=gcc
CFLAGS=-std=c99 -Wall -Werror -pedantic
LDFLAGS=-lncurses -lpulse -lform

DEPENDFILE=.depend

all: $(DEPENDFILE) $(BIN)
.PHONY: all clean

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(DEPENDFILE): $(SRCS) $(HEADS)
	$(CC) -MM $(SRCS) > $(DEPENDFILE)

-include $(DEPENDFILE)

clean:
	rm -f $(OBJS) $(DEPENDFILE)
distclean: clean
	rm -f $(BIN)
