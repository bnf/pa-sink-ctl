#ifndef INTERFACE_H
#define INTERFACE_H

#include <glib.h>
#include <pulse/pulseaudio.h>

void print_sink_list(void);
void print_input_list(gint);
void print_volume(pa_volume_t, gint, gint);
void get_input(void);
void interface_init(void);
void interface_resize(void);
void interface_clear(void);

#endif
