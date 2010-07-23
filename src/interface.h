#ifndef INTERFACE_H
#define INTERFACE_H

#include <glib.h>
#include <pulse/pulseaudio.h>

void print_sink_list(void);
void print_input_list(gint);
void print_volume(pa_volume_t, gint, gint);
gboolean get_input(gpointer);
void interface_init(void);
gboolean interface_resize(gpointer);
void interface_clear(void);
void status(gchar *);

#endif
