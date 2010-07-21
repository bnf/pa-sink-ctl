#ifndef PA_SINK_CTL_H
#define PA_SINK_CTL_H

#include <glib.h>
#include <pulse/pulseaudio.h>

void collect_all_info(void);
void quit(void);

void context_state_callback(pa_context*, gpointer);
void get_sink_info_callback(pa_context *, const pa_sink_info *, gint, gpointer);
void get_sink_input_info_callback(pa_context *, const pa_sink_input_info*, gint, gpointer);
void change_callback(pa_context* c, gint success, gpointer);

#endif
