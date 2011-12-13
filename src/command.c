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
		if (input->sink == sink->index)
			len++;

	return len;
}

static struct sink_input_info *
sink_get_nth_input(struct context *ctx, struct sink_info *sink, int n)
{
	struct sink_input_info *input;
	int i = 0;

	list_foreach(ctx->input_list, input) {
		if (input->sink != sink->index)
			continue;
		if (i++ == n)
			return input;
	}

	return NULL;
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
	struct sink_info *sink;
	struct sink_input_info *input;
	guint32 index;

	if (!ctx->context_ready)
		return;

	sink = g_list_nth_data(ctx->sink_list, ctx->chooser_sink);
	pa_cvolume volume;
	pa_volume_t tmp_vol;
	pa_operation* (*volume_set) (pa_context*, guint32, const pa_cvolume *,
				     pa_context_success_cb_t, gpointer);

	if (ctx->chooser_input >= 0) {
		input      = sink_get_nth_input(ctx, sink, ctx->chooser_input);
		index      = input->index;
		volume     = (pa_cvolume) { .channels = input->channels };
		tmp_vol    = input->vol; 
		volume_set = pa_context_set_sink_input_volume;
	} else if (ctx->chooser_input == SELECTED_SINK) {
		index      = sink->index;
		volume     = (pa_cvolume) { .channels = sink->channels };
		tmp_vol    = sink->vol;
		volume_set = pa_context_set_sink_volume_by_index;
	} else {
		g_assert(0);
		return;
	}

	pa_cvolume_set(&volume, volume.channels, tmp_vol);
	pa_volume_t inc = 2 * PA_VOLUME_NORM / 100;

	if (volume_increment)
		if (PA_VOLUME_NORM > tmp_vol &&
		    PA_VOLUME_NORM - tmp_vol > inc)
			pa_cvolume_inc(&volume, inc);
		else
			pa_cvolume_set(&volume, volume.channels,
				       PA_VOLUME_NORM);
	else
		pa_cvolume_dec(&volume, inc);


	pa_operation_unref(volume_set(ctx->context, index, &volume,
				      change_callback, ctx));
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
mute(struct context *ctx, int key)
{
	struct sink_info *sink;
	struct sink_input_info *input;
	guint32 index;
	gint mute;

	if (!ctx->context_ready)
		return;

	sink = g_list_nth_data(ctx->sink_list, ctx->chooser_sink);
	pa_operation* (*mute_set) (pa_context*, guint32, int,
				   pa_context_success_cb_t, void*);

	if (ctx->chooser_input >= 0) {
		input    = sink_get_nth_input(ctx, sink, ctx->chooser_input);
		index    = input->index;
		mute     = !input->mute;
		mute_set = pa_context_set_sink_input_mute;
	} else if (ctx->chooser_input == SELECTED_SINK) {
		index    = sink->index;
		mute     = !sink->mute;
		mute_set = pa_context_set_sink_mute_by_index;
	} else {
		g_assert(0);
		return;
	}

	pa_operation_unref(mute_set(ctx->context, index, mute,
				    change_callback, ctx));
}

static void
switch_sink(struct context *ctx, int key)
{
	struct sink_info *sink = NULL;
	struct sink_input_info *input, *t;
	pa_operation *o;
	gint i;

	if (!ctx->context_ready)
		return;

	if (ctx->chooser_input == SELECTED_SINK)
		return;
	sink = g_list_nth_data(ctx->sink_list, ctx->chooser_sink);
	input = sink_get_nth_input(ctx, sink, ctx->chooser_input);
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
						change_callback, NULL);
	pa_operation_unref(o);

	/* get new chooser_input, if non, select sink as fallback */
	ctx->chooser_input = SELECTED_SINK; 
	i = -1;
	list_foreach(ctx->input_list, t) {
		if (t->index == input->index) {
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
	{ "mute",        mute },
	{ "switch",      switch_sink },
	{ "quit",        quit_cmd },
	{ NULL,          NULL }
};
