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

#include <glib.h>
#include <pulse/pulseaudio.h>

#define SELECTED_SINK -1
#define H_MSG_BOX 3

struct context;

struct vol_ctl *
interface_get_current_ctl(struct context *ctx, struct vol_ctl **parent);

void
interface_redraw(struct context *ctx);

int
interface_init(struct context *ctx);

void
interface_clear(struct context *ctx);

void 
interface_set_status(struct context *ctx, const gchar *, ...);

#endif
