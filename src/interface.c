#include <unistd.h>
#include <sys/ioctl.h>

#include <glib.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>

#include "interface.h"
#include "sink.h"
#include "pa-sink-ctl.h"

#include "g_unix_signal.h"
#include "g_curses_input.h"

#define VOLUME_BAR_LEN 50
#define H_MSG_BOX 3

extern pa_context* context;

static WINDOW *menu_win;
static WINDOW *msg_win;

static guint resize_source_id;
static guint input_source_id;

static gint chooser_sink;
static gint chooser_input;
static guint32 selected_index;

void interface_init(void)
{
	chooser_sink  =  0;	/* Selected sink-device. 0 is the first device 	*/
	chooser_input = -1;	/* Selected input of the current sink-device.	*/
				/* -1 means the sink-device itself		*/
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

	/* register event handler for resize and input */
	resize_source_id = g_unix_signal_add(SIGWINCH, interface_resize, NULL);
	input_source_id  = g_curses_input_add(menu_win, get_input, NULL);
	
	refresh();
}

gboolean interface_resize(gpointer data)
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
	status(NULL); 
	print_sink_list();
	return TRUE;
}

void print_sink_list(void)
{
	gint i = 0;
	gint x = 2;
	gint y = 2;
	gint offset = 0;
		
	werase(menu_win);
	box(menu_win, 0, 0);

	/* derive chooser_input from selected_index (this is set when input is moved) */
	if (chooser_input == -2) {
		/* if index is will not be found (in the loop), select the sink itself */
		chooser_input = -1; 
		/* step through inputs for current sink and find the selected */
		for (i = 0; i < sink_list_get(chooser_sink)->input_list->len; ++i) {
			if (selected_index == sink_input_get(chooser_sink, i)->index) {
				chooser_input = i;
				break;
			}
		}
	}
	
	for (i = 0; i < sink_list->len; ++i) {
		if (i == chooser_sink && chooser_input == -1)
			wattron(menu_win, A_REVERSE);

		mvwprintw(menu_win, y+i+offset, x, "%2u %-13s",
			sink_list_get(i)->index,
			sink_list_get(i)->device != NULL ? sink_list_get(i)->device : sink_list_get(i)->name);
		
		if (i == chooser_sink && chooser_input == -1)
			wattroff(menu_win, A_REVERSE);
		print_volume(sink_list_get(i)->vol, sink_list_get(i)->mute, y+i+offset);

		print_input_list(i);

		offset += sink_list_get(i)->input_list->len;
	}
	wrefresh(menu_win);
}

void print_input_list(gint sink_num)
{
	gint offset = sink_num + 1 + 2;

	for (gint i = 0; i < sink_num; ++i)
		offset += sink_list_get(i)->input_list->len;

	for (gint i = 0; i < sink_list_get(sink_num)->input_list->len; ++i) {
		if (chooser_sink == sink_num && chooser_input == i)
			wattron(menu_win, A_REVERSE);
		mvwprintw(menu_win, offset + i, 2, "%*s%-*s", 2+1+1, "", 13 - 1, sink_input_get(sink_num, i)->name);
		if (chooser_sink == sink_num && chooser_input == i)
			wattroff(menu_win, A_REVERSE);

		print_volume(sink_input_get(sink_num, i)->vol, sink_input_get(sink_num, i)->mute, offset + i);
	}
		
}

void print_volume(pa_volume_t volume, int mute, int y)
{
	gint x = 2 /* left */  + 2 /* index num width */ + 1 /* space */ +
		1 /* space */ + 13 /* input name*/ + 1 /* space */;

	gint vol = (gint) (VOLUME_BAR_LEN * volume / PA_VOLUME_NORM);

	mvwprintw(menu_win, y, x - 1, "[%c]", mute ? 'M' : ' ');
	x += 3;

	mvwprintw(menu_win, y, x - 1 , "[");
	for (gint i = 0; i < vol; ++i)
		mvwprintw(menu_win, y, x + i, "=");
	for (gint i = vol; i < VOLUME_BAR_LEN; ++i)
		mvwprintw(menu_win, y, x + i, " ");
	mvwprintw(menu_win, y, x + VOLUME_BAR_LEN, "]");
}

gboolean get_input(gpointer data)
{
	gint c;
	gboolean volume_increment = TRUE;

	c = wgetch(menu_win);
	switch (c) {
		case 'k':
		case 'w':
		case KEY_UP:
			if (chooser_input == -1 && chooser_sink > 0) {
				--chooser_sink;
				chooser_input = (gint)sink_list_get(chooser_sink)->input_list->len - 1;
			}

			else if (chooser_input >= 0)
				--chooser_input;
			print_sink_list();
			break;

		case 'j':
		case 's':
		case KEY_DOWN:
			if (chooser_input == ((gint)sink_list_get(chooser_sink)->input_list->len - 1) && chooser_sink < (gint)sink_list->len - 1) {
					++chooser_sink;
					chooser_input = -1;
			}
			else if (chooser_input < ((gint)sink_list_get(chooser_sink)->input_list->len - 1))
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

			if (chooser_input >= 0) {
				sink_input_info *input = sink_input_get(chooser_sink, chooser_input);
				tmp = (struct tmp_t) {
					.index      = input->index,
					.volume     = (pa_cvolume) {.channels = input->channels},
					.tmp_vol    = input->vol, 
					.volume_set = pa_context_set_sink_input_volume
				};
			} else if (chooser_input == -1) {
				sink_info *sink = sink_list_get(chooser_sink);
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

			if (chooser_input >= 0) {
				sink_input_info *input = sink_input_get(chooser_sink, chooser_input);
				tmp = (struct tmp_t) {
					.index    = input->index,
					.mute     = input->mute,
					.mute_set = pa_context_set_sink_input_mute
				};
			} else if (chooser_input == -1) {
				sink_info *sink = sink_list_get(chooser_sink);
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
			if (chooser_input == -1)
				break;
			selected_index = sink_input_get(chooser_sink, chooser_input)->index;
			if (chooser_sink < (gint)sink_list->len - 1)
				chooser_sink++;
			else
				chooser_sink = 0;

			/* chooser_input needs to be derived from $selected_index */
			chooser_input = -2; 
			pa_operation_unref(pa_context_move_sink_input_by_index(context, selected_index,
						sink_list_get(chooser_sink)->index,
						change_callback, NULL));
			break;

		case 'q':
		default:
			quit();
			break;
	}
	return TRUE;
}

void interface_clear(void)
{
	g_source_remove(resize_source_id);
	g_source_remove(input_source_id);
	clear();
	refresh();
	endwin();
}

void status(const gchar *msg)
{
	static gchar *save = NULL;
	if (msg != NULL) {
		g_free(save);
		save = g_strdup(msg);
	}
	werase(msg_win);
	box(msg_win, 0, 0);
	if (save != NULL)
		mvwprintw(msg_win, 1, 1, save);
	wrefresh(msg_win);
	refresh();
}
