bin_PROGRAMS = pa-sink-ctl
pa_sink_ctl_SOURCES = interface.c pa-sink-ctl.c sink.c sink_input.c unix_signal.c

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS = $(PULSE_CFLAGS) $(PULSE_MAINLOOP_CFLAGS) $(GLIB_CFLAGS)
pa_sink_ctl_LDADD = $(GLIB_LIBS) $(PULSE_LIBS) $(PULSE_MAINLOOP_LIBS) $(CURSES_LIBS)

noinst_HEADERS = interface.h pa-sink-ctl.h sink.h sink_input.h unix_signal.h
