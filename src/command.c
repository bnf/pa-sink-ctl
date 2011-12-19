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
#include "sink.h"
#include "command.h"

static int
main_ctl_childs_len(struct context *ctx, struct main_ctl *ctl)
{
	struct slave_ctl *sctl;
	int len = 0;

	list_foreach(*ctl->childs_list, sctl)
		if (sctl->parent_index == ctl->base.index)
			len++;

	return len;
}

static void
up(struct context *ctx, int key)
{
	struct interface *ifc = &ctx->interface;
	struct sink *sink = NULL;

	if (!ctx->context_ready)
		return;

	if (ifc->chooser_input == SELECTED_SINK &&
	    ifc->chooser_sink > 0) {
		sink = g_list_nth_data(ctx->sink_list, --ifc->chooser_sink);
		/* autoassigment to SELECTED_SINK (=-1) if length = 0 */
		ifc->chooser_input = main_ctl_childs_len(ctx, (struct main_ctl *) sink) - 1;
	} else if (ifc->chooser_input >= 0)
		--ifc->chooser_input;

	interface_redraw(ifc);
}

static void
down(struct context *ctx, int key)
{
	struct interface *ifc = &ctx->interface;
	int max_input;
	struct vol_ctl *ctl, *parent;
	int max_len;

	if (!ctx->context_ready)
		return;

	max_len = g_list_length(ctx->sink_list) + g_list_length(ctx->source_list);

	ctl = interface_get_current_ctl(&ctx->interface, &parent);
	if (parent)
		ctl = parent;

	max_input = main_ctl_childs_len(ctx, (struct main_ctl *) ctl) -1;

	if (ifc->chooser_input == max_input) {
		if (ifc->chooser_sink < max_len -1) {
			++ifc->chooser_sink;
			ifc->chooser_input = SELECTED_SINK;
		}
	} else if (ifc->chooser_input < max_input)
		++ifc->chooser_input;

	interface_redraw(ifc);
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

	ctl = interface_get_current_ctl(&ctx->interface, NULL);
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

	ctl = interface_get_current_ctl(&ctx->interface, NULL);
	if (!ctl && !ctl->mute_set)
		return;

	o = ctl->mute_set(ctx->context, ctl->index, !ctl->mute, NULL, NULL);
	pa_operation_unref(o);
}

static void
switch_sink(struct context *ctx, int key)
{
	struct interface *ifc = &ctx->interface;
	struct sink_input *t;
	struct vol_ctl *input, *sink;
	pa_operation *o;
	gint i;

	if (!ctx->context_ready)
		return;


	input = interface_get_current_ctl(&ctx->interface, &sink);
	if (!input || !sink)
		return;

	if (g_list_length(ctx->sink_list) <= 1)
		return;

	if (ifc->chooser_sink < (gint) g_list_length(ctx->sink_list) - 1)
		ifc->chooser_sink++;
	else
		ifc->chooser_sink = 0;

	sink = g_list_nth_data(ctx->sink_list, ifc->chooser_sink);
	/* chooser_input needs to be derived from $selected_index */
	o = pa_context_move_sink_input_by_index(ctx->context,
						input->index, sink->index,
						NULL, NULL);
	pa_operation_unref(o);

	/* get new chooser_input, if non, select sink as fallback */
	ifc->chooser_input = SELECTED_SINK; 
	i = -1;
	list_foreach(ctx->input_list, t) {
		if (t->base.index == input->index) {
			ifc->chooser_input = ++i;
			break;
		}
		if (t->sink == sink->index)
			++i;
	}
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
