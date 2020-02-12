/*
 * pa-sink-ctl - NCurses based Pulseaudio control client
 * Copyright (C) 2011  Benjamin Franzke <benjaminfranzke@googlemail.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <signal.h>
#include <glib.h>
#include "unix_signal.h"

static GPtrArray *signal_data = NULL;

typedef struct _UnixSignalData {
	guint source_id;
	GMainContext *context;
	gboolean triggered;
	gint signum;
} UnixSignalData;

typedef struct _UnixSignalSource {
	GSource source;
	UnixSignalData *data;
} UnixSignalSource;

static void
handler(gint signum);

struct sigaction act_handler = {
	.sa_handler = handler
};
struct sigaction act_null = {
	.sa_handler = NULL
};

static inline UnixSignalData *
unix_signal_data(guint index)
{
	return (UnixSignalData *) g_ptr_array_index(signal_data, index);
}

static void
handler(gint signum)
{
	g_assert(signal_data != NULL);
	for (guint i = 0; i < signal_data->len; ++i)
		if (unix_signal_data(i)->signum == signum)
			unix_signal_data(i)->triggered = TRUE;
	sigaction(signum, &act_handler, NULL);
}

static gboolean
check(GSource *source)
{
	UnixSignalSource *signal_source = (UnixSignalSource *) source;

	return signal_source->data->triggered;
}

static gboolean
prepare(GSource *source, gint *timeout_)
{
	UnixSignalSource *signal_source = (UnixSignalSource*) source;

	if (signal_source->data->context == NULL) {
		g_main_context_ref(signal_source->data->context =
				   g_source_get_context(source));
		signal_source->data->source_id = g_source_get_id(source);
	}

	*timeout_ = -1;

	return signal_source->data->triggered;
}

static gboolean
dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	UnixSignalSource *signal_source = (UnixSignalSource *) source;

	signal_source->data->triggered = FALSE;

	return callback(user_data) ? TRUE : FALSE;
}

static void
finalize(GSource *source)
{
	UnixSignalSource *signal_source = (UnixSignalSource*) source;

	sigaction(signal_source->data->signum, &act_null, NULL);
	g_main_context_unref(signal_source->data->context);
	g_ptr_array_remove_fast(signal_data, signal_source->data);
	if (signal_data->len == 0)
		signal_data = (GPtrArray*) g_ptr_array_free(signal_data, TRUE);
	g_free(signal_source->data);

}
static GSourceFuncs SourceFuncs = 
{
	.prepare  = prepare,
	.check    = check,
	.dispatch = dispatch,
	.finalize = finalize,
	.closure_callback = NULL, .closure_marshal = NULL
};

static void
unix_signal_source_init(GSource *source, gint signum)
{
	UnixSignalSource *signal_source = (UnixSignalSource *) source;

	signal_source->data = g_new(UnixSignalData, 1);
	signal_source->data->triggered = FALSE;
	signal_source->data->signum    = signum;
	signal_source->data->context   = NULL;

	if (signal_data == NULL)
		signal_data = g_ptr_array_new();
	g_ptr_array_add(signal_data, signal_source->data);
}

GSource *
unix_signal_source_new(gint signum)
{
	GSource *source = g_source_new(&SourceFuncs, sizeof(UnixSignalSource));

	unix_signal_source_init(source, signum);
	sigaction(signum, &act_handler, NULL);

	return source;
}

guint
unix_signal_add_full(gint priority, gint signum, GSourceFunc function,
		     gpointer data, GDestroyNotify notify)
{
	guint id;
	GSource *source = unix_signal_source_new(signum);
	g_return_val_if_fail(function != NULL, 0);

	if (priority != G_PRIORITY_DEFAULT)
		g_source_set_priority (source, priority);
	g_source_set_callback(source, function, data, notify);
	id = g_source_attach(source, NULL);
	g_source_unref(source);

	return id;
}

guint unix_signal_add(gint signum, GSourceFunc function, gpointer data)
{
	return unix_signal_add_full(G_PRIORITY_DEFAULT, signum,
				    function, data, NULL);
}
