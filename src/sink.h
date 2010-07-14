#ifndef SINK_H
#define SINK_H

#include <stdio.h>
#include <glib.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#include "sink_input.h"

typedef struct _sink_info {
	uint32_t index;
	char* name;
	char* device;
	int mute;
	uint8_t channels;
	pa_volume_t vol;
	
	// input list
	int input_counter;
	int input_max;
	sink_input_info** input_list;
} sink_info;

void sink_check_input_list(sink_info*);

void sink_list_alloc(GArray **sink_list);
void sink_list_free(GArray *sink_list);

#endif
