#include <glib.h>
#include "sink_input.h"

GArray *
sink_input_list_alloc(void)
{
	return g_array_sized_new(FALSE, FALSE, sizeof(sink_input_info), 8);
}

static void
sink_input_clear(sink_input_info* sink_input)
{
	g_free(sink_input->name);
	g_free(sink_input->pid);
}

void
sink_input_list_free(GArray *sink_input_list)
{
	for (int i = 0; i < sink_input_list->len; ++i)
		sink_input_clear(&g_array_index(sink_input_list, sink_input_info, i));
	g_array_free(sink_input_list, TRUE);
}
