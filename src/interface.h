#ifndef INTERFACE_H
#define INTERFACE_H

#include <glib.h>
#include <pulse/pulseaudio.h>

void print_sink_list(void);
void interface_init(void);
void interface_clear(void);
void interface_set_status(const gchar *);

#endif
