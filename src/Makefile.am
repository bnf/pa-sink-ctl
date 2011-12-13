bin_PROGRAMS = pa-sink-ctl
pa_sink_ctl_SOURCES = interface.c command.c config.c pa-sink-ctl.c
EXTRA_pa_sink_ctl_SOURCES = unix_signal.c

if !HAVE_SIGNALFD
pa_sink_ctl_SOURCES += unix_signal.c
endif

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS = $(PULSE_CFLAGS) $(PULSE_MAINLOOP_CFLAGS) $(GLIB_CFLAGS) \
	      -include $(top_builddir)/config.h
pa_sink_ctl_LDADD = $(GLIB_LIBS) $(PULSE_LIBS) $(PULSE_MAINLOOP_LIBS) $(CURSES_LIBS)

noinst_HEADERS = interface.h command.h config.h pa-sink-ctl.h sink.h unix_signal.h

pa_sink_ctl_xdgdir = $(sysconfdir)/xdg/pa-sink-ctl
dist_pa_sink_ctl_xdg_DATA = config.ini
