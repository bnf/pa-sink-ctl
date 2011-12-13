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

#include "sink.h"
#include "interface.h"
#include "config.h"
#include "pa-sink-ctl.h"

static struct sink_input_info *
find_sink_input_by_idx(struct context *ctx, gint idx)
{
	struct sink_input_info *input;

	list_foreach(ctx->input_list, input)
		if (input->index == idx)
			return input;

	return NULL;
}

static struct sink_info *
find_sink_by_idx(struct context *ctx, gint idx)
{
	struct sink_info *sink;

	list_foreach(ctx->sink_list, sink)
		if (sink->index == idx)
			return sink;

	return NULL;
}

static void
sink_input_info_cb(pa_context *c, const pa_sink_input_info *i,
		   gint is_last, gpointer userdata)
{
	g_assert(userdata != NULL);
	struct context *ctx = userdata;

	if (is_last < 0) {
		if (pa_context_errno(c) == PA_ERR_NOENTITY)
			return;
		g_printerr("Failed to get sink input information: %s\n",
			   pa_strerror(pa_context_errno(c)));
		return;
	}

	if (is_last) {
		interface_redraw(ctx);
		return;
	}

	if (!(i->client != PA_INVALID_INDEX)) return;

	struct sink_input_info sink_input = {
		.index = i->index,
		.sink = i->sink,
		.name = pa_proplist_contains(i->proplist, "application.name") ?
			g_strdup(pa_proplist_gets(i->proplist,
						  "application.name")):
			g_strdup(i->name),
		.mute = i->mute,
		.channels = i->volume.channels,
		.vol = pa_cvolume_avg(&i->volume),
		.pid = NULL /* maybe obsolete */
	};

	struct sink_input_info *inlist = find_sink_input_by_idx(ctx, i->index);
	if (inlist)
		*inlist = sink_input;
	else
		list_append_struct(ctx->input_list, sink_input);
}

static int
get_sink_priority(struct context *ctx, const pa_sink_info *sink_info)
{
	struct priority *p;
	const char *value;

	list_foreach(ctx->config.priorities, p) {
		value = pa_proplist_gets(sink_info->proplist, p->match);

		if (g_strcmp0(value, p->value) == 0)
			return p->priority;
	}

	return 0;
}

static gint
compare_sink_priority(gconstpointer new_data, gconstpointer el_data)
{
	const struct sink_info *new = new_data;
	const struct sink_info *el = el_data;

	/* Add 1 to append at end of sinks if priority equals */
	return el->priority - new->priority + 1;
}

static gchar *
get_sink_name(struct context *ctx, const pa_sink_info *sink)
{
	struct config *cfg = &ctx->config;
	int i;

	for (i = 0; cfg->name_props && cfg->name_props[i]; ++i) {
		if (pa_proplist_contains(sink->proplist,
					 cfg->name_props[i]))
			return g_strdup(pa_proplist_gets(sink->proplist,
							 cfg->name_props[i]));
	}
	
	return g_strdup(sink->name);
}

static void
sink_info_cb(pa_context *c, const pa_sink_info *i,
	     gint is_last, gpointer userdata)
{
	g_assert(userdata != NULL);
	struct context *ctx = userdata;

	if (is_last < 0) {
		if (pa_context_errno(c) == PA_ERR_NOENTITY)
			return;
		g_printerr("Failed to get sink information: %s\n",
			   pa_strerror(pa_context_errno(c)));
		quit(ctx);
	}

	if (is_last) {
		interface_redraw(ctx);
		return;
	}

	struct sink_info sink = {
		.index = i->index,
		.mute  = i->mute,
		.vol   = pa_cvolume_avg(&i->volume),
		.channels = i->volume.channels,
		.name = get_sink_name(ctx, i),
		.priority = get_sink_priority(ctx, i),
	};

	struct sink_info *inlist = find_sink_by_idx(ctx, i->index);
	if (inlist)
		*inlist = sink;
	else
		ctx->sink_list =
			g_list_insert_sorted(ctx->sink_list,
					     g_memdup(&sink, sizeof sink),
					     compare_sink_priority);
}

static void
sink_free(gpointer data)
{
	struct sink_info *sink = data;

	g_free(sink->name);
	g_free(sink);
}

static void
sink_input_free(gpointer data)
{
	struct sink_input_info *input = data;

	g_free(input->name);
	g_free(input);
}

static void
subscribe_cb(pa_context *c, pa_subscription_event_type_t t,
	     guint32 idx, gpointer userdata)
{
	struct context *ctx = userdata;
	pa_operation *op;
	gpointer object;

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
			object = find_sink_by_idx(ctx, idx);
			if (object == NULL)
				break;
			ctx->sink_list = g_list_remove(ctx->sink_list, object);
			sink_free(object);
			break;
		case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
			object = find_sink_input_by_idx(ctx, idx);
			if (object == NULL)
				break;
			ctx->input_list = g_list_remove(ctx->input_list,
							object);
			sink_input_free(object);
			break;
		default:
			return;
		}
		interface_redraw(ctx);
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
		interface_set_status(ctx, "connecting...");
		break;
	case PA_CONTEXT_AUTHORIZING:
		interface_set_status(ctx, "authorizing...");
		break;
	case PA_CONTEXT_SETTING_NAME:
		interface_set_status(ctx, "setting name...");
		break;

	case PA_CONTEXT_READY:
		op = pa_context_get_sink_info_list(c, sink_info_cb, ctx);
		pa_operation_unref(op);
		op = pa_context_get_sink_input_info_list(c, sink_input_info_cb,
							 ctx);
		pa_operation_unref(op);

		pa_context_set_subscribe_callback(c, subscribe_cb, ctx);
		pa_subscription_mask_t mask =
			PA_SUBSCRIPTION_MASK_SINK |
			PA_SUBSCRIPTION_MASK_SINK_INPUT;
		g_assert((ctx->op = pa_context_subscribe(c, mask, NULL, NULL)));
		ctx->context_ready = TRUE;
		interface_set_status(ctx, "ready to process events.");
		break;
	case PA_CONTEXT_FAILED:
		interface_set_status(ctx, "connection lost!");
		break;
	case PA_CONTEXT_TERMINATED:
		g_assert(ctx->op != NULL);
		pa_operation_cancel(ctx->op);
		pa_operation_unref(ctx->op);
		ctx->op = NULL;
		interface_set_status(ctx, "connection terminated.");
		g_main_loop_quit(ctx->loop);
		break;
	default:
		interface_set_status(ctx, "unknown state");
		break;
	}
}

void
quit(struct context *ctx)
{
	pa_context_disconnect(ctx->context);
}

/*
 * is called, after user input
 */
void
change_callback(pa_context* c, gint success, gpointer userdata)
{
#if 0
	struct context *ctx = userdata;
#endif
	return;
}

int
main(int argc, char** argv)
{
	struct context *ctx = g_new0(struct context, 1);
	pa_mainloop_api  *mainloop_api = NULL;
	pa_glib_mainloop *m            = NULL;

	ctx->sink_list = NULL;
	ctx->input_list = NULL;
	ctx->max_name_len = 0;
	ctx->context_ready = FALSE;

	ctx->loop = g_main_loop_new(NULL, FALSE);
	
	if (config_init(&ctx->config) < 0)
		return -1;

	interface_init(ctx);

	if (!(m = pa_glib_mainloop_new(NULL))) {
		interface_clear(ctx);
		g_printerr("error: pa_glib_mainloop_new() failed.\n");
		return -1;
	}

	mainloop_api = pa_glib_mainloop_get_api(m);

	if (!(ctx->context = pa_context_new(mainloop_api, "pa-sink-ctl"))) {
		interface_clear(ctx);
		g_printerr("error: pa_context_new() failed.\n");
		return -1;
	}

	// define callback for connection init
	pa_context_set_state_callback(ctx->context,
				      context_state_callback, ctx);
	if (pa_context_connect(ctx->context, NULL,
			       PA_CONTEXT_NOAUTOSPAWN, NULL)) {
		interface_clear(ctx);
		g_printerr("error: pa_context_connect() failed.\n");
		return -1;
	}

	g_main_loop_run(ctx->loop);

	interface_clear(ctx);
	g_list_free_full(ctx->sink_list, sink_free);
	g_list_free_full(ctx->input_list, sink_input_free);

	pa_glib_mainloop_free(m);
	pa_context_unref(ctx->context);
	g_main_loop_unref(ctx->loop);

	config_uninit(&ctx->config);

	g_free(ctx);

	return 0;
}
