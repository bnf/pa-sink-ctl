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

extern pa_context* context;
extern gboolean context_ready;

static WINDOW *menu_win;
static WINDOW *msg_win;

static guint resize_source_id;
#ifdef HAVE_SIGNALFD
static int signal_fd;
#endif
static guint input_source_id;

static gint chooser_sink;
static gint chooser_input;
static guint32 selected_index;

guint max_name_len = 0;

extern GList *sink_list;

static gboolean
interface_resize(gpointer data)
{
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

	wresize(menu_win, height - H_MSG_BOX, width);
	wresize(msg_win, H_MSG_BOX, width);
	mvwin(msg_win, height - H_MSG_BOX, 0);
	
	/* NULL := display old status */
	interface_set_status(NULL); 
	print_sink_list();
	return TRUE;
}

static void
print_volume(pa_volume_t volume, int mute, int y)
{
	gint x = 2 /* left */  + 2 /* index num width */ + 1 /* space */ +
		1 /* space */ + max_name_len + 1 /* space */;

	//gint vol = (gint) (VOLUME_BAR_LEN * volume / PA_VOLUME_NORM);
	int volume_bar_len = getmaxx(menu_win) - x - 6 /* mute button + brackets + space */;
	gint vol = (gint) (volume_bar_len * volume / PA_VOLUME_NORM);

	mvwprintw(menu_win, y, x - 1, "[%c]", mute ? 'M' : ' ');
	x += 3;

	mvwprintw(menu_win, y, x - 1 , "[");
	for (gint i = 0; i < vol; ++i)
		mvwprintw(menu_win, y, x + i, "=");
	for (gint i = vol; i < volume_bar_len; ++i)
		mvwprintw(menu_win, y, x + i, " ");
	mvwprintw(menu_win, y, x + volume_bar_len, "]");
}

static void
print_input_list(GList *input_list, gint sink_num)
{
	GList *l;
	gint offset = sink_num + 3 /* win border + empty line + 1th sink */;
	gint i;

	for (l = sink_list, i = 0; l && i < sink_num; l = l->next,i++)
		offset += g_list_length(((sink_info *)l->data)->input_list);

	for (l = input_list, i = 0; l; l = l->next,++i) {
		sink_input_info *input = l->data;
		gboolean selected = (chooser_sink == sink_num && chooser_input == i);

		if (selected)
			wattron(menu_win, A_REVERSE);

		mvwprintw(menu_win, offset + i, 2, "%*s%-*s",
			  2+1+1, "", /* space for index number + indentation*/
			  max_name_len - 1, input->name);

		if (selected)
			wattroff(menu_win, A_REVERSE);

		print_volume(input->vol, input->mute, offset + i);
	}
}

/* looking for the longest name length of all SINK's and INPUT's */
static void
set_max_name_len(void)
{
	GList *l,*k;
	guint len = 0;
	max_name_len = len;

	for (l = sink_list; l; l = l->next) {
		sink_info *sink = l->data;

		len = strlen(sink->device != NULL ? sink->device : sink->name);

		if (len > max_name_len)
			max_name_len = len;
		
		for (k = sink->input_list; k; k = k->next) {
			sink_input_info *input = k->data;
			
			len = strlen(input->name) + 1 /* indentation */;

			if (len > max_name_len)
				max_name_len = len;
		}
	}
}

void
print_sink_list(void)
{
	gint i = 0;
	gint x = 2;
	gint y = 2;
	gint offset = 0;
	GList *l;

	/* looking for the longest name for right indentation */
	set_max_name_len();
	
	werase(menu_win);
	box(menu_win, 0, 0);

	/* derive chooser_input from selected_index (this is set when input is moved) */
	if (chooser_input == SELECTED_UNKNOWN) {
		/* if index is will not be found (in the loop), select the sink itself */
		chooser_input = SELECTED_SINK; 
		/* step through inputs for current sink and find the selected */
		sink_info *sink = g_list_nth_data(sink_list, chooser_sink);
		for (l = sink->input_list, i = 0; l; l = l->next,++i) {
			sink_input_info *input = l->data;
			if (selected_index == input->index) {
				chooser_input = i;
				break;
			}
		}
	}
	
	for (l = sink_list, i = 0; l; l = l->next,++i) {
		sink_info *sink = l->data;
		gboolean selected = (i == chooser_sink && chooser_input == SELECTED_SINK);

		if (selected)
			wattron(menu_win, A_REVERSE);

		mvwprintw(menu_win, y+i+offset, x, "%2u %-*s",
			  sink->index, max_name_len,
			  sink->device != NULL ? sink->device : sink->name);
		
		if (selected)
			wattroff(menu_win, A_REVERSE);
		print_volume(sink->vol, sink->mute, y+i+offset);

		print_input_list(sink->input_list, i);

		offset += g_list_length(sink->input_list);
	}
	wrefresh(menu_win);
}

static gboolean
interface_get_input(GIOChannel *source, GIOCondition condition, gpointer data)
{
	gint c;
	gboolean volume_increment = TRUE;
	sink_info *sink = NULL;

	if (!context_ready)
		return TRUE;

	c = wgetch(menu_win);
	switch (c) {
		case 'k':
		case 'w':
		case KEY_UP:
			if (chooser_input == SELECTED_SINK && chooser_sink > 0) {
				sink = g_list_nth_data(sink_list, --chooser_sink);
				/* automatic SELECTED_SINK (=-1) assignment if length = 0 */
				chooser_input = (gint)g_list_length(sink->input_list) - 1;
			}

			else if (chooser_input >= 0)
				--chooser_input;
			print_sink_list();
			break;

		case 'j':
		case 's':
		case KEY_DOWN:
			sink = g_list_nth_data(sink_list, chooser_sink);
			if (chooser_input == ((gint)g_list_length(sink->input_list) - 1) && chooser_sink < (gint)g_list_length(sink_list) - 1) {
					++chooser_sink;
					chooser_input = SELECTED_SINK;
			}
			else if (chooser_input < ((gint)g_list_length(sink->input_list) - 1))
				++chooser_input;
			print_sink_list();
			break;

		case 'h':
		case 'a':
		case KEY_LEFT:
			volume_increment = FALSE;
			/* fall through */
		case 'l':
		case 'd':
		case KEY_RIGHT: {
			struct tmp_t {
				guint32 index;
				pa_cvolume volume;
				pa_volume_t tmp_vol;
				pa_operation* (*volume_set) (pa_context*, guint32, const pa_cvolume*, pa_context_success_cb_t, gpointer);
			} tmp;

			sink = g_list_nth_data(sink_list, chooser_sink);
			if (chooser_input >= 0) {
				sink_input_info *input = g_list_nth_data(sink->input_list, chooser_input);
				tmp = (struct tmp_t) {
					.index      = input->index,
					.volume     = (pa_cvolume) {.channels = input->channels},
					.tmp_vol    = input->vol, 
					.volume_set = pa_context_set_sink_input_volume
				};
			} else if (chooser_input == SELECTED_SINK) {
				tmp = (struct tmp_t) {
					.index      = sink->index,
					.volume     = (pa_cvolume) {.channels = sink->channels},
					.tmp_vol    = sink->vol,
					.volume_set = pa_context_set_sink_volume_by_index
				};
			} else
				break;

			pa_cvolume_set(&tmp.volume, tmp.volume.channels, tmp.tmp_vol);
			pa_volume_t inc = 2 * PA_VOLUME_NORM / 100;

			if (volume_increment)
				if (PA_VOLUME_NORM > tmp.tmp_vol && PA_VOLUME_NORM - tmp.tmp_vol > inc)
					pa_cvolume_inc(&tmp.volume, inc);
				else
					pa_cvolume_set(&tmp.volume, tmp.volume.channels, PA_VOLUME_NORM);
			else
				pa_cvolume_dec(&tmp.volume, inc);


			pa_operation_unref(tmp.volume_set(context, tmp.index, &tmp.volume, change_callback, NULL));
			break;
		}


		case 'm':
		case 'x':
		case 'M': {
			struct tmp_t {
				guint32 index;
				gint mute;
				pa_operation* (*mute_set) (pa_context*, guint32, int, pa_context_success_cb_t, void*);
			} tmp;

			sink = g_list_nth_data(sink_list, chooser_sink);
			if (chooser_input >= 0) {
				sink_input_info *input = g_list_nth_data(sink->input_list, chooser_input);
				tmp = (struct tmp_t) {
					.index    = input->index,
					.mute     = input->mute,
					.mute_set = pa_context_set_sink_input_mute
				};
			} else if (chooser_input == SELECTED_SINK) {
				tmp = (struct tmp_t) {
					.index    = sink->index,
					.mute     = sink->mute,
					.mute_set = pa_context_set_sink_mute_by_index
				};
			} else
				break;

			pa_operation_unref(tmp.mute_set(context, tmp.index, !tmp.mute, change_callback, NULL));
			break;
		}

		case '\n':
		case '\t':
		case ' ':
			if (chooser_input == SELECTED_SINK)
				break;
			sink = g_list_nth_data(sink_list, chooser_sink);
			sink_input_info *input = g_list_nth_data(sink->input_list, chooser_input);
			selected_index = input->index;
			if (chooser_sink < (gint)g_list_length(sink_list) - 1)
				chooser_sink++;
			else
				chooser_sink = 0;

			sink = g_list_nth_data(sink_list, chooser_sink);
			/* chooser_input needs to be derived from $selected_index */
			chooser_input = SELECTED_UNKNOWN; 
			pa_operation_unref(pa_context_move_sink_input_by_index(context, selected_index,
									       sink->index,
									       change_callback, NULL));
			break;

		case 'q':
		default:
			quit();
			break;
	}

	return TRUE;
}

void
interface_clear(void)
{
	g_source_remove(resize_source_id);
	g_source_remove(input_source_id);
	close(signal_fd);
	clear();
	refresh();
	endwin();
}

void
interface_set_status(const gchar *msg)
{
	static gchar *status = NULL;

	if (msg != NULL) {
		g_free(status);
		status = g_strdup(msg);
	}
	werase(msg_win);
	box(msg_win, 0, 0);
	if (status != NULL)
		mvwprintw(msg_win, 1, 1, status);
	wrefresh(msg_win);
	refresh();
}

#ifdef HAVE_SIGNALFD
static gboolean
resize_gio(GIOChannel *source, GIOCondition condition, gpointer data)
{
	struct signalfd_siginfo fdsi;
	ssize_t s;

	g_assert(condition & G_IO_IN);

	s = read(signal_fd, &fdsi, sizeof fdsi);
	if (s != sizeof fdsi || fdsi.ssi_signo != SIGWINCH)
		return FALSE;

	return interface_resize(data);
}
#endif

void
interface_init(void)
{
	GIOChannel *input_channel;

	chooser_sink  = 0;		/* Selected sink-device. 0 is the first device */
	chooser_input = SELECTED_SINK;	/* Selected input of the current sink-device.  */
					/* SELECTED_SINK refers to sink-device itself  */
	initscr();
	clear();

	noecho();
	/* Line buffering disabled. pass on everything */
	cbreak();
	/* hide cursor */
	curs_set(0);

	/* 0,0,0,0 := fullscreen */
	menu_win = newwin(0, 0, 0, 0);
	msg_win  = newwin(0, 0, 0, 0);
	
	/* multichar keys are mapped to one char */
	keypad(menu_win, TRUE);

	/* "resizing" here is for initial box positioning and layout */ 
	interface_resize(NULL);

#ifdef HAVE_SIGNALFD
	{
		GIOChannel *channel;
		sigset_t mask;

		sigemptyset(&mask);
		sigaddset(&mask, SIGWINCH);

		if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
			exit(EXIT_FAILURE);
		signal_fd = signalfd(-1, &mask, 0);
		channel = g_io_channel_unix_new(signal_fd);
		g_io_add_watch(channel, G_IO_IN, resize_gio, NULL);
		g_io_channel_unref(channel);
	}
#else
	/* register event handler for resize and input */
	resize_source_id = unix_signal_add(SIGWINCH, interface_resize, NULL);
#endif
	input_channel = g_io_channel_unix_new(STDIN_FILENO);
	if (!input_channel)
		exit(EXIT_FAILURE);
	input_source_id = g_io_add_watch(input_channel, G_IO_IN,
					 interface_get_input, NULL);
	g_io_channel_unref(input_channel);
	
	refresh();
}
