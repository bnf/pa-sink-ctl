#ifndef SINK_INPUT_H
#define SINK_INPUT_H

#include <glib.h>
#include <pulse/pulseaudio.h>

typedef struct _sink_input_info {
	guint32 index;
	guint32 sink;
	gchar *name;
	gchar *pid;	// maybe useless?!!?
	gint mute;
	guint8 channels;
	pa_volume_t vol; // TOTO: exchange with the channel-list
} sink_input_info;

#endif
