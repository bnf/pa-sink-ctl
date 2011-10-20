#ifndef UNIX_SIGNAL_H
#define UNIX_SIGNAL_H

#include <glib.h>

GSource *
unix_signal_source_new(gint signum);
guint
unix_signal_add(gint signum, GSourceFunc function, gpointer data);
guint
unix_signal_add_full(gint priority, gint signum, GSourceFunc function,
		     gpointer data, GDestroyNotify notify);

#endif /* UNIX_SIGNAL_H */
