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

static void
up(struct context *ctx, int key)
{
	struct interface *ifc = &ctx->interface;
	struct main_ctl *ctl = NULL;

	if (!ctx->context_ready)
		return;

	if (ifc->chooser_child == SELECTED_MAIN_CTL &&
	    ifc->chooser_main_ctl > 0) {

		--ifc->chooser_main_ctl;
		/* Always a main_ctl since chooser_child = SELECTED_MAIN_CTL */
		ctl = (struct main_ctl *) interface_get_current_ctl(ifc, NULL);

		/* autoassigment to SELECTED_MAIN_CTL (=-1) if length = 0 */
		ifc->chooser_child = ctl->base.childs_len(&ctl->base) - 1;
	} else if (ifc->chooser_child >= 0)
		--ifc->chooser_child;

	interface_redraw(ifc);
}

static void
down(struct context *ctx, int key)
{
	struct interface *ifc = &ctx->interface;
	int max_ctl_childs;
	struct vol_ctl *ctl, *parent;

	if (!ctx->context_ready)
		return;

	ctl = interface_get_current_ctl(&ctx->interface, &parent);
	if (parent)
		ctl = parent;

	max_ctl_childs = ctl->childs_len(ctl) -1;
	if (ifc->chooser_child == max_ctl_childs) {
		if (ifc->chooser_main_ctl <
		    interface_get_main_ctl_length(ifc) -1) {
			++ifc->chooser_main_ctl;
			ifc->chooser_child = SELECTED_MAIN_CTL;
		}
	} else if (ifc->chooser_child < max_ctl_childs)
		++ifc->chooser_child;

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
	struct slave_ctl *t;
	struct vol_ctl *cslave, *cparent;
	struct main_ctl *mcparent;
	pa_operation *o;
	gint i;
	GList **list;
	int offset;

	if (!ctx->context_ready)
		return;

	cslave = interface_get_current_ctl(&ctx->interface, &cparent);
	if (!cslave || !cparent)
		return;

	mcparent = (struct main_ctl *) cparent;
	if (*mcparent->childs_list == ctx->input_list) {
		list = &ctx->sink_list;
		offset = 0;
	} else {
		list = &ctx->source_list;
		offset = g_list_length(ctx->sink_list);
	}

	if (g_list_length(*list) <= 1)
		return;

	if (ifc->chooser_main_ctl < (gint) (offset + g_list_length(*list) - 1))
		ifc->chooser_main_ctl++;
	else
		ifc->chooser_main_ctl = offset;

	mcparent = g_list_nth_data(*list, ifc->chooser_main_ctl - offset);
	/* chooser_child needs to be derived from $selected_index */
	o = mcparent->move_child(ctx->context,
				 cslave->index, mcparent->base.index,
				 NULL, NULL);
	pa_operation_unref(o);

	/* get new chooser_child, if non, select sink as fallback */
	ifc->chooser_child = SELECTED_MAIN_CTL; 
	i = -1;
	list_foreach(*mcparent->childs_list, t) {
		if (t->base.index == cslave->index) {
			ifc->chooser_child = ++i;
			break;
		}
		if (t->parent_index == mcparent->base.index)
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
