# pa-sink-ctl - NCurses based Pulseaudio control client
# Copyright (C) 2011  Benjamin Franzke <benjaminfranzke@googlemail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

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

AC_CHECK_HEADERS([ncurses.h sys/ioctl.h sys/signalfd.h unistd.h signal.h])

AC_CHECK_FUNC([sigaction], [], AC_MSG_ERROR([function sigaction() not found]))
AC_CHECK_FUNCS([signalfd])

AM_CONDITIONAL([HAVE_SIGNALFD],
	       [test "x$ac_cv_func_signalfd" = "xyes" -a "x$ac_cv_header_sys_signalfd_h" = "xyes"])

AC_DEFINE([_POSIX_C_SOURCE], [1], [Enable POSIX.1-1990 definitions])

AC_C_INLINE

AC_OUTPUT
