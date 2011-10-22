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

#ifndef PA_SINK_CTL_H
#define PA_SINK_CTL_H

#include <glib.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>

struct context {
	pa_context *context;
	pa_operation *op;
	gboolean context_ready;

	WINDOW *menu_win;
	WINDOW *msg_win;

	guint resize_source_id;
#ifdef HAVE_SIGNALFD
	int signal_fd;
#endif
	guint input_source_id;

	gint chooser_sink;
	gint chooser_input;
	guint32 selected_index;

	guint max_name_len;

	GMainLoop *loop;

	GList *sink_list;
	GList *input_list;

	gchar *status;
	gboolean block_for_selected_index;
};

void
quit(struct context *ctx);

void
change_callback(pa_context* c, gint success, gpointer);

#endif
