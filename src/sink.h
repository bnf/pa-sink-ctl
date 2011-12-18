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

#ifndef SINK_H
#define SINK_H

#include <glib.h>
#include <pulse/pulseaudio.h>

struct context;

struct vol_ctl {
	guint32 index;
	pa_volume_t vol; // TOTO: exchange with the channel-list
	gint mute;
	guint8 channels;

	gchar *name; /* displayed name */
	gint indent; /* indentation when displayed */
	gboolean hide_index;

	pa_operation *(*mute_set)(pa_context *, guint32, int,
				  pa_context_success_cb_t, void *);

	pa_operation *(*volume_set)(pa_context *, guint32, const pa_cvolume *,
				    pa_context_success_cb_t, gpointer);

	void (*childs_foreach)(struct vol_ctl *ctx, GFunc func, gpointer udata);
};

struct sink_info {
	struct vol_ctl base;
	gint priority;
	struct context *ctx;
};

struct sink_input_info {
	struct vol_ctl base;
	guint32 sink;
};

#endif
