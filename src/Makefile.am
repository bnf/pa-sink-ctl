bin_PROGRAMS = pa-sink-ctl
pa_sink_ctl_SOURCES = interface.c command.c config.c pa-sink-ctl.c

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS = $(PULSE_CFLAGS) $(PULSE_MAINLOOP_CFLAGS) $(GLIB_CFLAGS) \
	      -include $(top_builddir)/config.h
pa_sink_ctl_LDADD = $(GLIB_LIBS) $(PULSE_LIBS) $(PULSE_MAINLOOP_LIBS) $(CURSES_LIBS)

noinst_HEADERS = interface.h command.h config.h pa-sink-ctl.h ctl.h

dist_man_MANS = pa-sink-ctl.1
EXTRA_DIST = pa-sink-ctl.1.txt

%: %.txt
	a2x -f manpage $<

%.html: %.txt
	a2x -f xhtml -d manpage --stylesheet=pa-sink-ctl-manpage.css $<
