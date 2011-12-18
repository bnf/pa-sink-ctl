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
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>
#include <string.h>

#include "sink.h"
#include "interface.h"
#include "config.h"
#include "pa-sink-ctl.h"

/* Used for sink(_inputs) index comparison, since index is the first member */
static gint
compare_idx_pntr(gconstpointer i1, gconstpointer i2)
{
	return *((const guint32 *) i1) == *((const guint32 *) i2) ? 0 : -1;
}

static int
get_sink_priority(struct context *ctx, const pa_sink_info *sink_info)
{
	struct priority *p;

	list_foreach(ctx->config.priorities, p)
		if (g_strcmp0(pa_proplist_gets(sink_info->proplist, p->match),
			      p->value) == 0)
			return p->priority;

	return 0;
}

static gchar *
get_sink_name(struct context *ctx, const pa_sink_info *sink)
{
	struct config *cfg = &ctx->config;
	int i;

	for (i = 0; cfg->name_props && cfg->name_props[i]; ++i)
		if (pa_proplist_contains(sink->proplist, cfg->name_props[i]))
			return g_strdup(pa_proplist_gets(sink->proplist,
							 cfg->name_props[i]));

	return g_strdup(sink->name);
}

static gint
compare_sink_priority(gconstpointer new_data, gconstpointer el_data)
{
	const struct sink_info *new = new_data;
	const struct sink_info *el = el_data;

	/* Add 1 to append at end of sinks if priority equals */
	return el->priority - new->priority + 1;
}

static void
sink_childs_foreach(struct vol_ctl *ctl, GFunc func, gpointer user_data)
{
	struct sink_info *sink = (struct sink_info *) ctl;
	struct sink_input_info *input;

	list_foreach(sink->ctx->input_list, input)
		if (input->sink == sink->base.index)
			func(&input->base, user_data);
}

static void
sink_info_cb(pa_context *c, const pa_sink_info *i,
	     gint is_last, gpointer userdata)
{
	struct context *ctx = userdata;
	struct sink_info *sink;
	GList *el;

	if (is_last < 0) {
		if (pa_context_errno(c) == PA_ERR_NOENTITY)
			return;
		interface_set_status(&ctx->interface,
				     "Failed to get sink information: %s\n",
				     pa_strerror(pa_context_errno(c)));
		return;
	}

	if (is_last) {
		interface_redraw(&ctx->interface);
		return;
	}

	el = g_list_find_custom(ctx->sink_list, &i->index, compare_idx_pntr);
	if (el == NULL) {
		sink = g_new0(struct sink_info, 1);
		if (sink == NULL)
			return;
		sink->base.index = i->index;
		sink->base.mute_set = pa_context_set_sink_mute_by_index;
		sink->base.volume_set = pa_context_set_sink_volume_by_index;
		sink->base.childs_foreach = sink_childs_foreach;
		sink->ctx = ctx;

		sink->priority = get_sink_priority(ctx, i);
		ctx->sink_list = g_list_insert_sorted(ctx->sink_list, sink,
						      compare_sink_priority);
	} else {
		sink = el->data;
		g_free(sink->base.name);
	}

	sink->base.mute     = i->mute;
	sink->base.vol      = pa_cvolume_avg(&i->volume);
	sink->base.channels = i->volume.channels;
	sink->base.name     = get_sink_name(ctx, i);
}

static void
sink_input_info_cb(pa_context *c, const pa_sink_input_info *i,
		   gint is_last, gpointer userdata)
{
	struct context *ctx = userdata;
	struct sink_input_info *sink_input;
	GList *el;

	if (is_last < 0) {
		if (pa_context_errno(c) == PA_ERR_NOENTITY)
			return;
		interface_set_status(&ctx->interface,
				     "Failed to get sink input info: %s\n",
				     pa_strerror(pa_context_errno(c)));
		return;
	}

	if (is_last) {
		interface_redraw(&ctx->interface);
		return;
	}

	if (!(i->client != PA_INVALID_INDEX)) return;

	el = g_list_find_custom(ctx->input_list, &i->index, compare_idx_pntr);
	if (el == NULL) {
		sink_input = g_new0(struct sink_input_info, 1);
		if (sink_input == NULL)
			return;
		sink_input->base.index = i->index;
		sink_input->base.indent = 1;
		sink_input->base.hide_index = TRUE;
		sink_input->base.mute_set = pa_context_set_sink_input_mute;
		sink_input->base.volume_set = pa_context_set_sink_input_volume;
		ctx->input_list = g_list_append(ctx->input_list, sink_input);
	} else {
		sink_input = el->data;
		g_free(sink_input->base.name);
	}

	sink_input->sink = i->sink;
	sink_input->base.name =
		pa_proplist_contains(i->proplist, "application.name") ?
		g_strdup(pa_proplist_gets(i->proplist, "application.name")) :
		g_strdup(i->name);
	sink_input->base.mute = i->mute;
	sink_input->base.channels = i->volume.channels;
	sink_input->base.vol = pa_cvolume_avg(&i->volume);
}

static void
sink_free(gpointer data)
{
	struct sink_info *sink = data;

	g_free(sink->base.name);
	g_free(sink);
}

static void
sink_input_free(gpointer data)
{
	struct sink_input_info *input = data;

	g_free(input->base.name);
	g_free(input);
}

static void
subscribe_cb(pa_context *c, pa_subscription_event_type_t t,
	     guint32 idx, gpointer userdata)
{
	struct context *ctx = userdata;
	pa_operation *op;
	GList *el;

	switch (t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
	case PA_SUBSCRIPTION_EVENT_NEW:
	case PA_SUBSCRIPTION_EVENT_CHANGE:
		switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
		case PA_SUBSCRIPTION_EVENT_SINK:
			op = pa_context_get_sink_info_by_index(c, idx,
							       sink_info_cb,
							       ctx);
			pa_operation_unref(op);
			break;
		case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
			op = pa_context_get_sink_input_info(c, idx,
							    sink_input_info_cb,
							    ctx);
			pa_operation_unref(op);
			break;
		}
		break;
	case PA_SUBSCRIPTION_EVENT_REMOVE:
		switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
		case PA_SUBSCRIPTION_EVENT_SINK:
			el = g_list_find_custom(ctx->sink_list, &idx,
						compare_idx_pntr);
			if (el == NULL)
				break;
			sink_free(el->data);
			ctx->sink_list = g_list_delete_link(ctx->sink_list, el);
			break;
		case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
			el = g_list_find_custom(ctx->input_list, &idx,
						compare_idx_pntr);
			if (el == NULL)
				break;
			sink_input_free(el->data);
			ctx->input_list =
				g_list_delete_link(ctx->input_list, el);
			break;
		default:
			return;
		}
		interface_redraw(&ctx->interface);
		break;
	default:
		break;
	}
}

/*
 * is called after connection
 */
static void
context_state_callback(pa_context *c, gpointer userdata)
{
	struct context *ctx = userdata;
	pa_operation *op;

	ctx->context_ready = FALSE;
	switch (pa_context_get_state(c)) {
	case PA_CONTEXT_CONNECTING:
		interface_set_status(&ctx->interface, "connecting...");
		break;
	case PA_CONTEXT_AUTHORIZING:
		interface_set_status(&ctx->interface, "authorizing...");
		break;
	case PA_CONTEXT_SETTING_NAME:
		interface_set_status(&ctx->interface, "setting name...");
		break;

	case PA_CONTEXT_READY:
		op = pa_context_get_sink_info_list(c, sink_info_cb, ctx);
		pa_operation_unref(op);
		op = pa_context_get_sink_input_info_list(c, sink_input_info_cb,
							 ctx);
		pa_operation_unref(op);

		pa_context_set_subscribe_callback(c, subscribe_cb, ctx);
		{
			pa_subscription_mask_t mask =
				PA_SUBSCRIPTION_MASK_SINK |
				PA_SUBSCRIPTION_MASK_SINK_INPUT;
			g_assert((ctx->op = pa_context_subscribe(c, mask,
								 NULL, NULL)));
		}
		ctx->context_ready = TRUE;
		interface_set_status(&ctx->interface,
				     "ready to process events.");
		break;
	case PA_CONTEXT_FAILED:
		interface_set_status(&ctx->interface, "connection lost!");
		break;
	case PA_CONTEXT_TERMINATED:
		g_assert(ctx->op != NULL);
		pa_operation_cancel(ctx->op);
		pa_operation_unref(ctx->op);
		ctx->op = NULL;
		interface_set_status(&ctx->interface, "connection terminated.");
		break;
	default:
		interface_set_status(&ctx->interface, "unknown state");
		break;
	}
}

void
quit(struct context *ctx)
{
	pa_context_disconnect(ctx->context);
	g_main_loop_quit(ctx->loop);
}

int
main(int argc, char** argv)
{
	struct context ctx;
	pa_mainloop_api  *mainloop_api = NULL;
	pa_glib_mainloop *m            = NULL;

	memset(&ctx, 0, sizeof ctx);

	ctx.sink_list = NULL;
	ctx.input_list = NULL;
	ctx.context_ready = FALSE;
	ctx.return_value = 1;

	ctx.loop = g_main_loop_new(NULL, FALSE);
	if (!ctx.loop)
		goto cleanup_context;
	
	if (config_init(&ctx.config) < 0)
		goto cleanup_loop;

	if (interface_init(&ctx.interface) < 0)
		goto cleanup_config;

	if (!(m = pa_glib_mainloop_new(NULL))) {
		g_printerr("pa_glib_mainloop_new failed\n");
		goto cleanup_interface;
	}
	mainloop_api = pa_glib_mainloop_get_api(m);

	if (!(ctx.context = pa_context_new(mainloop_api, "pa-sink-ctl"))) {
		interface_set_status(&ctx.interface,
				     "pa_context_new failed: %s\n",
				     pa_strerror(pa_context_errno(ctx.context)));
	}

	// define callback for connection init
	pa_context_set_state_callback(ctx.context,
				      context_state_callback, &ctx);
	if (pa_context_connect(ctx.context, NULL,
			       PA_CONTEXT_NOAUTOSPAWN, NULL)) {
		interface_set_status(&ctx.interface,
				     "pa_context_connect failed: %s\n",
				     pa_strerror(pa_context_errno(ctx.context)));
	}

	ctx.return_value = 0;
	g_main_loop_run(ctx.loop);

	g_list_free_full(ctx.sink_list, sink_free);
	g_list_free_full(ctx.input_list, sink_input_free);

	pa_context_unref(ctx.context);
	pa_glib_mainloop_free(m);
cleanup_interface:
	interface_clear(&ctx.interface);
cleanup_config:
	config_uninit(&ctx.config);
cleanup_loop:
	g_main_loop_unref(ctx.loop);
cleanup_context:

	return ctx.return_value;
}
