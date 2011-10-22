/*
 * pa-sink-ctl - NCurses based Pulseaudio control client
 * Copyright (C) 2011  Benjamin Franzke <benjaminfranzke@googlemail.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UNIX_SIGNAL_H
#define UNIX_SIGNAL_H

#include <glib.h>

GSource *
unix_signal_source_new(gint signum);
guint
unix_signal_add(gint signum, GSourceFunc function, gpointer data);
guint
unix_signal_add_full(gint priority, gint signum, GSourceFunc function,
		     gpointer data, GDestroyNotify notify);

#endif /* UNIX_SIGNAL_H */
