#include <glib.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

#include "sink_input.h"
#include "sink.h"
#include "interface.h"
#include "pa-sink-ctl.h"

pa_context *context = NULL;
gboolean context_ready = FALSE;

static gboolean info_callbacks_finished = TRUE;
static gboolean info_callbacks_blocked = FALSE;

int
main(int argc, char** argv)
{
	pa_mainloop_api  *mainloop_api = NULL;
	pa_glib_mainloop *m            = NULL;

	sink_list = sink_list_alloc();

	GMainLoop *g_loop = g_main_loop_new(NULL, FALSE);

	interface_init();

	if (!(m = pa_glib_mainloop_new(NULL))) {
		interface_clear();
		g_printerr("error: pa_glib_mainloop_new() failed.\n");
		return -1;
	}

	mainloop_api = pa_glib_mainloop_get_api(m);

	if (!(context = pa_context_new(mainloop_api, "pa-sink-ctl"))) {
		interface_clear();
		g_printerr("error: pa_context_new() failed.\n");
		return -1;
	}
	
	// define callback for connection init
	pa_context_set_state_callback(context, context_state_callback, g_loop);
	if (pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL)) {
		interface_clear();
		g_printerr("error: pa_context_connect() failed.\n");
	}

	g_main_loop_run(g_loop);

	interface_clear();
	sink_list_free(sink_list);

	pa_glib_mainloop_free(m);
	g_main_loop_unref(g_loop);

	return 0;
}

static void
subscribe_cb(pa_context *c, pa_subscription_event_type_t t, guint32 idx, gpointer userdata)
{
	if (!info_callbacks_finished)
		info_callbacks_blocked = TRUE;
	else
		collect_all_info();
}

/*
 * is called after connection
 */
void
context_state_callback(pa_context *c, gpointer userdata)
{
	static pa_operation *o = NULL;
	context_ready = FALSE;
	switch (pa_context_get_state(c)) {
		case PA_CONTEXT_CONNECTING:
			interface_set_status("connecting...");
			break;
		case PA_CONTEXT_AUTHORIZING:
			interface_set_status("authorizing...");
			break;
		case PA_CONTEXT_SETTING_NAME:
			interface_set_status("setting name...");
			break;

		case PA_CONTEXT_READY:
			collect_all_info();
			pa_context_set_subscribe_callback(context, subscribe_cb, NULL);
			g_assert((o = pa_context_subscribe(c, (pa_subscription_mask_t) (
					PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SINK_INPUT
					), NULL, NULL)));
			context_ready = TRUE;
			interface_set_status("ready to process events.");
			break;
		case PA_CONTEXT_FAILED:
			interface_set_status("cannot connect!");
			break;

		case PA_CONTEXT_TERMINATED:
			g_assert(o != NULL);
			pa_operation_cancel(o);
			pa_operation_unref(o);
			o = NULL;
			interface_set_status("connection terminated.");
			g_main_loop_quit((GMainLoop *)userdata);
			break;
		default:
			interface_set_status("unknown state");
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
	GArray *sink_list_tmp = userdata;

	if (is_last < 0) {
		g_printerr("Failed to get sink information: %s\n", pa_strerror(pa_context_errno(c)));
		quit();
	}

	if (is_last) {
		pa_operation_unref(pa_context_get_sink_input_info_list(c, get_sink_input_info_callback, sink_list_tmp));
		return;
	}

	g_array_append_val(sink_list_tmp, ((sink_info) {
		.index = i->index,
		.mute  = i->mute,
		.vol   = pa_cvolume_avg(&i->volume),
		.channels = i->volume.channels,
		.name = g_strdup(i->name),
		.device = pa_proplist_contains(i->proplist, "device.product.name") ? 
			g_strdup(pa_proplist_gets(i->proplist, "device.product.name")) : NULL,
		.input_list = sink_input_list_alloc()
	}));
}

/*
 * is called after sink-input
 */
void
get_sink_input_info_callback(pa_context *c, const pa_sink_input_info *i, gint is_last, gpointer userdata)
{
	g_assert(userdata != NULL);
	GArray *sink_list_tmp = userdata;

	if (is_last < 0) {
		g_printerr("Failed to get sink input information: %s\n", pa_strerror(pa_context_errno(c)));
		return;
	}

	if (is_last) {
		info_callbacks_finished = TRUE;
		sink_list_free(sink_list);
		sink_list = sink_list_tmp;

		print_sink_list();

		if (info_callbacks_blocked) {
			info_callbacks_blocked = FALSE;
			collect_all_info();
		}
		return;
	}

	if (!(i->client != PA_INVALID_INDEX)) return;

	g_array_append_val(g_array_index(sink_list_tmp, sink_info, i->sink).input_list, ((sink_input_info) {
		.index = i->index,
		.sink = i->sink,
		.name = pa_proplist_contains(i->proplist, "application.name") ?
			g_strdup(pa_proplist_gets(i->proplist, "application.name")):
			g_strdup(i->name),
		.mute = i->mute,
		.channels = i->volume.channels,
		.vol = pa_cvolume_avg(&i->volume),
		.pid = NULL /* maybe obsolete */
	}));
}

void
quit(void)
{
	pa_context_disconnect(context);
}

/*
 * is called, after user input
 */
void
change_callback(pa_context* c, gint success, gpointer userdata)
{
	return;
}

void
collect_all_info(void)
{
	if (!info_callbacks_finished)
		return;
	info_callbacks_finished = FALSE;
	pa_operation_unref(pa_context_get_sink_info_list(context, get_sink_info_callback, sink_list_alloc()));
}
