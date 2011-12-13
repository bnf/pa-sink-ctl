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
	print_sink_list(ctx);
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
	GList *l;
	gint offset = *poffset;
	gint i;

	for (l = ctx->input_list, i = -1; l; l = l->next) {
		struct sink_input_info *input = l->data;
		if (input->sink != sink->index)
			continue;
		gboolean selected = (ctx->chooser_sink == sink_num &&
				     ctx->chooser_input == ++i);

		if (selected)
			wattron(ctx->menu_win, A_REVERSE);

		mvwprintw(ctx->menu_win, offset, 2, "%*s%-*s",
			  2+1+1, "", /* space for index number + indentation*/
			  ctx->max_name_len - 1, input->name);

		if (selected)
			wattroff(ctx->menu_win, A_REVERSE);

		print_volume(ctx, input->vol, input->mute, offset);
		offset++;
	}
	*poffset = offset;
}

/* looking for the longest name length of all SINK's and INPUT's */
static void
set_max_name_len(struct context *ctx)
{
	GList *l;
	guint len = 0;
	ctx->max_name_len = len;

	for (l = ctx->sink_list; l; l = l->next) {
		struct sink_info *sink = l->data;

		len = strlen(sink->name);

		if (len > ctx->max_name_len)
			ctx->max_name_len = len;
	}

	for (l = ctx->input_list; l; l = l->next) {
		struct sink_input_info *input = l->data;

		len = strlen(input->name) + 1 /* indentation */;

		if (len > ctx->max_name_len)
			ctx->max_name_len = len;
	}
}

void
print_sink_list(struct context *ctx)
{
	gint i = 0;
	gint x = 2;
	gint offset = 2; /* top border + 1 empty line */
	GList *l;

	/* looking for the longest name for right indentation */
	set_max_name_len(ctx);

	werase(ctx->menu_win);
	box(ctx->menu_win, 0, 0);

	for (l = ctx->sink_list, i = 0; l; l = l->next,++i) {
		struct sink_info *sink = l->data;
		gboolean selected = (i == ctx->chooser_sink &&
				     ctx->chooser_input == SELECTED_SINK);

		if (selected)
			wattron(ctx->menu_win, A_REVERSE);

		mvwprintw(ctx->menu_win, offset, x, "%2u %-*s",
			  sink->index, ctx->max_name_len, sink->name);

		if (selected)
			wattroff(ctx->menu_win, A_REVERSE);
		print_volume(ctx, sink->vol, sink->mute, offset);

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

	if (!ctx->context_ready)
		return TRUE;

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
	close(ctx->signal_fd);
	clear();
	refresh();
	endwin();
}

void
interface_set_status(struct context *ctx, const gchar *msg)
{
	if (msg != NULL) {
		g_free(ctx->status);
		ctx->status = g_strdup(msg);
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

void
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
			exit(EXIT_FAILURE);
		ctx->signal_fd = signalfd(-1, &mask, 0);
		channel = g_io_channel_unix_new(ctx->signal_fd);
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
		exit(EXIT_FAILURE);
	ctx->input_source_id = g_io_add_watch(input_channel, G_IO_IN,
					      interface_get_input, ctx);
	g_io_channel_unref(input_channel);

	refresh();
}
