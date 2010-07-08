#include <stdio.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#include "sink_input.h"
#include "sink.h"

/*
 * return an initilized sink
 */
sink_info* sink_init(void) {
	
	sink_info* sink = (sink_info*) calloc(1, sizeof(sink_info));
	
	sink->name = NULL;
	sink->input_counter = 0;
	sink->input_max = 1;
	sink->input_list = NULL;

	sink->input_list = sink_input_list_init(sink->input_max);

	return sink;
}

/*
 * frees all components of a sink 
 */
void sink_clear(sink_info* sink) {

	if (sink->name != NULL)
		free(sink->name);

	sink_input_list_clear(sink->input_list, &sink->input_max);

	free(sink);
	sink = NULL;
}

void sink_check_list(sink_info* sink) {
	
	if (sink->input_counter >= sink->input_max)
		sink_input_list_enlarge(sink->input_list, &sink->input_max, sink->input_counter);
}

sink_info** sink_list_init(int max) {

	sink_info** sink_list = (sink_info**) calloc(max, sizeof(sink_info*));

	for (int i = 0; i < max; ++i)
		sink_list[i] = NULL;

	return sink_list;
}

void sink_list_clear(sink_info** sink_list, int* max) {

	for (int i = 0; i < (*max); ++i)
		if (sink_list[i] != NULL)
			sink_clear(sink_list[i]);

	(*max) = 0;
	
	free(sink_list);
	sink_list = NULL;
}
