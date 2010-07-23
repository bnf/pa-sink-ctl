#include <curses.h>
#include <glib.h>
#include "interface.h"

typedef struct _GCursesInput {
	GSource source;
	WINDOW *win;
} GCursesInput;

static gboolean check(GSource *source)
{
	GCursesInput *curses_input = (GCursesInput*) source;
	static int i = 0;
	i++;
	gint ch = wgetch(curses_input->win);
	if (ch != ERR)
		ungetch(ch);
	return ch != ERR;
}

static gboolean prepare(GSource *source, gint *timeout_)
{
	*timeout_ = 2;
	return check(source);
}

static gboolean dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
	GCursesInput *curses_input = (GCursesInput*) source;
	return callback((gpointer)curses_input->win) ? TRUE : FALSE;
}

static GSourceFuncs SourceFuncs = 
{
	.prepare  = prepare,
	.check    = check,
	.dispatch = dispatch,
	.finalize = NULL,
	.closure_callback = NULL, .closure_marshal = NULL
};

GSource *g_curses_input_source_new(WINDOW *win) {
	GSource *source = g_source_new(&SourceFuncs, sizeof(GCursesInput));
	GCursesInput *curses_input = (GCursesInput*) source;
	curses_input->win = win;
	nodelay(win, TRUE); /* important! make wgetch non-blocking */
	return source;
}

guint g_curses_input_add_full(gint priority, WINDOW *win, GSourceFunc function, gpointer data, GDestroyNotify notify)
{
	g_return_val_if_fail(function != NULL, 0);
	GSource *source = g_curses_input_source_new(win);
	if (priority != G_PRIORITY_DEFAULT)
		g_source_set_priority (source, priority);
	g_source_set_callback(source, function, data, notify);
	guint id = g_source_attach(source, NULL);
	g_source_unref(source);
	return id;
}

guint g_curses_input_add(WINDOW *win, GSourceFunc function, gpointer data)
{
	return g_curses_input_add_full(G_PRIORITY_DEFAULT, win, function, data, NULL);
}
