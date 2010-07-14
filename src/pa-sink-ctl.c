#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

#include <ncurses.h>

#include "sink_input.h"
#include "sink.h"
#include "interface.h"
#include "pa-sink-ctl.h"

#define VOLUME_MAX UINT16_MAX
#define VOLUME_BAR_LEN 50
#define WIDTH 80
#define HEIGHT 10

GArray *sink_list = NULL;

pa_mainloop_api *mainloop_api = NULL;
pa_context      *context      = NULL;

int main(int argc, char** argv)
{
	GMainContext *g_context = NULL;
	GMainLoop    *g_loop    = NULL;
	pa_glib_mainloop *m = NULL;

	sink_list_alloc(&sink_list);

	interface_init();

	g_context = g_main_context_new();
	g_loop    = g_main_loop_new(g_context, false);

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
/*
 * is called after connection
 */
void context_state_callback(pa_context *c, void *userdata) {

	switch (pa_context_get_state(c)) {
		case PA_CONTEXT_CONNECTING:
//			printf("connecting...\n");
			break;

		case PA_CONTEXT_AUTHORIZING:
//			printf("authorizing...\n");
			break;

		case PA_CONTEXT_SETTING_NAME:
//			printf("setting name\n");
			break;

		case PA_CONTEXT_READY:
//			printf("Menue\n");
			collect_all_info();
			break;

		default:
			printf("unknown state\n");
			break;
	}
}

/*
 * the begin of the callback loops
 */
void get_sink_info_callback(pa_context *c, const pa_sink_info *i, int is_last, void *userdata) {

	if (is_last < 0) {
		printf("Failed to get sink information: %s\n", pa_strerror(pa_context_errno(c)));
		quit();
	}

	if (is_last) {
		pa_operation_unref(pa_context_get_sink_input_info_list(c, get_sink_input_info_callback, NULL));
		return;
	}

	g_array_append_val(sink_list, ((sink_info) {
		.index = i->index,
		.mute  = i->mute,
		.vol   = pa_cvolume_avg(&i->volume),
		.channels = i->volume.channels,
		.name = strdup(i->name),
		.device = pa_proplist_contains(i->proplist, "device.product.name") ? 
			strdup(pa_proplist_gets(i->proplist, "device.product.name")) : NULL,

		.input_counter = 0,
		.input_max     = 1,
		.input_list    = sink_input_list_init(1)

	}));
}

/*
 * is called after sink-input
 */
void get_sink_input_info_callback(pa_context *c, const pa_sink_input_info *i, int is_last, void *userdata) {
	char t[32], k[32];

	if (is_last < 0) {
		printf("Failed to get sink input information: %s\n", pa_strerror(pa_context_errno(c)));
		return;
	}

	if (is_last) {
		print_sink_list();
		get_input(); 
		return;
	}

	if (!(i->client != PA_INVALID_INDEX)) return;

	snprintf(t, sizeof(t), "%u", i->owner_module);
	snprintf(k, sizeof(k), "%u", i->client);

	int sink_num = i->sink;
	int counter = g_array_index(sink_list, sink_info, sink_num).input_counter; 
	// check the length of the list
	sink_check_input_list(&g_array_index(sink_list, sink_info, sink_num));

	// check the current element of the list
	sink_input_check(&(g_array_index(sink_list, sink_info, sink_num).input_list[counter]));

	sink_input_info* input = g_array_index(sink_list, sink_info, sink_num).input_list[counter];
	input->name = strdup(pa_proplist_gets(i->proplist, "application.name"));
	input->index = i->index;
	input->channels = i->volume.channels;
	input->vol = pa_cvolume_avg(&i->volume);
	input->mute = i->mute;

	++(g_array_index(sink_list, sink_info, sink_num).input_counter);
}

void quit(void) {
	sink_list_free(sink_list);
	interface_clear();
	exit(0);
}

/*
 * is called, after user input
 */
void change_callback(pa_context* c, int success, void* userdate) {

	// get information about sinks
	collect_all_info();
}

void collect_all_info(void) {
	sink_list_free(sink_list);
	sink_list_alloc(&sink_list);
	pa_operation_unref(pa_context_get_sink_info_list(context, get_sink_info_callback, NULL));
}
