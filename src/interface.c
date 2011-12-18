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
#include "sink.h"
#include "command.h"
#include "pa-sink-ctl.h"

#ifdef HAVE_SIGNALFD
#include <sys/signalfd.h>
#include <signal.h>
#else
#include "unix_signal.h"
#endif

static struct sink_input_info *
sink_get_nth_input(struct context *ctx, struct sink_info *sink, int n)
{
	struct sink_input_info *input;
	int i = 0;

	list_foreach(ctx->input_list, input) {
		if (input->sink != sink->base.index)
			continue;
		if (i++ == n)
			return input;
	}

	return NULL;
}

struct vol_ctl *
interface_get_current_ctl(struct context *ctx, struct vol_ctl **parent)
{
	struct sink_info *sink;
	struct sink_input_info *input;

	if (parent)
		*parent = NULL;

	sink = g_list_nth_data(ctx->sink_list, ctx->chooser_sink);
	if (sink == NULL)
		return NULL;

	if (ctx->chooser_input == SELECTED_SINK)
		return &sink->base;
	else if (ctx->chooser_input >= 0) {
		input = sink_get_nth_input(ctx, sink, ctx->chooser_input);
		if (input == NULL)
			return NULL;
		if (parent)
			*parent = &sink->base;
		return &input->base;
	}

	g_assert(0);
	return NULL;
}

static gboolean
interface_resize(gpointer data)
{
	struct context *ctx = data;
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

	wresize(ctx->menu_win, height - H_MSG_BOX, width);
	wresize(ctx->msg_win, H_MSG_BOX, width);
	mvwin(ctx->msg_win, height - H_MSG_BOX, 0);

	/* NULL := display old status */
	interface_set_status(ctx, NULL); 
	interface_redraw(ctx);

	return TRUE;
}

static void
print_volume(struct context *ctx, pa_volume_t volume, int mute, int y)
{
	gint x = 2 /* left */  + 2 /* index num width */ + 1 /* space */ +
		1 /* space */ + ctx->max_name_len + 1 /* space */;

	/* mute button + brackets + space */
	int volume_bar_len = getmaxx(ctx->menu_win) - x - 6;
	gint vol = (gint) (volume_bar_len * volume / PA_VOLUME_NORM);

	mvwprintw(ctx->menu_win, y, x - 1, "[%c]", mute ? 'M' : ' ');
	x += 3;

	mvwprintw(ctx->menu_win, y, x - 1 , "[");
	for (gint i = 0; i < vol; ++i)
		mvwprintw(ctx->menu_win, y, x + i, "=");
	for (gint i = vol; i < volume_bar_len; ++i)
		mvwprintw(ctx->menu_win, y, x + i, " ");
	mvwprintw(ctx->menu_win, y, x + volume_bar_len, "]");
}

static void
print_input_list(struct context *ctx, struct sink_info *sink,
		 gint sink_num, gint *poffset)
{
	struct sink_input_info *input;
	gint offset = *poffset;
	gboolean selected;
	gint i = -1;

	list_foreach(ctx->input_list, input) {
		if (input->sink != sink->base.index)
			continue;
		selected = (ctx->chooser_sink == sink_num &&
			    ctx->chooser_input == ++i);

		if (selected)
			wattron(ctx->menu_win, A_REVERSE);

		mvwprintw(ctx->menu_win, offset, 2, "%*s%-*s",
			  2+1+1, "", /* space for index number + indentation*/
			  ctx->max_name_len - 1, input->base.name);

		if (selected)
			wattroff(ctx->menu_win, A_REVERSE);

		print_volume(ctx, input->base.vol, input->base.mute, offset);
		offset++;
	}
	*poffset = offset;
}

/* looking for the longest name length of all SINK's and INPUT's */
static void
set_max_name_len(struct context *ctx)
{
	struct vol_ctl *ctl;
	guint len = 0;
	ctx->max_name_len = len;

	list_foreach(ctx->sink_list, ctl) {
		len = strlen(ctl->name);

		if (len > ctx->max_name_len)
			ctx->max_name_len = len;
	}

	list_foreach(ctx->input_list, ctl) {
		len = strlen(ctl->name) + 1 /* indentation */;

		if (len > ctx->max_name_len)
			ctx->max_name_len = len;
	}
}

void
interface_redraw(struct context *ctx)
{
	struct sink_info *sink;
	gint i = -1;
	gint x = 2;
	gint offset = 2; /* top border + 1 empty line */

	/* looking for the longest name for right indentation */
	set_max_name_len(ctx);

	werase(ctx->menu_win);
	box(ctx->menu_win, 0, 0);

	list_foreach(ctx->sink_list, sink) {
		gboolean selected = (++i == ctx->chooser_sink &&
				     ctx->chooser_input == SELECTED_SINK);

		if (selected)
			wattron(ctx->menu_win, A_REVERSE);

		mvwprintw(ctx->menu_win, offset, x, "%2u %-*s",
			  sink->base.index, ctx->max_name_len, sink->base.name);

		if (selected)
			wattroff(ctx->menu_win, A_REVERSE);
		print_volume(ctx, sink->base.vol, sink->base.mute, offset);

		offset++;
		print_input_list(ctx, sink, i, &offset);
	}
	wrefresh(ctx->menu_win);
}

static gboolean
interface_get_input(GIOChannel *source, GIOCondition condition, gpointer data)
{
	struct context *ctx = data;
	struct command_cb_descriptor *cmd;
	gint c;

	c = wgetch(ctx->menu_win);

	cmd = g_hash_table_lookup(ctx->config.keymap, GINT_TO_POINTER(c));
	if (cmd)
		cmd->cb(ctx, c);

	return TRUE;
}

void
interface_clear(struct context *ctx)
{
	g_source_remove(ctx->resize_source_id);
	g_source_remove(ctx->input_source_id);
#ifdef HAVE_SIGNALFD
	close(ctx->signal_fd);
#endif
	clear();
	refresh();
	delwin(ctx->menu_win);
	delwin(ctx->msg_win);
	endwin();
	delscreen(NULL);
	if (ctx->status)
		g_free(ctx->status);
}

void
interface_set_status(struct context *ctx, const gchar *msg, ...)
{
	va_list ap;

	if (msg != NULL) {
		g_free(ctx->status);
		va_start(ap, msg);
		ctx->status = g_strdup_vprintf(msg, ap);
		va_end(ap);
	}
	werase(ctx->msg_win);
	box(ctx->msg_win, 0, 0);
	if (ctx->status != NULL)
		mvwprintw(ctx->msg_win, 1, 1, ctx->status);
	wrefresh(ctx->msg_win);
	refresh();
}

#ifdef HAVE_SIGNALFD
static gboolean
resize_gio(GIOChannel *source, GIOCondition condition, gpointer data)
{
	struct context *ctx = data;
	struct signalfd_siginfo fdsi;
	ssize_t s;

	g_assert(condition & G_IO_IN);

	s = read(ctx->signal_fd, &fdsi, sizeof fdsi);
	if (s != sizeof fdsi || fdsi.ssi_signo != SIGWINCH)
		return FALSE;

	return interface_resize(ctx);
}
#endif

int
interface_init(struct context *ctx)
{
	GIOChannel *input_channel;

	/* Selected sink-device. 0 is the first device */
	ctx->chooser_sink  = 0;	
	/* Selected input of the current sink-device.  */
	/* SELECTED_SINK refers to sink-device itself  */
	ctx->chooser_input = SELECTED_SINK;
	initscr();
	clear();

	noecho();
	/* Line buffering disabled. pass on everything */
	cbreak();
	/* hide cursor */
	curs_set(0);

	/* 0,0,0,0 := fullscreen */
	ctx->menu_win = newwin(0, 0, 0, 0);
	ctx->msg_win  = newwin(0, 0, 0, 0);

	/* multichar keys are mapped to one char */
	keypad(ctx->menu_win, TRUE);

	/* "resizing" here is for initial box positioning and layout */ 
	interface_resize(ctx);

#ifdef HAVE_SIGNALFD
	{
		GIOChannel *channel;
		sigset_t mask;

		sigemptyset(&mask);
		sigaddset(&mask, SIGWINCH);

		if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
			return -1;
		ctx->signal_fd = signalfd(-1, &mask, 0);
		channel = g_io_channel_unix_new(ctx->signal_fd);
		ctx->resize_source_id =
			g_io_add_watch(channel, G_IO_IN, resize_gio, ctx);
		g_io_channel_unref(channel);
	}
#else
	/* register event handler for resize and input */
	ctx->resize_source_id =	unix_signal_add(SIGWINCH,
						interface_resize, ctx);
#endif
	input_channel = g_io_channel_unix_new(STDIN_FILENO);
	if (!input_channel)
		return -1;
	ctx->input_source_id = g_io_add_watch(input_channel, G_IO_IN,
					      interface_get_input, ctx);
	g_io_channel_unref(input_channel);

	refresh();

	return 0;
}
