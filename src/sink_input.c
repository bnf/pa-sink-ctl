#include <stdio.h>
#include <glib.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#include "sink_input.h"

GArray *sink_input_list_alloc(void) {
	return g_array_sized_new(false, false, sizeof(sink_input_info), 8);
}

static void sink_input_clear(sink_input_info* sink_input) {
	if (sink_input->name != NULL)
		free(sink_input->name);

	if (sink_input->pid != NULL)
		free(sink_input->pid);
}

void sink_input_list_free(GArray *sink_input_list) {
	for (int i = 0; i < sink_input_list->len; ++i)
		sink_input_clear(&g_array_index(sink_input_list, sink_input_info, i));
	g_array_free(sink_input_list, true);
}
