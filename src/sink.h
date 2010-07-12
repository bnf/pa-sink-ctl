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

void sink_check(sink_info**);
void sink_list_check(sink_info***, uint32_t*, uint32_t);
void sink_check_input_list(sink_info*);

sink_info** sink_list_init(uint32_t);
void sink_list_reset(sink_info**, uint32_t*);
void sink_list_clear(sink_info**, uint32_t*, uint32_t*);

#endif
