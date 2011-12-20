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

#include <glib.h>

#include "pa-sink-ctl.h"
#include "interface.h"
#include "ctl.h"
#include "command.h"

static struct vol_ctl *
ctl_last_child_or_ctl(struct vol_ctl *ctl)
{
	int len;

	if (ctl->childs_len && (len = ctl->childs_len(ctl)) > 0)
		return ctl->get_nth_child(ctl, len-1);

	return ctl;
}

static void
up(struct context *ctx, int key)
{
	struct interface *ifc = &ctx->interface;
	struct vol_ctl *ctl, *prev;
	GList *last;

	if (!ctx->context_ready)
		return;

	ctl = ifc->current_ctl;
	if (ctl == NULL)
		return;

	prev = ctl->prev_ctl(ctl);
	if (prev) {
		ifc->current_ctl = ctl_last_child_or_ctl(prev);
	} else if (ctl->get_parent) {
		ifc->current_ctl = ctl->get_parent(ctl);
	} else {
		struct main_ctl *mctl = (struct main_ctl *) ctl;

		if (*mctl->list == ctx->source_list &&
		    (last = g_list_last(ctx->sink_list)) != NULL)
			ifc->current_ctl = ctl_last_child_or_ctl(last->data);
	}

	interface_redraw(ifc);
}

static void
down(struct context *ctx, int key)
{
	struct interface *ifc = &ctx->interface;
	struct vol_ctl *ctl, *next = NULL, *tmp, *parent;

	if (!ctx->context_ready)
		return;

	ctl = ifc->current_ctl;
	if (ctl == NULL)
		return;

	if (ctl->childs_len && ctl->childs_len(ctl) > 0) {
		next = ctl->get_nth_child(ctl, 0);
	} else if ((tmp = ctl->next_ctl(ctl)) != NULL) {
		next = tmp;
	} else if (ctl->get_parent) {
		parent = ctl->get_parent(ctl);
		next = parent->next_ctl(parent);
		if (!next)
			ctl = parent; /* parent for end-of-sink list lookup */
	}

	if (!next) {
		struct main_ctl *mctl = (struct main_ctl *) ctl;

		if (*mctl->list == ctx->sink_list &&
		    g_list_first(ctx->source_list) != NULL)
			next = g_list_first(ctx->source_list)->data;
	}

	if (next) {
		ifc->current_ctl = next;
		interface_redraw(ifc);
	}
}

static void
volume_change(struct context *ctx, gboolean volume_increment)
{
	struct vol_ctl *ctl;
	pa_cvolume volume;
	const pa_volume_t inc = 2 * PA_VOLUME_NORM / 100;
	pa_operation *o;

	if (!ctx->context_ready)
		return;

	ctl = ctx->interface.current_ctl;
	if (!ctl || !ctl->volume_set)
		return;

	volume.channels = ctl->channels;
	pa_cvolume_set(&volume, volume.channels, ctl->vol);

	if (volume_increment)
		if (PA_VOLUME_NORM > ctl->vol &&
		    PA_VOLUME_NORM - ctl->vol > inc)
			pa_cvolume_inc(&volume, inc);
		else
			pa_cvolume_set(&volume, volume.channels,
				       PA_VOLUME_NORM);
	else
		pa_cvolume_dec(&volume, inc);


	o = ctl->volume_set(ctx->context, ctl->index, &volume, NULL,NULL);
	pa_operation_unref(o);
}

static void
volume_down(struct context *ctx, int key)
{
	volume_change(ctx, FALSE);
}

static void
volume_up(struct context *ctx, int key)
{
	volume_change(ctx, TRUE);
}

static void
toggle_mute(struct context *ctx, int key)
{
	struct vol_ctl *ctl;
	pa_operation *o;

	if (!ctx->context_ready)
		return;

	ctl = ctx->interface.current_ctl;
	if (!ctl && !ctl->mute_set)
		return;

	o = ctl->mute_set(ctx->context, ctl->index, !ctl->mute, NULL, NULL);
	pa_operation_unref(o);
}

static void
switch_sink(struct context *ctx, int key)
{
	struct vol_ctl *cslave;
	struct main_ctl *mcparent, *ctl;
	pa_operation *o;
	GList *el;

	if (!ctx->context_ready)
		return;

	cslave = ctx->interface.current_ctl;
	if (!cslave || !cslave->get_parent)
		return;

	mcparent = (struct main_ctl *) cslave->get_parent(cslave);
	if (g_list_length(*mcparent->list) <= 1)
		return;

	el = g_list_find(*mcparent->list, mcparent);
	g_assert(el != NULL);
	if (el->next)
		el = el->next;
	else
		el = g_list_first(*mcparent->list);

	ctl = el->data;
	o = ctl->move_child(ctx->context,
			    cslave->index, ctl->base.index, NULL, NULL);
	pa_operation_unref(o);
}

static void
quit_cmd(struct context *ctx, int key)
{
	quit(ctx);
}

struct command_cb_descriptor command_cbs[] = {
	{ "up",          up },
	{ "down",        down },
	{ "volume-down", volume_down },
	{ "volume-up",   volume_up },
	{ "mute",        toggle_mute },
	{ "switch",      switch_sink },
	{ "quit",        quit_cmd },
	{ NULL,          NULL }
};
