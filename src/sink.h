#ifndef SINK_H
#define SINK_H

#include <glib.h>
#include <pulse/pulseaudio.h>

#include "sink_input.h"

typedef struct _sink_info {
	guint32 index;
	gchar* name;
	gchar* device;
	gint mute;
	guint8 channels;
	pa_volume_t vol;
	GArray *input_list;
} sink_info;

GArray *sink_list_alloc(void);
void sink_list_free(GArray *sink_list);

sink_info *sink_list_get(gint index);
sink_input_info *sink_input_get(gint sink_list_index, gint index);

#endif
