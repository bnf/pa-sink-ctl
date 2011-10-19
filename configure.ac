#AC_PREREQ([2.65])
AC_INIT([pa-sink-ctl], [20110223], [web2p10@wemelug.de])
AM_INIT_AUTOMAKE([1.11 foreign dist-bzip2])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([src/pa-sink-ctl.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([ncurses], [newwin], [CURSES_LIBS=-lncurses], AC_MSG_ERROR([curses required]))

PKG_CHECK_MODULES(PULSE, [libpulse], [], AC_MSG_ERROR([libpulse required]))
PKG_CHECK_MODULES(PULSE_MAINLOOP, [libpulse-mainloop-glib], [], AC_MSG_ERROR([libpulse-mainloop-glib required]))
PKG_CHECK_MODULES(GLIB, [glib-2.0], [], AC_MSG_ERROR([glib required]))

if test "x$GCC" = "xyes"; then
	GCC_CFLAGS="-Wall -Werror -pedantic -std=c99 -Wstrict-prototypes -Wmissing-prototypes -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)


AC_SUBST(CURSES_LIBS)

AC_CHECK_HEADERS([ncurses.h sys/ioctl.h unistd.h signal.h])

AC_CHECK_FUNC([sigaction], [], AC_MSG_ERROR([function sigaction() not found]))

AC_C_INLINE

AC_OUTPUT
