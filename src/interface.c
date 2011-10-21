#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#include <glib.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>

#include "interface.h"
#include "sink.h"
#include "pa-sink-ctl.h"

#ifdef HAVE_SIGNALFD
#include <sys/signalfd.h>
#include <signal.h>
#else
#include "unix_signal.h"
#endif

#define H_MSG_BOX 3

#define SELECTED_UNKNOWN -2
#define SELECTED_SINK -1

static int
sink_input_len(struct context *ctx, sink_info *sink)
{
	int len = 0;
	GList *l;

	for (l = ctx->input_list; l; l = l->next) {
		sink_input_info *input = l->data;

		if (input->sink == sink->index)
			len++;
	}

	return len;
}

static sink_input_info *
sink_get_nth_input(struct context *ctx, sink_info *sink, int n)
{
	GList *l;
	int i;

	for (l = ctx->input_list; l; l = l->next) {
		sink_input_info *input = l->data;
		if (input->sink != sink->index)
			continue;
		if (i++ == n)
			return input;
	}

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
	print_sink_list(ctx);
	return TRUE;
}

static void
print_volume(struct context *ctx, pa_volume_t volume, int mute, int y)
{
	gint x = 2 /* left */  + 2 /* index num width */ + 1 /* space */ +
		1 /* space */ + ctx->max_name_len + 1 /* space */;

	//gint vol = (gint) (VOLUME_BAR_LEN * volume / PA_VOLUME_NORM);
	int volume_bar_len = getmaxx(ctx->menu_win) - x - 6 /* mute button + brackets + space */;
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
print_input_list(struct context *ctx, sink_info *sink, gint sink_num)
{
	GList *l;
	gint offset = sink_num + 3 /* win border + empty line + 1th sink */;
	gint i;

	for (l = ctx->sink_list, i = 0; l && i < sink_num; l = l->next,i++)
		offset += sink_input_len(ctx, l->data);

	for (l = ctx->input_list, i = 0; l; l = l->next,++i) {
		sink_input_info *input = l->data;
		if (input->sink != sink->index)
			continue;
		gboolean selected = (ctx->chooser_sink == sink_num && ctx->chooser_input == i);

		if (selected)
			wattron(ctx->menu_win, A_REVERSE);

		mvwprintw(ctx->menu_win, offset + i, 2, "%*s%-*s",
			  2+1+1, "", /* space for index number + indentation*/
			  ctx->max_name_len - 1, input->name);

		if (selected)
			wattroff(ctx->menu_win, A_REVERSE);

		print_volume(ctx, input->vol, input->mute, offset + i);
	}
}

/* looking for the longest name length of all SINK's and INPUT's */
static void
set_max_name_len(struct context *ctx)
{
	GList *l;
	guint len = 0;
	ctx->max_name_len = len;

	for (l = ctx->sink_list; l; l = l->next) {
		sink_info *sink = l->data;

		len = strlen(sink->device != NULL ? sink->device : sink->name);

		if (len > ctx->max_name_len)
			ctx->max_name_len = len;
	}

	for (l = ctx->input_list; l; l = l->next) {
		sink_input_info *input = l->data;

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
	gint y = 2;
	gint offset = 0;
	GList *l;

	/* looking for the longest name for right indentation */
	set_max_name_len(ctx);

	werase(ctx->menu_win);
	box(ctx->menu_win, 0, 0);

	/* derive ctx->chooser_input from ctx->selected_index (this is set when input is moved) */
	if (ctx->chooser_input == SELECTED_UNKNOWN) {
		/* if index is will not be found (in the loop), select the sink itself */
		ctx->chooser_input = SELECTED_SINK; 
		/* step through inputs for current sink and find the selected */
		sink_info *sink = g_list_nth_data(ctx->sink_list, ctx->chooser_sink);
		for (l = ctx->input_list, i = 0; l; l = l->next,++i) {
			sink_input_info *input = l->data;
			if (input->sink != sink->index)
				continue;
			if (ctx->selected_index == input->index) {
				ctx->chooser_input = i;
				break;
			}
		}
	}

	for (l = ctx->sink_list, i = 0; l; l = l->next,++i) {
		sink_info *sink = l->data;
		gboolean selected = (i == ctx->chooser_sink && ctx->chooser_input == SELECTED_SINK);

		if (selected)
			wattron(ctx->menu_win, A_REVERSE);

		mvwprintw(ctx->menu_win, y+i+offset, x, "%2u %-*s",
			  sink->index, ctx->max_name_len,
			  sink->device != NULL ? sink->device : sink->name);

		if (selected)
			wattroff(ctx->menu_win, A_REVERSE);
		print_volume(ctx, sink->vol, sink->mute, y+i+offset);

		print_input_list(ctx, sink, i);

		offset += sink_input_len(ctx, sink);
	}
	wrefresh(ctx->menu_win);
}

static gboolean
interface_get_input(GIOChannel *source, GIOCondition condition, gpointer data)
{
	struct context *ctx = data;
	gint c;
	gboolean volume_increment = TRUE;
	sink_info *sink = NULL;
	guint32 index;

	if (!ctx->context_ready)
		return TRUE;

	c = wgetch(ctx->menu_win);
	switch (c) {
	case 'k':
	case 'w':
	case KEY_UP:
		if (ctx->chooser_input == SELECTED_SINK && ctx->chooser_sink > 0) {
			sink = g_list_nth_data(ctx->sink_list, --ctx->chooser_sink);
			/* automatic SELECTED_SINK (=-1) assignment if length = 0 */
			ctx->chooser_input = sink_input_len(ctx, sink) - 1;
		}

		else if (ctx->chooser_input >= 0)
			--ctx->chooser_input;
		print_sink_list(ctx);
		break;

	case 'j':
	case 's':
	case KEY_DOWN:
		sink = g_list_nth_data(ctx->sink_list, ctx->chooser_sink);
		if (ctx->chooser_input == (sink_input_len(ctx, sink) - 1) && ctx->chooser_sink < (gint)g_list_length(ctx->sink_list) - 1) {
			++ctx->chooser_sink;
			ctx->chooser_input = SELECTED_SINK;
		}
		else if (ctx->chooser_input < (sink_input_len(ctx, sink) - 1))
			++ctx->chooser_input;
		print_sink_list(ctx);
		break;

	case 'h':
	case 'a':
	case KEY_LEFT:
		volume_increment = FALSE;
		/* fall through */
	case 'l':
	case 'd':
	case KEY_RIGHT:
		sink = g_list_nth_data(ctx->sink_list, ctx->chooser_sink);
		pa_cvolume volume;
		pa_volume_t tmp_vol;
		pa_operation* (*volume_set) (pa_context*, guint32, const pa_cvolume*, pa_context_success_cb_t, gpointer);

		if (ctx->chooser_input >= 0) {
			sink_input_info *input = sink_get_nth_input(ctx, sink, ctx->chooser_input);
			index      = input->index;
			volume     = (pa_cvolume) {.channels = input->channels};
			tmp_vol    = input->vol; 
			volume_set = pa_context_set_sink_input_volume;
		} else if (ctx->chooser_input == SELECTED_SINK) {
			index      = sink->index;
			volume     = (pa_cvolume) {.channels = sink->channels};
			tmp_vol    = sink->vol;
			volume_set = pa_context_set_sink_volume_by_index;
		} else
			break;

		pa_cvolume_set(&volume, volume.channels, tmp_vol);
		pa_volume_t inc = 2 * PA_VOLUME_NORM / 100;

		if (volume_increment)
			if (PA_VOLUME_NORM > tmp_vol && PA_VOLUME_NORM - tmp_vol > inc)
				pa_cvolume_inc(&volume, inc);
			else
				pa_cvolume_set(&volume, volume.channels, PA_VOLUME_NORM);
		else
			pa_cvolume_dec(&volume, inc);


		pa_operation_unref(volume_set(ctx->context, index, &volume, change_callback, ctx));
		break;
	case 'm':
	case 'x':
	case 'M':
		sink = g_list_nth_data(ctx->sink_list, ctx->chooser_sink);
		gint mute;
		pa_operation* (*mute_set) (pa_context*, guint32, int, pa_context_success_cb_t, void*);

		if (ctx->chooser_input >= 0) {
			sink_input_info *input = sink_get_nth_input(ctx, sink, ctx->chooser_input);
			index    = input->index;
			mute     = !input->mute;
			mute_set = pa_context_set_sink_input_mute;
		} else if (ctx->chooser_input == SELECTED_SINK) {
			index    = sink->index;
			mute     = !sink->mute;
			mute_set = pa_context_set_sink_mute_by_index;
		} else
			break;

		pa_operation_unref(mute_set(ctx->context, index, mute, change_callback, ctx));
		break;

	case '\n':
	case '\t':
	case ' ':
		if (ctx->chooser_input == SELECTED_SINK)
			break;
		sink = g_list_nth_data(ctx->sink_list, ctx->chooser_sink);
		sink_input_info *input = sink_get_nth_input(ctx, sink, ctx->chooser_input);
		ctx->selected_index = input->index;
		if (ctx->chooser_sink < (gint)g_list_length(ctx->sink_list) - 1)
			ctx->chooser_sink++;
		else
			ctx->chooser_sink = 0;

		sink = g_list_nth_data(ctx->sink_list, ctx->chooser_sink);
		/* ctx->chooser_input needs to be derived from $ctx->selected_index */
		ctx->chooser_input = SELECTED_UNKNOWN; 
		pa_operation_unref(pa_context_move_sink_input_by_index(ctx->context, ctx->selected_index,
								       sink->index,
								       change_callback, NULL));
		break;

	case 'q':
	default:
		quit(ctx);
		break;
	}

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

	ctx->chooser_sink  = 0;		/* Selected sink-device. 0 is the first device */
	ctx->chooser_input = SELECTED_SINK;	/* Selected input of the current sink-device.  */
	/* SELECTED_SINK refers to sink-device itself  */
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
	ctx->resize_source_id = unix_signal_add(SIGWINCH, interface_resize, ctx);
#endif
	input_channel = g_io_channel_unix_new(STDIN_FILENO);
	if (!input_channel)
		exit(EXIT_FAILURE);
	ctx->input_source_id = g_io_add_watch(input_channel, G_IO_IN,
					      interface_get_input, ctx);
	g_io_channel_unref(input_channel);

	refresh();
}
