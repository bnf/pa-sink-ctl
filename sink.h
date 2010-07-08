#ifndef SINK_H
#define SINK_H

#include <stdio.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#include "sink_input.h"

typedef struct _sink_info {
	uint32_t index;
	char* name;
	int mute;
	pa_volume_t vol;
	
	// input list
	int input_counter;
	int input_max;
	sink_input_info** input_list;
} sink_info;

sink_info* sink_init(void);
void sink_clear(sink_info*);

#endif
