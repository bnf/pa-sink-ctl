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
sink_input_len(struct context *ctx, struct sink_info *sink)
{
	struct sink_input_info *input;
	int len = 0;

	list_foreach(ctx->input_list, input)
		if (input->sink == sink->base.index)
			len++;

	return len;
}

static void
up(struct context *ctx, int key)
{
	struct sink_info *sink = NULL;

	if (!ctx->context_ready)
		return;

	if (ctx->chooser_input == SELECTED_SINK &&
	    ctx->chooser_sink > 0) {
		sink = g_list_nth_data(ctx->sink_list, --ctx->chooser_sink);
		/* autoassigment to SELECTED_SINK (=-1) if length = 0 */
		ctx->chooser_input = sink_input_len(ctx, sink) - 1;
	} else if (ctx->chooser_input >= 0)
		--ctx->chooser_input;

	interface_redraw(ctx);
}

static void
down(struct context *ctx, int key)
{
	struct sink_info *sink;

	if (!ctx->context_ready)
		return;

	sink = g_list_nth_data(ctx->sink_list, ctx->chooser_sink);
	if (ctx->chooser_input == (sink_input_len(ctx, sink) - 1) &&
	    ctx->chooser_sink < (gint) g_list_length(ctx->sink_list)-1) {
		++ctx->chooser_sink;
		ctx->chooser_input = SELECTED_SINK;
	}
	else if (ctx->chooser_input < (sink_input_len(ctx, sink) - 1))
		++ctx->chooser_input;
	interface_redraw(ctx);
}

static void
volume_change(struct context *ctx, gboolean volume_increment)
{
	struct vol_ctl *ctl;
	pa_operation *o;
	pa_cvolume volume;
	pa_volume_t inc;

	if (!ctx->context_ready)
		return;

	ctl = interface_get_current_ctl(ctx, NULL);
	if (!ctl)
		return;

	volume = (pa_cvolume) { .channels = ctl->channels };
	pa_cvolume_set(&volume, volume.channels, ctl->vol);
	inc = 2 * PA_VOLUME_NORM / 100;

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
do_mute(struct context *ctx, int key)
{
	struct vol_ctl *ctl;
	pa_operation *o;

	if (!ctx->context_ready)
		return;

	ctl = interface_get_current_ctl(ctx, NULL);
	if (!ctl)
		return;

	o = ctl->mute_set(ctx->context, ctl->index, !ctl->mute, NULL, NULL);
	pa_operation_unref(o);
}

static void
switch_sink(struct context *ctx, int key)
{
	struct sink_input_info *t;
	struct vol_ctl *input, *sink;
	pa_operation *o;
	gint i;

	if (!ctx->context_ready)
		return;


	input = interface_get_current_ctl(ctx, &sink);
	if (!input || !sink)
		return;

	if (g_list_length(ctx->sink_list) <= 1)
		return;

	if (ctx->chooser_sink < (gint) g_list_length(ctx->sink_list) - 1)
		ctx->chooser_sink++;
	else
		ctx->chooser_sink = 0;

	sink = g_list_nth_data(ctx->sink_list, ctx->chooser_sink);
	/* chooser_input needs to be derived from $selected_index */
	o = pa_context_move_sink_input_by_index(ctx->context,
						input->index, sink->index,
						NULL, NULL);
	pa_operation_unref(o);

	/* get new chooser_input, if non, select sink as fallback */
	ctx->chooser_input = SELECTED_SINK; 
	i = -1;
	list_foreach(ctx->input_list, t) {
		if (t->base.index == input->index) {
			ctx->chooser_input = ++i;
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
	{ "mute",        do_mute },
	{ "switch",      switch_sink },
	{ "quit",        quit_cmd },
	{ NULL,          NULL }
};
