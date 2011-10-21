#ifndef INTERFACE_H
#define INTERFACE_H

#include <glib.h>
#include <pulse/pulseaudio.h>


struct context;

void print_sink_list(struct context *ctx);
void interface_init(struct context *ctx);
void interface_clear(struct context *ctx);
void interface_set_status(struct context *ctx, const gchar *);

#endif
