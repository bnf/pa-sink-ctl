#ifndef PA_SINK_CTL_H
#define PA_SINK_CTL_H

#include <glib.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>

struct context {
	pa_context *context;
	pa_operation *op;
	gboolean context_ready;

	WINDOW *menu_win;
	WINDOW *msg_win;

	guint resize_source_id;
#ifdef HAVE_SIGNALFD
	int signal_fd;
#endif
	guint input_source_id;

	gint chooser_sink;
	gint chooser_input;
	guint32 selected_index;

	guint max_name_len;

	int info_callbacks_finished;
	gboolean info_callbacks_blocked;
	GMainLoop *loop;

	GList *sink_list;
	GList *input_list;
	GList *tmp_sinks;
	GList *tmp_inputs;

	gchar *status;
};

void
quit(struct context *ctx);

void
change_callback(pa_context* c, gint success, gpointer);

#endif
