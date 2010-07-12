bin_PROGRAMS = pa_sink_ctl
pa_sink_ctl_SOURCES =interface.c pa-sink-ctl.c sink.c sink_input.c

AM_CFLAGS  =-std=c99 -pedantic -Wall -Werror @PULSE_CFLAGS@ 
pa_sink_ctl_LDADD   =@PULSE_LIBS@ @CURSES_LIBS@

noinst_HEADERS = interface.h pa-sink-ctl.h sink.h sink_input.h
