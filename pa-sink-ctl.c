#include <stdio.h>
#include <pulse/pulseaudio.h>

//#include <config.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#include <locale.h>
//#include <sndfile.h>
//#include <pulse/i18n.h>
#include <pulse/pulseaudio.h>

//#include <pulsecore/macro.h>
//#include <pulsecore/core-util.h>
//#include <pulsecore/log.h>
//#include <pulsecore/sndfile-util.h>


static void context_state_callback(pa_context*, void *);
static void get_sink_input_info_callback(pa_context *, const pa_sink_input_info*, int, void *);

static pa_mainloop_api *mainloop_api = NULL;
static pa_context *context = NULL;

int main(int argc, char** argv)
{
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
	char t[32], k[32], cv[PA_CVOLUME_SNPRINT_MAX];// cvdb[PA_SW_CVOLUME_SNPRINT_DB_MAX];
//	char *pl;

	if (is_last < 0) {
		printf("Failed to get sink input information: %s", pa_strerror(pa_context_errno(c)));
		return;
	}

	if (is_last) {
//		complete_action();
		return;
	}

//	pa_assert(i);

//	if (nl)
//		printf("\n");
//	nl = TRUE;

	snprintf(t, sizeof(t), "%u", i->owner_module);
	snprintf(k, sizeof(k), "%u", i->client);

	printf("Sink Input #%u"
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

//	pa_xfree(pl);
}
