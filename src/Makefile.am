bin_PROGRAMS = pa-sink-ctl
pa_sink_ctl_SOURCES = interface.c pa-sink-ctl.c sink.c sink_input.c g_unix_signal.c g_curses_input.c

AM_CFLAGS = -std=c99 -pedantic -Wall -Werror @PULSE_CFLAGS@ @PULSE_MAINLOOP_CFLAGS@ @GLIB_CFLAGS@
pa_sink_ctl_LDADD = @GLIB_LIBS@ @PULSE_LIBS@ @PULSE_MAINLOOP_LIBS@ @CURSES_LIBS@

noinst_HEADERS = interface.h pa-sink-ctl.h sink.h sink_input.h
