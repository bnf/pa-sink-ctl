#ifndef SINK_INPUT_H
#define SINK_INPUT_H

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

sink_input_info* sink_input_init(void);
void sink_input_clear(sink_input_info*);

sink_input_info** sink_input_list_init(int);
void sink_input_list_enlarge(sink_input_info***, int*, int);
void sink_input_list_clear(sink_input_info**, int*);
void sink_input_check(sink_input_info**);
int cmp_sink_input_list(const void *, const void *);

#endif
