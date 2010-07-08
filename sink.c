#include <stdio.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#include "sink_input.h"
#include "sink.h"

sink_info* sink_init(void) {
	
	sink_info* sink = (sink_info*) calloc(1, sizeof(sink_info));
	
	sink->name = NULL;
	sink->input_counter = 0;
	sink->input_max = 1;
	sink->input_list = NULL;

	sink_input_list_init(sink->input_list, sink->input_max);

	return sink;
}

/*
 * free's sink 
 */
void sink_clear(sink_info* sink) {

	if (sink->name != NULL)
		free(sink->name);

	sink_input_list_clear(sink->input_list, &sink->input_max);

	free(sink);
}
