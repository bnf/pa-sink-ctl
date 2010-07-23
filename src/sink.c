#include <glib.h>

#define SINK_C
#include "sink.h"
#include "sink_input.h"

/*
 * init a sink list
 */
GArray *sink_list_alloc(void)
{
	return g_array_sized_new(FALSE, FALSE, sizeof(sink_info), 16);
}

/*
 * frees all dynamic allocated components of a sink 
 */
static void sink_clear(sink_info* sink)
{
	g_free(sink->name);
	g_free(sink->device);
	sink_input_list_free(sink->input_list);
}

/*
 * frees a complete sink array
 */
void sink_list_free(GArray *sink_list)
{
	for (int i = 0; i < sink_list->len; ++i)
		sink_clear(&g_array_index(sink_list, sink_info, i));
	g_array_free(sink_list, TRUE);
}

/*
 * get sink at index from sink_list
 */
sink_info *sink_list_get(gint index)
{
	return &g_array_index(sink_list, sink_info, index);
}

/*
 * get an input association to an sink by their indizes
 */
sink_input_info *sink_input_get(gint sink_list_index, gint index)
{
	return &g_array_index(sink_list_get(sink_list_index)->input_list, sink_input_info, index);
}
