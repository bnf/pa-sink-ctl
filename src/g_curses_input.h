#ifndef G_CURESES_INPUT_H
#define G_CURESES_INPUT_H

#include <ncurses.h>
#include <glib.h>

GSource *g_curses_input_source_new(WINDOW *screen);
guint g_curses_input_add_full(gint priority, WINDOW *win, GSourceFunc function,
		gpointer data, GDestroyNotify notify);
guint g_curses_input_add(WINDOW *win, GSourceFunc function, gpointer data);

#endif /* G_CURESES_INPUT_H */
