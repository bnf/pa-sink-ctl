#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <glib.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>

#include "interface.h"
#include "sink.h"
#include "pa-sink-ctl.h"

#define VOLUME_BAR_LEN 50
#define H_MSG_BOX 3

// ncurses
WINDOW *menu_win;
WINDOW *msg_win;
int height;
int width;
int chooser_sink;
int chooser_input;
int selected_index;

extern GArray *sink_list;
extern pa_context* context;

void resize(int signal);

bool resize_running = false;
bool resize_pending = false;

static void set_resize_callback(void)
{
	signal(SIGWINCH, resize);
}

void resize(int signal)
{
	set_resize_callback();

	if (resize_running) {
		resize_pending = true;
		return;
	}

	resize_running = true;
	do {
		resize_pending = false;
		interface_resize();
	} while (resize_pending);
	resize_running = false;
}

void interface_init(void)
{
	chooser_sink = 0;
	chooser_input = -1;
	
	initscr();
	clear();
	noecho();
	cbreak();	/* Line buffering disabled. pass on everything */
	
	// 0,0,0,0 means fullscreen
	menu_win = newwin(0, 0, 0, 0);
	msg_win  = newwin(0, 0, 0, 0);
	nodelay(menu_win, TRUE); /* important! make wgetch non-blocking */
	keypad(menu_win, TRUE);
	curs_set(0); /* hide cursor */
	mvwprintw(msg_win, 0, 0, "Use arrow keys to go up and down, Press enter to select a choice");
	// resize the windows into the correct form
//	interface_resize();
	set_resize_callback();
	refresh();
}

void interface_resize(void)
{
	struct winsize wsize = (struct winsize) { 0 };
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsize) >= 0) {
		height = wsize.ws_row;
		width  = wsize.ws_col;
	}

	resize_term(height, width);
	clear();
	refresh();

	wresize(menu_win, height - H_MSG_BOX, width);
	wresize(msg_win, H_MSG_BOX, width);
	wmove(msg_win, height - H_MSG_BOX, 0);

	print_sink_list();
}

void print_sink_list(void)
{
	int i = 0;
	int x = 2;
	int y = 2;
	int offset = 0;
		
	werase(menu_win);
	werase(msg_win);
	box(menu_win, 0, 0);
	box(msg_win, 0, 0);
	mvwprintw(msg_win, 0, 0, "Test!");

	/* derive chooser_input from selected_index (this is set when input is moved) */
	if (chooser_input == -2) {
		chooser_input = -1; /* if index is going to be not found, select the sink itself */
		/* step through inputs for current sink and find the selected */
		for (int i = 0; i < sink_list_get(chooser_sink)->input_list->len; ++i) {
			if (selected_index == sink_input_get(chooser_sink, i)->index) {
				chooser_input = i;
				break;
			}
		}
	}
	
	for (i = 0; i < sink_list->len; ++i) {
		
		if (i == chooser_sink && chooser_input == -1)
			wattron(menu_win, A_REVERSE);

		mvwprintw(menu_win, y+i+offset, x, "%2d %-13s",
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

void print_input_list(int sink_num)
{
	int offset = sink_num + 1 + 2;
	for (int i = 0; i < sink_num; ++i)
		offset += sink_list_get(i)->input_list->len;

	for (int i = 0; i < sink_list_get(sink_num)->input_list->len; ++i) {
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
	int x = 2 /* left */  + 2 /* index num width */ + 1 /* space */ +
		1 /* space */ + 13 /* input name*/ + 1 /* space */;

	unsigned int vol = (unsigned int) ( (((double)volume) / ((double)VOLUME_MAX)) * VOLUME_BAR_LEN );
	mvwprintw(menu_win, y, x - 1, "[%c]", mute ? 'M' : ' ');
	x += 3;
	mvwprintw(menu_win, y, x - 1 , "[");
	for (int i = 0; i < vol; ++i)
		mvwprintw(menu_win, y, x + i, "=");
	for (int i = vol; i < VOLUME_BAR_LEN; ++i)
		mvwprintw(menu_win, y, x + i, " ");
	
	mvwprintw(menu_win, y, x + VOLUME_BAR_LEN, "]");
}

void get_input(void)
{
	int c;
	int volume_mult = 0;

	c = wgetch(menu_win);
	switch (c) {
		case ERR:
			/* nothing typed in */
			return;

		case 'k':
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
		case KEY_LEFT:
			volume_mult = -1;
			/* fall through */
		case 'l':
		case KEY_RIGHT:
			if (volume_mult == 0)
				volume_mult = 1;

			struct tmp_t {
				int index;
				pa_cvolume volume;
				pa_volume_t tmp_vol;
				pa_operation* (*volume_set) (pa_context*, uint32_t, const pa_cvolume*, pa_context_success_cb_t, void*);
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

			pa_cvolume_set(&tmp.volume, tmp.volume.channels,
				CLAMP(tmp.tmp_vol + 2 * volume_mult * (VOLUME_MAX / 100),
					VOLUME_MIN, VOLUME_MAX) /* force vol in [0, VOL_MAX] */
			);

			pa_operation_unref(tmp.volume_set(context, tmp.index, &tmp.volume, change_callback, NULL));
			break;
		case 'm':
		case 'M': {
			struct tmp_t {
				int index, mute;
				pa_operation* (*mute_set) (pa_context*, uint32_t, int, pa_context_success_cb_t, void*);
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
		case ' ':
			if (chooser_input == -1)
				break;
			selected_index = sink_input_get(chooser_sink, chooser_input)->index;
			if (chooser_sink < (gint)sink_list->len - 1)
				chooser_sink++;
			else
				chooser_sink = 0;

			chooser_input = -2; /* chooser_input needs to be derived from $selected_index */
			pa_operation_unref(pa_context_move_sink_input_by_index(context, selected_index,
						sink_list_get(chooser_sink)->index,
						change_callback, NULL));
			break;

		case 'q':
		default:
			printf("key: %d\n", c);
			quit();
			break;
	}
}

void interface_clear(void)
{
	clrtoeol();
	refresh();
	endwin();
	exit(0);
}
