#AC_PREREQ([2.65])
AC_INIT([pa-sink-ctl], [0.1], [web2p10@wemelug.de])
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([pa-sink-ctl.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([ncurses], [newwin], [CURSES=-lncurses], [echo "Error: you need ncurses!"; exit 1])
PKG_CHECK_MODULES(PULSE, [libpulse], [], [echo "Error: you need libpulse!"; exit 1])

# compiler flags
LDFLAGS="$CURSES $PULSE_LIBS"
CFLAGS="-std=c99 -pedantic -Wall -Werror $PULSE_CFLAGS"

# Checks for header files.
AC_CHECK_HEADERS([stdio.h stdlib.h string.h ncurses.h pulse/pulseaudio.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_REALLOC
AC_CHECK_FUNCS([strdup])

AC_OUTPUT
