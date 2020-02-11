/*
 * pa-sink-ctl - NCurses based Pulseaudio control client
 * Copyright (C) 2011  Benjamin Franzke <benjaminfranzke@googlemail.com>
 * Copyright (C) 2010  Jan Klemkow <web2p10@wemelug.de>
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

#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/ioctl.h>

#include <glib.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>

#include "interface.h"
#include "ctl.h"
#include "command.h"
#include "pa-sink-ctl.h"

#ifdef HAVE_SIGNALFD
#include <sys/signalfd.h>
#include <signal.h>
#else
#include "unix_signal.h"
#endif

static void
allocate_volume_bar(struct interface *ifc)
{
	gint x, y, max_x, max_y;
	(void) y;
	(void) max_y;

	if (ifc->volume_bar)
		return;

	getyx(ifc->menu_win, y, x);
	getmaxyx(ifc->menu_win, max_y, max_x);
	ifc->volume_bar_len = max_x - x - 8;
	if (ifc->volume_bar_len < 0)
		return;
	ifc->volume_bar = g_new(char, ifc->volume_bar_len+1);
	/* FIXME: if (ifc->volume_bar == NULL) */
	memset(ifc->volume_bar, '=', ifc->volume_bar_len);
	ifc->volume_bar[ifc->volume_bar_len] = '\0';
}

static void
print_volume(struct interface *ifc, struct vol_ctl *ctl)
{
	gint vol;

	if (!ctl->mute_set || !ctl->volume_set)
		return;

	allocate_volume_bar(ifc);
	vol = (gint) (ifc->volume_bar_len * ctl->vol / PA_VOLUME_NORM);
	wprintw(ifc->menu_win, " [%c][%-*.*s]",
		ctl->mute ? 'M':' ', ifc->volume_bar_len, vol, ifc->volume_bar);
}

static char *
ellipsize(char *str, size_t length)
{
	char *trimmed;
	size_t offset_start, offset_end;

	if (strlen(str) <= length) {
		return g_strdup(str);
	}

	trimmed = g_new(char, length + 1);

	offset_start = length/2 - 2;
	offset_end = length - offset_start - 3;

	strncpy(trimmed, str, offset_start);

	trimmed[offset_start] = '.';
	trimmed[offset_start+1] = '.';
	trimmed[offset_start+2] = '.';

	strncpy(&trimmed[offset_start+3], &str[strlen(str) - offset_end], offset_end);
	trimmed[length] = '\0';

	return trimmed;
}

static void
print_vol_ctl(gpointer data, gpointer user_data)
{
	struct vol_ctl *ctl = data;
	struct interface *ifc = user_data;
	gint x, y;
	size_t max_x, max_y, name_len;
	char *name;
	(void) max_y;

	getyx(ifc->menu_win, y, x);
	if (ctl == ifc->current_ctl)
		wattron(ifc->menu_win, A_REVERSE);

	getmaxyx(ifc->menu_win, max_y, max_x);
	name_len =(ifc->max_name_len > max_x / 2 ) ? max_x / 5 * 2 : ifc->max_name_len;

	if (!ctl->hide_index)
		wprintw(ifc->menu_win, "%2u ", ctl->index);
	name = ellipsize(ctl->name, name_len);
	wprintw(ifc->menu_win, "%*s%-*s",
		ctl->indent + (ctl->hide_index ? 2+1 : 0), "",
		name_len - ctl->indent, name);
	g_free(name);

	if (ctl == ifc->current_ctl)
		wattroff(ifc->menu_win, A_REVERSE);
	print_volume(ifc, ctl);
	wmove(ifc->menu_win, y+1, x);

	if (ctl->childs_foreach)
		ctl->childs_foreach(ctl, print_vol_ctl, ifc);
}

static void
max_name_len_helper(gpointer data, gpointer user_data)
{
	struct vol_ctl *ctl = data;
	struct interface *ifc = user_data;
	guint len;

	len = ctl->indent + strlen(ctl->name);
	if (len > ifc->max_name_len)
		ifc->max_name_len = len;

	if (ctl->childs_foreach)
		ctl->childs_foreach(ctl, max_name_len_helper, ifc);
}

void
interface_redraw(struct interface *ifc)
{
	struct context *ctx = container_of(ifc, struct context, interface);
	gint x, y;

	werase(ifc->menu_win);
	box(ifc->menu_win, 0, 0);

	ifc->max_name_len = 0;
	if (ifc->volume_bar) {
		g_free(ifc->volume_bar);
		ifc->volume_bar = NULL;
	}

	if (ifc->current_ctl == NULL)
		ifc->current_ctl = g_list_nth_data(ctx->sink_list, 0);
	if (ifc->current_ctl == NULL)
		ifc->current_ctl = g_list_nth_data(ctx->source_list, 0);

	g_list_foreach(ctx->sink_list, max_name_len_helper, ifc);
	g_list_foreach(ctx->source_list, max_name_len_helper, ifc);

	wmove(ifc->menu_win, 2, 2);
	wprintw(ifc->menu_win, "Sinks");
	wmove(ifc->menu_win, 3, 2); /* set initial cursor offset */
	g_list_foreach(ctx->sink_list, print_vol_ctl, ifc);
	getmaxyx(ifc->menu_win, y, x);
	whline(ifc->menu_win, 0, x - 4);
	getyx(ifc->menu_win, y, x);
	wmove(ifc->menu_win, y + 1, x);
	wprintw(ifc->menu_win, "Sources");
	wmove(ifc->menu_win, y + 2, x);
	g_list_foreach(ctx->source_list, print_vol_ctl, ifc);

	wrefresh(ifc->menu_win);
}

int
interface_get_main_ctl_length(struct interface *ifc)
{
	struct context *ctx = container_of(ifc, struct context, interface);

	return g_list_length(ctx->sink_list) + g_list_length(ctx->source_list);
}

static gboolean
interface_get_input(GIOChannel *source, GIOCondition condition, gpointer data)
{
	struct interface *ifc = data;
	struct context *ctx = container_of(ifc, struct context, interface);
	struct command_cb_descriptor *cmd;
	gint c;

	c = wgetch(ifc->menu_win);

	cmd = g_hash_table_lookup(ctx->config.keymap, GINT_TO_POINTER(c));
	if (cmd)
		cmd->cb(ctx, c);

	return TRUE;
}

void
interface_set_status(struct interface *ifc, const gchar *msg, ...)
{
	va_list ap;

	if (msg != NULL) {
		g_free(ifc->status);
		va_start(ap, msg);
		ifc->status = g_strdup_vprintf(msg, ap);
		va_end(ap);
	}
	werase(ifc->msg_win);
	box(ifc->msg_win, 0, 0);
	if (ifc->status != NULL)
		mvwprintw(ifc->msg_win, 1, 1, ifc->status);
	wrefresh(ifc->msg_win);
	refresh();
}

static gboolean
interface_resize(gpointer data)
{
	struct interface *ifc = data;
	struct winsize wsize;
	gint height = 80;
	gint width  = 24;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsize) >= 0) {
		height = wsize.ws_row;
		width  = wsize.ws_col;
	}

	resize_term(height, width);
	clear();
	refresh();

	wresize(ifc->menu_win, height - H_MSG_BOX, width);
	wresize(ifc->msg_win, H_MSG_BOX, width);
	mvwin(ifc->msg_win, height - H_MSG_BOX, 0);

	/* NULL := display old status */
	interface_set_status(ifc, NULL); 
	interface_redraw(ifc);

	return TRUE;
}

#ifdef HAVE_SIGNALFD
static gboolean
resize_gio(GIOChannel *source, GIOCondition condition, gpointer data)
{
	struct interface *ifc = data;
	struct signalfd_siginfo fdsi;
	ssize_t s;

	g_assert(condition & G_IO_IN);

	s = read(ifc->signal_fd, &fdsi, sizeof fdsi);
	if (s != sizeof fdsi || fdsi.ssi_signo != SIGWINCH)
		return FALSE;

	return interface_resize(ifc);
}
#endif

int
interface_init(struct interface *ifc)
{
	GIOChannel *input_channel;

	ifc->current_ctl = NULL;

	initscr();
	clear();

	noecho();
	/* Line buffering disabled. pass on everything */
	cbreak();
	/* hide cursor */
	curs_set(0);

	/* 0,0,0,0 := fullscreen */
	ifc->menu_win = newwin(0, 0, 0, 0);
	ifc->msg_win  = newwin(0, 0, 0, 0);

	/* multichar keys are mapped to one char */
	keypad(ifc->menu_win, TRUE);

	/* "resizing" here is for initial box positioning and layout */ 
	interface_resize(ifc);

#ifdef HAVE_SIGNALFD
	{
		GIOChannel *channel;
		sigset_t mask;

		sigemptyset(&mask);
		sigaddset(&mask, SIGWINCH);

		if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
			return -1;
		ifc->signal_fd = signalfd(-1, &mask, 0);
		channel = g_io_channel_unix_new(ifc->signal_fd);
		ifc->resize_source_id =
			g_io_add_watch(channel, G_IO_IN, resize_gio, ifc);
		g_io_channel_unref(channel);
	}
#else
	/* register event handler for resize and input */
	ifc->resize_source_id =	unix_signal_add(SIGWINCH,
						interface_resize, ifc);
#endif
	input_channel = g_io_channel_unix_new(STDIN_FILENO);
	if (!input_channel)
		return -1;
	ifc->input_source_id = g_io_add_watch(input_channel, G_IO_IN,
					      interface_get_input, ifc);
	g_io_channel_unref(input_channel);

	refresh();

	return 0;
}

void
interface_clear(struct interface *ifc)
{
	g_source_remove(ifc->resize_source_id);
	g_source_remove(ifc->input_source_id);
#ifdef HAVE_SIGNALFD
	close(ifc->signal_fd);
#endif
	clear();
	refresh();
	delwin(ifc->menu_win);
	delwin(ifc->msg_win);
	endwin();
	delscreen(NULL);
	if (ifc->status)
		g_free(ifc->status);
}
