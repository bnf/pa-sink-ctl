#include <stdio.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <string.h>
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
int sink_counter;
uint32_t sink_max;

sink_input_info** sink_input_list = NULL;
int sink_input_counter;
int sink_input_max;

pa_mainloop_api *mainloop_api = NULL;
pa_context *context = NULL;

// ncurses
WINDOW *menu_win;
int chooser;
int startx;
int starty;

int main(int argc, char** argv)
{
	// pulseaudio
	sink_input_counter = 0;
	sink_input_max = 1;
	sink_input_list = (sink_input_info**) calloc(sink_input_max, sizeof(sink_input_info*));

	sink_counter = 0;
	sink_max = 1;
	sink_list = (sink_info**) calloc(sink_max, sizeof(sink_info*));

	// ncurses
	chooser = 0;

	initscr();
	clear();
	noecho();
	cbreak();	/* Line buffering disabled. pass on everything */
	startx = (80 - WIDTH) / 2;
	starty = (24 - HEIGHT) / 2;
	menu_win = newwin(HEIGHT, WIDTH, starty, startx);
	keypad(menu_win, TRUE);
	mvprintw(0, 0, "Use arrow keys to go up and down, Press enter to select a choice");
	refresh();

	pa_mainloop *m = NULL;
	int ret = 1;

	if(!(m = pa_mainloop_new())) {
		printf("error: pa_mainloop_new() failed.\n");
		return -1;
	}

	mainloop_api = pa_mainloop_get_api(m);

	if (!(context = pa_context_new(mainloop_api, "pa-sink-ctl"))) {
		printf("error: pa_context_new() failed.\n");
		return -1;
	}

	pa_context_set_state_callback(context, context_state_callback, NULL);
	if (pa_context_connect(context, "tcp:127.0.0.1:4713", PA_CONTEXT_NOAUTOSPAWN, NULL)) {
		printf("error: pa_context_connect() failed.\n");
	}

	if (pa_mainloop_run(m, &ret) < 0) {
		printf("error: pa_mainloop_run() failed.\n");
		return -1;
	}

	return ret;
}

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
			pa_operation_unref(pa_context_get_sink_info_list(c, get_sink_info_callback, NULL));
//			pa_operation_unref(pa_context_get_sink_input_info_list(c, get_sink_input_info_callback, NULL));
			break;
		default:
			printf("unknown state\n");
			break;
	}
}

void get_sink_info_callback(pa_context *c, const pa_sink_info *i, int is_last, void *userdata) {

	if (is_last < 0) {
		printf("Failed to get sink information: %s", pa_strerror(pa_context_errno(c)));
		quit();
	}

	if (is_last) {
		pa_operation_unref(pa_context_get_sink_input_info_list(c, get_sink_input_info_callback, NULL));
		return;
	}
	
	++sink_counter;
	if (sink_counter >= sink_max) {
		sink_max *= 2;
		sink_list = realloc(sink_list, sizeof(sink_info*) * sink_max);
	}
	
	sink_list[sink_counter - 1] = (sink_info*) calloc(1, sizeof(sink_info));
	sink_list[sink_counter - 1]->index = i->index;
	sink_list[sink_counter - 1]->mute = i->mute;
	
	sink_list[sink_counter - 1]->name = (char*) calloc(strlen(i->name) + 1, sizeof(char));
	strncpy(sink_list[sink_counter - 1]->name, i->name, strlen(i->name));
}

void get_sink_input_info_callback(pa_context *c, const pa_sink_input_info *i, int is_last, void *userdata) {
	char t[32], k[32]; //,cv[PA_CVOLUME_SNPRINT_MAX];

	if (is_last < 0) {
		printf("Failed to get sink input information: %s\n", pa_strerror(pa_context_errno(c)));
		return;
	}

	if (is_last) {
		print_sinks();
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

	const char *name = pa_proplist_gets(i->proplist, "application.name");

//	if (i->sink > sink_max)
//		sink_max = i->sink;

	++sink_input_counter;

	if (sink_input_counter >= sink_input_max) {
		sink_input_max*=2;
		sink_input_list = (sink_input_info**) realloc(sink_input_list, sizeof(sink_input_info*) * sink_input_max);
	}

	sink_input_list[sink_input_counter-1] = (sink_input_info*) calloc(1, sizeof(sink_input_info));
	sink_input_list[sink_input_counter-1]->name = (char*) calloc(strlen(name) + 1, sizeof(char));

	sink_input_list[sink_input_counter-1]->index = i->index;
	sink_input_list[sink_input_counter-1]->sink = i->sink;
	strncpy(sink_input_list[sink_input_counter-1]->name, name, strlen(name));
	sink_input_list[sink_input_counter-1]->vol = pa_cvolume_avg(&i->volume);
}

void quit(void)
{
	clrtoeol();
	refresh();
	endwin();
	exit(0);
}

void change_callback(pa_context* c, int success, void* userdate) {
	sink_input_counter = 0;
	pa_operation_unref(pa_context_get_sink_input_info_list(context, get_sink_input_info_callback, NULL));
}
