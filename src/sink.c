#include <stdio.h>
#include <glib.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#include "sink_input.h"
#include "sink.h"

extern GArray *sink_list;

sink_info *sink_list_get(int index) {
	return &g_array_index(sink_list, sink_info, index);
}

sink_input_info *sink_input_get(int sink_list_index, int index) {
	return &g_array_index(sink_list_get(sink_list_index)->input_list, sink_input_info, index);
}

/*
 * init a sink list
 */
GArray *sink_list_alloc(void)
{
	return g_array_sized_new(false, false, sizeof(sink_info), 16);
}

/*
 * frees all dynamic allocated components of a sink 
 */
static void sink_clear(sink_info* sink)
{
	if (sink->name != NULL)
		free(sink->name);
	if (sink->device != NULL)
		free(sink->device);
	sink_input_list_free(sink->input_list);
}

/*
 * frees a complete sink array
 */
void sink_list_free(GArray *sink_list)
{
	for (int i = 0; i < sink_list->len; ++i)
		sink_clear(&g_array_index(sink_list, sink_info, i));
	g_array_free(sink_list, true);
}


