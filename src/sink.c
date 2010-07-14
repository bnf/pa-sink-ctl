#include <stdio.h>
#include <glib.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#include "sink_input.h"
#include "sink.h"

void sink_check_input_list(sink_info* sink) {
	
	if (sink->input_counter >= sink->input_max)
		sink_input_list_enlarge(&sink->input_list, &sink->input_max, sink->input_counter);
}

/*
 * init a sink list
 */
void sink_list_alloc(GArray **sink_list) {
	*sink_list = g_array_sized_new(false, false, sizeof(sink_info), 16);
}

/*
 * frees a complete sink array
 */
void sink_list_free(GArray *sink_list) {
	for (int i = 0; i < sink_list->len; ++i)
		sink_clear(&g_array_index(sink_list, sink_info, i));
	g_array_free(sink_list, true);
}

/*
 * frees all dynamic allocated components of a sink 
 */
void sink_clear(sink_info* sink) {

	if (sink->name != NULL)
		free(sink->name);
	
	if (sink->device != NULL)
		free(sink->device);

	sink_input_list_clear(sink->input_list, &sink->input_max);
}

