#include <glib.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

#include "sink.h"
#include "interface.h"
#include "pa-sink-ctl.h"

#define list_append_struct(list, data) \
	do { \
		(list) = g_list_append((list), \
				       g_memdup(&(data), sizeof(data))); \
	} while (0)

int
main(int argc, char** argv)
{
	struct context *ctx = g_new0(struct context, 1);
	pa_mainloop_api  *mainloop_api = NULL;
	pa_glib_mainloop *m            = NULL;

	ctx->info_callbacks_finished = TRUE;
	ctx->info_callbacks_blocked = FALSE;
	ctx->sink_list = NULL;
	ctx->max_name_len = 0;
	ctx->context_ready = FALSE;

	ctx->loop = g_main_loop_new(NULL, FALSE);

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
	pa_context_set_state_callback(ctx->context, context_state_callback, ctx);
	if (pa_context_connect(ctx->context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL)) {
		interface_clear(ctx);
		g_printerr("error: pa_context_connect() failed.\n");
	}

	g_main_loop_run(ctx->loop);

	interface_clear(ctx);
	g_list_free(ctx->sink_list);

	pa_glib_mainloop_free(m);
	g_main_loop_unref(ctx->loop);

	return 0;
}

static void
subscribe_cb(pa_context *c, pa_subscription_event_type_t t, guint32 idx, gpointer userdata)
{
	struct context *ctx = userdata;

	if (!ctx->info_callbacks_finished)
		ctx->info_callbacks_blocked = TRUE;
	else
		collect_all_info(ctx);
}

/*
 * is called after connection
 */
void
context_state_callback(pa_context *c, gpointer userdata)
{
	struct context *ctx = userdata;

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
			collect_all_info(ctx);
			pa_context_set_subscribe_callback(c, subscribe_cb, ctx);
			g_assert((ctx->op = pa_context_subscribe(c, (pa_subscription_mask_t) (
					PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SINK_INPUT
					), NULL, NULL)));
			ctx->context_ready = TRUE;
			interface_set_status(ctx, "ready to process events.");
			break;
		case PA_CONTEXT_FAILED:
			interface_set_status(ctx, "cannot connect!");
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


/*
 * the begin of the callback loops
 */
void
get_sink_info_callback(pa_context *c, const pa_sink_info *i, gint is_last, gpointer userdata)
{
	g_assert(userdata != NULL);
	struct context *ctx = userdata;

	if (is_last < 0) {
		g_printerr("Failed to get sink information: %s\n", pa_strerror(pa_context_errno(c)));
		quit(ctx);
	}

	if (is_last) {
		pa_operation_unref(pa_context_get_sink_input_info_list(c, get_sink_input_info_callback, ctx));
		return;
	}

	sink_info sink = {
		.index = i->index,
		.mute  = i->mute,
		.vol   = pa_cvolume_avg(&i->volume),
		.channels = i->volume.channels,
		.name = g_strdup(i->name),
		.device = pa_proplist_contains(i->proplist, "device.product.name") ? 
			g_strdup(pa_proplist_gets(i->proplist, "device.product.name")) : NULL,
		.input_list = NULL
	};

	list_append_struct(ctx->tmp_sinks, sink);
}

/*
 * is called after sink-input
 */
void
get_sink_input_info_callback(pa_context *c, const pa_sink_input_info *i, gint is_last, gpointer userdata)
{
	g_assert(userdata != NULL);
	struct context *ctx = userdata;

	if (is_last < 0) {
		g_printerr("Failed to get sink input information: %s\n", pa_strerror(pa_context_errno(c)));
		return;
	}

	if (is_last) {
		ctx->info_callbacks_finished = TRUE;
		g_list_free_full(ctx->sink_list, g_free);
		ctx->sink_list = ctx->tmp_sinks;

		print_sink_list(ctx);

		if (ctx->info_callbacks_blocked) {
			ctx->info_callbacks_blocked = FALSE;
			collect_all_info(ctx);
		}
		return;
	}

	if (!(i->client != PA_INVALID_INDEX)) return;

	sink_input_info sink_input = {
		.index = i->index,
		.sink = i->sink,
		.name = pa_proplist_contains(i->proplist, "application.name") ?
			g_strdup(pa_proplist_gets(i->proplist, "application.name")):
			g_strdup(i->name),
		.mute = i->mute,
		.channels = i->volume.channels,
		.vol = pa_cvolume_avg(&i->volume),
		.pid = NULL /* maybe obsolete */
	};

	sink_info *sink = g_list_nth_data(ctx->tmp_sinks, i->sink);
	list_append_struct(sink->input_list, sink_input);
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

void
collect_all_info(struct context *ctx)
{
	if (!ctx->info_callbacks_finished)
		return;
	ctx->info_callbacks_finished = FALSE;
	ctx->tmp_sinks = NULL;
	pa_operation_unref(pa_context_get_sink_info_list(ctx->context, get_sink_info_callback, ctx));
}
