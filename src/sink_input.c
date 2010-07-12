#include <stdio.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#include "sink_input.h"

sink_input_info* sink_input_init() {

	sink_input_info* sink_input = (sink_input_info*) calloc(1, sizeof(sink_input_info));
	sink_input->name = NULL;
	sink_input->pid = NULL;

	return sink_input;
}

void sink_input_clear(sink_input_info* sink_input) {
	
	if (sink_input->name != NULL)
		free(sink_input->name);
	
	if (sink_input->pid != NULL)
		free(sink_input->pid);

	free(sink_input);
	sink_input = NULL;
}

sink_input_info**  sink_input_list_init(int max) {

	sink_input_info** sink_input_list = (sink_input_info**) calloc(max, sizeof(sink_input_info*));

	for (int i = 0; i < max; ++i)
		sink_input_list = NULL;
	
	return sink_input_list;
}

void sink_input_list_enlarge(sink_input_info** sink_input_list, int* max, int counter) {
	
	(*max) *= 2;
	sink_input_list = (sink_input_info**) realloc(sink_input_list, (*max) * sizeof(sink_input_info*));

	for (int i = counter; i < (*max); ++i)
		sink_input_list[i] = NULL;
}

void sink_input_list_clear(sink_input_info** sink_input_list, int *max) {
	
	for (int i = 0; i < (*max); ++i)
		sink_input_clear(sink_input_list[i]);

	(*max) = 0;

	free(sink_input_list);
	sink_input_list = NULL;
}

void sink_input_check(sink_input_info** sink_input) {

	if ((*sink_input) == NULL)
		(*sink_input) = (sink_input_info*) calloc(1, sizeof(sink_input_info));
}

int cmp_sink_input_list(const void *a, const void *b) {

	sink_input_info* sinka = *((sink_input_info**) a);
	sink_input_info* sinkb = *((sink_input_info**) b);

	if (sinka->sink < sinkb->sink)
		return -1;
	else if (sinka->sink > sinkb->sink)
		return 1;
	else
		return 0;
}

