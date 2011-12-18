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
#include <stddef.h>

#include "config.h"
#include "interface.h"

struct context {
	pa_context *context;
	pa_operation *op;
	gboolean context_ready;

	GMainLoop *loop;

	GList *sink_list;
	GList *source_list;
	GList *input_list;

	struct interface interface;
	struct config config;
	int return_value;
};

void
quit(struct context *ctx);

#define list_append_struct(list, data) \
	do { \
		(list) = g_list_append((list), \
				       g_memdup(&(data), sizeof(data))); \
	} while (0)

#define list_foreach(list, el) \
	for (GList *__l = (list); __l && ((el) = __l->data); __l = __l->next)

#define container_of(ptr, type, member) \
	(type *)(((char *) ((const __typeof__( ((type *)0)->member ) *)(ptr))) \
		 - offsetof(type,member) )

#endif
