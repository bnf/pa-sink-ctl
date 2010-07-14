#ifndef SINK_INPUT_H
#define SINK_INPUT_H

#include <stdint.h>

#include <glib.h>
#include <pulse/pulseaudio.h>
// TODO: change this with the given define from pulselib
#define VOLUME_MAX UINT16_MAX

typedef struct _sink_input_info {
	uint32_t index;
	uint32_t sink;
	char *name;
	char *pid;	// maybe useless?!!?
	int mute;
	uint8_t channels;
	pa_volume_t vol; // TOTO: exchange with the channel-list
} sink_input_info;

GArray *sink_input_list_alloc(void);
void sink_input_list_free(GArray *sink_input_list);

#endif
