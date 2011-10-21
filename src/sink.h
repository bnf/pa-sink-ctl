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
	GList *input_list;
} sink_info;

#endif
