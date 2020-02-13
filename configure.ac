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
AC_INIT([pa-sink-ctl], [2], [benjaminfranzke@googlemail.com])
AM_INIT_AUTOMAKE([1.11 foreign dist-xz])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([src/pa-sink-ctl.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile data/Makefile])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_CHECK_HEADERS([sys/ioctl.h sys/signalfd.h unistd.h signal.h])

AC_CHECK_FUNCS([signalfd])

AM_CONDITIONAL([HAVE_SIGNALFD],
	       [test "x$ac_cv_func_signalfd" = "xyes" -a "x$ac_cv_header_sys_signalfd_h" = "xyes"])

glib_version=2.0
if test "x$ac_cv_func_signalfd" = "xno" -o "x$ac_cv_header_sys_signalfd_h" = "xno"; then
	# If signalfd() is not available, we want to use g_unix_signal_add()
	# which is supported for SIGWINCH as of 2.54, see
	# https://github.com/GNOME/glib/commit/47a02c85610f4036681c9728b7339d
	glib_version=2.54
fi

# Checks for libraries.
PKG_CHECK_MODULES(CURSES, [ncursesw], [], [ncursesw=no])
PKG_CHECK_MODULES(PULSE, [libpulse], [], AC_MSG_ERROR([libpulse required]))
PKG_CHECK_MODULES(PULSE_MAINLOOP, [libpulse-mainloop-glib], [], AC_MSG_ERROR([libpulse-mainloop-glib required]))
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= $glib_version], [], AC_MSG_ERROR([glib >= $glib_version required]))

if test "x$ncursesw" = "xno"; then
	AC_CHECK_LIB([curses], [newwin], [CURSES_LIBS=-lcurses], AC_MSG_ERROR([ncursesw or curses required]))
	AC_CHECK_HEADERS([curses.h])
	AC_SUBST(CURSES_LIBS)
fi

if test "x$GCC" = "xyes"; then
	GCC_CFLAGS="-Wall -pedantic -std=c99 -W -Wextra -pipe -Wno-long-long -Winline -Wvla -Wno-overlength-strings -Wunsafe-loop-optimizations -Wundef -Wformat=2 -Wlogical-op -Wsign-compare -Wformat-security -Wmissing-include-dirs -Wformat-nonliteral -Wold-style-definition -Wpointer-arith -Winit-self -Wdeclaration-after-statement -Wfloat-equal -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wmissing-declarations -Wmissing-noreturn -Wshadow -Wendif-labels -Wcast-align -Wstrict-aliasing=2 -Wwrite-strings -Wno-unused-parameter -ffast-math -Wp,-D_FORTIFY_SOURCE=2 -fno-common -fdiagnostics-show-option -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)

AC_DEFINE([_POSIX_C_SOURCE], [1], [Enable POSIX.1-1990 definitions])

AC_C_INLINE

AC_OUTPUT
