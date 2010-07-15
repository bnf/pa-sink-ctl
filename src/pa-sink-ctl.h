#ifndef PA_SINK_CTL_H
#define PA_SINK_CTL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <string.h>

#include "sink_input.h"
#include "sink.h"
#include "interface.h"

#define VOLUME_MIN ((intmax_t) PA_VOLUME_MUTED)
#define VOLUME_MAX ((intmax_t) PA_VOLUME_NORM)

#define VOLUME_BAR_LEN 50
#define WIDTH 80
#define HEIGHT 10

void context_state_callback(pa_context*, void *);
void get_sink_info_callback(pa_context *, const pa_sink_info *, int, void *);
void get_sink_input_info_callback(pa_context *, const pa_sink_input_info*, int, void *);
void change_callback(pa_context* c, int success, void* userdate);
void quit(void);

void collect_all_info(void);

#endif
