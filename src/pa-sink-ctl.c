#define _XOPEN_SOURCE 500
#include <string.h>
#include <stdio.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <stdlib.h>

#include "sink_input.h"
#include "sink.h"
#include "interface.h"
#include "pa-sink-ctl.h"

#define VOLUME_MAX UINT16_MAX
#define VOLUME_BAR_LEN 50
#define WIDTH 80
#define HEIGHT 10

sink_info** sink_list = NULL;
uint32_t sink_counter;
uint32_t sink_max;

pa_mainloop_api *mainloop_api = NULL;
pa_context *context = NULL;

// ncurses
WINDOW *menu_win;
int chooser;
int startx;
int starty;

int main(int argc, char** argv)
{
	sink_counter = 0;
	sink_max = 1;
	sink_list = sink_list_init(sink_max);

	interface_init();
	pa_mainloop *m = NULL;
	int ret = 1;

	if (!(m = pa_mainloop_new())) {
		printf("error: pa_mainloop_new() failed.\n");
		return -1;
	}

	mainloop_api = pa_mainloop_get_api(m);

	if (!(context = pa_context_new(mainloop_api, "pa-sink-ctl"))) {
		printf("error: pa_context_new() failed.\n");
		return -1;
	}
	
	// define callback for connection init
	pa_context_set_state_callback(context, context_state_callback, NULL);
	if (pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL)) {
		printf("error: pa_context_connect() failed.\n");
	}

	if (pa_mainloop_run(m, &ret) < 0) {
		printf("error: pa_mainloop_run() failed.\n");
		return -1;
	}

	return ret;
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
//			pa_operation_unref(pa_context_get_sink_input_info_list(c, get_sink_input_info_callback, NULL));
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

	sink_list_check(&sink_list, &sink_max, sink_counter);
	sink_check(&(sink_list[sink_counter]));
	sink_list[sink_counter]->index = i->index;
	sink_list[sink_counter]->mute = i->mute;
	sink_list[sink_counter]->vol = pa_cvolume_avg(&i->volume);
	sink_list[sink_counter]->channels = i->volume.channels;
	sink_list[sink_counter]->name = strdup(i->name);
	sink_list[sink_counter]->device = strdup(pa_proplist_gets(i->proplist, "device.product.name"));

	++sink_counter;
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

/*	printf( "Sink Input #%u"
		"\tClient: %s"
		"\tSink: %u"
		"\tMute: %d"
		"\tVolume: %s"
		"\tname: %s"
		"\tpid: %s\n",
			i->index,
			i->client != PA_INVALID_INDEX ? k : "n/a",
			i->sink,
			i->mute,
			pa_cvolume_snprint(cv, sizeof(cv), &i->volume),
			pa_proplist_gets(i->proplist, "application.name"),
			pa_proplist_gets(i->proplist, "application.process.id"));
*/

//	const char *name = pa_proplist_gets(i->proplist, "application.name");

	int sink_num = i->sink;
	int counter = sink_list[sink_num]->input_counter;
	// check the length of the list
	sink_check_input_list(sink_list[sink_num]);
	// check the current element of the list
	sink_input_check(&(sink_list[ sink_num ]->input_list[ counter ]));
	sink_input_info* input = sink_list[sink_num]->input_list[counter];
	input->name = strdup(pa_proplist_gets(i->proplist, "application.name"));
	input->index = i->index;
	input->channels = i->volume.channels;
	input->vol = pa_cvolume_avg(&i->volume);
	input->mute = i->mute;

	++(sink_list[sink_num]->input_counter);
}

void quit(void) {
	sink_list_clear(sink_list, &sink_max, &sink_counter);
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
	sink_list_reset(sink_list, &sink_counter);
	pa_operation_unref(pa_context_get_sink_info_list(context, get_sink_info_callback, NULL));
}
