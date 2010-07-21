#include <stdio.h>

#include <glib.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

#include "sink_input.h"
#include "sink.h"
#include "interface.h"
#include "pa-sink-ctl.h"

GArray *sink_list     = NULL;
GArray *sink_list_tmp = NULL;

pa_mainloop_api *mainloop_api = NULL;
pa_context      *context      = NULL;

gboolean info_callbacks_finished = TRUE;
gboolean state_callback_pending = FALSE;

int main(int argc, char** argv)
{
	GMainContext *g_context = NULL;
	GMainLoop    *g_loop    = NULL;
	pa_glib_mainloop *m = NULL;

	sink_list = sink_list_alloc();

	interface_init();

	g_context = g_main_context_default();
	g_loop    = g_main_loop_new(g_context, FALSE);

	if (!(m = pa_glib_mainloop_new(g_context))) {
		printf("error: pa_glib_mainloop_new() failed.\n");
		return -1;
	}

	mainloop_api = pa_glib_mainloop_get_api(m);

	if (!(context = pa_context_new(mainloop_api, "pa-sink-ctl"))) {
		printf("error: pa_context_new() failed.\n");
		return -1;
	}
	
	// define callback for connection init
	pa_context_set_state_callback(context, context_state_callback, NULL);
	if (pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL)) {
		printf("error: pa_context_connect() failed.\n");
	}

	g_main_loop_run(g_loop);

	pa_glib_mainloop_free(m);

	g_main_loop_unref(g_loop);
	g_main_context_unref(g_context);

	return 0;
}

static void subscribe_cb(pa_context *c, pa_subscription_event_type_t t, guint32 idx, gpointer userdata)
{
	if (!info_callbacks_finished)
		state_callback_pending = TRUE;
	else
		collect_all_info();
}

static int loop(gpointer data)
{
	get_input();
	return TRUE;
}

/*
 * is called after connection
 */
void context_state_callback(pa_context *c, gpointer userdata)
{
	switch (pa_context_get_state(c)) {
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
			break;

		case PA_CONTEXT_READY:
			collect_all_info();
			g_timeout_add(3, loop, NULL);
			pa_context_set_subscribe_callback(context, subscribe_cb, NULL);
			pa_operation *o;
			g_assert((o = pa_context_subscribe(c, (pa_subscription_mask_t) (
					PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SINK_INPUT
					), NULL, NULL)));
			break;

		default:
			printf("unknown state\n");
			break;
	}
}

/*
 * the begin of the callback loops
 */
void get_sink_info_callback(pa_context *c, const pa_sink_info *i, gint is_last, gpointer userdata)
{
	if (is_last < 0) {
		printf("Failed to get sink information: %s\n", pa_strerror(pa_context_errno(c)));
		quit();
	}

	if (is_last) {
		pa_operation_unref(pa_context_get_sink_input_info_list(c, get_sink_input_info_callback, NULL));
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
void get_sink_input_info_callback(pa_context *c, const pa_sink_input_info *i, gint is_last, gpointer userdata)
{
	if (is_last < 0) {
		printf("Failed to get sink input information: %s\n", pa_strerror(pa_context_errno(c)));
		return;
	}

	if (is_last) {
		info_callbacks_finished = TRUE;
		sink_list_free(sink_list);
		sink_list = sink_list_tmp;

		print_sink_list();

		if (state_callback_pending) {
			state_callback_pending = FALSE;
			collect_all_info();
		}
		return;
	}

	if (!(i->client != PA_INVALID_INDEX)) return;

	g_array_append_val(g_array_index(sink_list_tmp, sink_info, i->sink).input_list, ((sink_input_info) {
		.index = i->index,
		.sink = i->sink,
		.name = g_strdup(pa_proplist_gets(i->proplist, "application.name")),
		.mute = i->mute,
		.channels = i->volume.channels,
		.vol = pa_cvolume_avg(&i->volume),
		.pid = NULL /* maybe obsolete */
	}));
}

void quit(void)
{
	sink_list_free(sink_list);
	interface_clear();
	exit(0);
}

/*
 * is called, after user input
 */
void change_callback(pa_context* c, gint success, gpointer userdata)
{
	return;
}

void collect_all_info(void)
{
	if (!info_callbacks_finished)
		return;
	info_callbacks_finished = FALSE;

	sink_list_tmp = sink_list_alloc();
	pa_operation_unref(pa_context_get_sink_info_list(context, get_sink_info_callback, NULL));
}
