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

struct sink_info {
	guint32 index;
	gchar* name;
	gint mute;
	guint8 channels;
	pa_volume_t vol;
	gint priority;
};

struct sink_input_info {
	guint32 index;
	guint32 sink;
	gchar *name;
	gint mute;
	guint8 channels;
	pa_volume_t vol; // TOTO: exchange with the channel-list
};

#endif
