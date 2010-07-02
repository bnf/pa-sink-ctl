#include <stdio.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <string.h>

static void context_state_callback(pa_context*, void *);
static void get_sink_input_info_callback(pa_context *, const pa_sink_input_info*, int, void *);
static void print_sinks(void);

typedef struct _sink_input_info {
	uint32_t sink;
	char *name;
	char *pid;
	pa_volume_t vol;
} sink_input_info;

static sink_input_info** sink_input_list = 0;
int sink_input_counter;
int sink_input_max;

static pa_mainloop_api *mainloop_api = NULL;
static pa_context *context = NULL;

int main(int argc, char** argv)
{
	sink_input_counter = 0;
	sink_input_max = 1;

	sink_input_list = (sink_input_info**) calloc(sink_input_max, sizeof(sink_input_info*));

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

static void context_state_callback(pa_context *c, void *userdata) {

	switch (pa_context_get_state(c)) {
		case PA_CONTEXT_CONNECTING:
			printf("connecting...\n");
			break;

		case PA_CONTEXT_AUTHORIZING:
			printf("authorizing...\n");
			break;

		case PA_CONTEXT_SETTING_NAME:
			printf("setting name\n");
			break;

		case PA_CONTEXT_READY:
			printf("Menue\n");

			pa_operation_unref(pa_context_get_sink_input_info_list(c, get_sink_input_info_callback, NULL));
			break;
		default:
			printf("unknown state\n");
			break;
	}
}

static void get_sink_input_info_callback(pa_context *c, const pa_sink_input_info *i, int is_last, void *userdata) {
	char t[32], k[32], cv[PA_CVOLUME_SNPRINT_MAX];

	if (is_last < 0) {
		printf("Failed to get sink input information: %s\n", pa_strerror(pa_context_errno(c)));
		return;
	}

	if (is_last) {
		print_sinks();
		return;
	}

	snprintf(t, sizeof(t), "%u", i->owner_module);
	snprintf(k, sizeof(k), "%u", i->client);

	printf( "Sink Input #%u"
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

	++sink_input_counter;

	if (sink_input_counter >= sink_input_max) {
		sink_input_max*=2;
		sink_input_list = (sink_input_info**) realloc(sink_input_list, sizeof(sink_input_info) * sink_input_max);
	}

	sink_input_list[sink_input_counter] = (sink_input_info*) calloc(1, sizeof(sink_input_info));
	sink_input_list[sink_input_counter]->name = (char*) calloc(strlen(i->name) + 1, sizeof(char));

	sink_input_list[sink_input_counter]->sink = i->sink;
	strncpy(sink_input_list[sink_input_counter]->name, i->name, strlen(i->name));
	sink_input_list[sink_input_counter]->vol = pa_cvolume_avg(&i->volume);
}

void print_sinks(void)
{
	printf("print sinks: %d\n", sink_input_counter);

	for(int i = 0; i < sink_input_counter; ++i) {
		printf(	"\t%s\t\n",
			sink_input_list[i]->name);
//			sink_input_list[i]->vol);
	}
}
