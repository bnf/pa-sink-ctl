/*
 * pa-sink-ctl - NCurses based Pulseaudio control client
 * Copyright (C) 2011  Benjamin Franzke <benjaminfranzke@googlemail.com>
 * Copyright (C) 2010  Jan Klemkow <web2p10@wemelug.de>
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

#ifndef INTERFACE_H
#define INTERFACE_H

#define _XOPEN_SOURCE 700

#include <glib.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>

#define SELECTED_MAIN_CTL -1
#define H_MSG_BOX 3

struct interface {
	WINDOW *menu_win;
	WINDOW *msg_win;
	gint volume_bar_len;
	gchar *volume_bar;

	guint resize_source_id;
#ifdef HAVE_SIGNALFD
	int signal_fd;
#endif
	guint input_source_id;
	guint max_name_len;

	gchar *status;

	struct vol_ctl *current_ctl;
};

int
interface_get_main_ctl_length(struct interface *ifc);

void
interface_redraw(struct interface *ifc);

int
interface_init(struct interface *ifc);

void
interface_clear(struct interface *ifc);

void 
interface_set_status(struct interface *ifc, const gchar *, ...);

#endif
