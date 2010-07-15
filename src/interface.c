#include <stdio.h>
#include <glib.h>
#include <pulse/pulseaudio.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#include "interface.h"
#include "sink.h"
#include "pa-sink-ctl.h"

#define VOLUME_MAX UINT16_MAX
#define VOLUME_BAR_LEN 50
#define WIDTH 80
#define HEIGHT 10

// ncurses
WINDOW *menu_win;
int chooser_sink;
int chooser_input;
int startx;
int starty;

int selected_index;

extern GArray *sink_list;

extern pa_context* context;

void interface_init(void)
{
	chooser_sink = 0;
	chooser_input = -1;

	initscr();
	clear();
	noecho();
	cbreak();	/* Line buffering disabled. pass on everything */
	startx = (80 - WIDTH) / 2;
	starty = (24 - HEIGHT) / 2;
	menu_win = newwin(HEIGHT, WIDTH, starty, startx);
	keypad(menu_win, TRUE);
	curs_set(0); /* hide cursor */
	mvprintw(0, 0, "Use arrow keys to go up and down, Press enter to select a choice");
	refresh();
}

void print_sink_list(void)
{
	int i = 0;
	int x = 2;
	int y = 2;
	int offset = 0;
	
	werase(menu_win);
	box(menu_win, 0, 0);

	/* derive chooser_input from selected_index (this is set when input is moved) */
	if (chooser_input == -2) {
		chooser_input = -1; /* if index is going to be not found, select the sink itself */
		/* step through inputs for current sink and find the selected */
		for (int i = 0; i < g_array_index(sink_list, sink_info, chooser_sink).input_list->len; ++i) {
			if (selected_index == g_array_index(g_array_index(sink_list, sink_info, chooser_sink).input_list, sink_input_info, i).index) {
				chooser_input = i;
				break;
			}
		}
	}
	
	for (i = 0; i < sink_list->len; ++i) {
		
		if (i == chooser_sink && chooser_input == -1)
			wattron(menu_win, A_REVERSE);

		mvwprintw(menu_win, y+i+offset, x, "%2d %-13s",
			g_array_index(sink_list, sink_info, i).index,
			g_array_index(sink_list, sink_info, i).device != NULL ? g_array_index(sink_list, sink_info, i).device : g_array_index(sink_list, sink_info, i).name);
		
		if (i == chooser_sink && chooser_input == -1)
			wattroff(menu_win, A_REVERSE);
		print_volume(g_array_index(sink_list, sink_info, i).vol, g_array_index(sink_list, sink_info, i).mute, y+i+offset);
		
		print_input_list(i);

		offset += g_array_index(sink_list, sink_info, i).input_list->len;
	}
	wrefresh(menu_win);
}

void print_input_list(int sink_num)
{
	int offset = sink_num + 1 + 2;
	for (int i = 0; i < sink_num; ++i)
		offset += g_array_index(sink_list, sink_info, i).input_list->len;

	for (int i = 0; i < g_array_index(sink_list, sink_info, sink_num).input_list->len; ++i) {
		if (chooser_sink == sink_num && chooser_input == i)
			wattron(menu_win, A_REVERSE);

		mvwprintw(menu_win, offset + i, 2, "%*s%-*s", 2+1+1, "", 13 - 1,
			g_array_index(g_array_index(sink_list, sink_info, sink_num).input_list, sink_input_info, i).name);

		if (chooser_sink == sink_num && chooser_input == i)
			wattroff(menu_win, A_REVERSE);

		print_volume(g_array_index(g_array_index(sink_list, sink_info, sink_num).input_list, sink_input_info, i).vol, 
				g_array_index(g_array_index(sink_list, sink_info, sink_num).input_list, sink_input_info, i).mute, offset + i);
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
		case 'k':
		case KEY_UP:
			if (chooser_input == -1 && chooser_sink > 0) {
				--chooser_sink;
				chooser_input = (gint)g_array_index(sink_list, sink_info, chooser_sink).input_list->len - 1;
			}

			else if (chooser_input >= 0)
				--chooser_input;
			break;

		case 'j':
		case KEY_DOWN:
			if (chooser_input == ((gint)g_array_index(sink_list, sink_info, chooser_sink).input_list->len - 1) && chooser_sink < (gint)sink_list->len - 1) {
					++chooser_sink;
					chooser_input = -1;
			}
			else if (chooser_input < ((gint)g_array_index(sink_list, sink_info, chooser_sink).input_list->len - 1))
				++chooser_input;
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
				sink_input_info *input = &g_array_index(g_array_index(sink_list, sink_info, chooser_sink).input_list, sink_input_info, chooser_input);
				tmp = (struct tmp_t) {
					.index      = input->index,
					.volume     = (pa_cvolume) {.channels = input->channels},
					.tmp_vol    = input->vol, 
					.volume_set = pa_context_set_sink_input_volume
				};
			} else if (chooser_input == -1) {
				sink_info *sink = &g_array_index(sink_list, sink_info, chooser_sink);
				tmp = (struct tmp_t) {
					.index      = sink->index,
					.volume     = (pa_cvolume) {.channels = sink->channels},
					.tmp_vol    = sink->vol,
					.volume_set = pa_context_set_sink_volume_by_index
				};
			} else
				break;

			int input_vol = tmp.tmp_vol + 2 * volume_mult * (VOLUME_MAX / 100);
			tmp.tmp_vol = CLAMP(input_vol, 0, VOLUME_MAX); /* force input_vol in [0, VOL_MAX] */
			for (int i = 0; i < tmp.volume.channels; ++i)
				tmp.volume.values[i] = tmp.tmp_vol;

			pa_operation_unref(tmp.volume_set(context, tmp.index, &tmp.volume, change_callback, NULL));
			return;
		case 'm':
		case 'M': {
			struct tmp_t {
				int index, mute;
				pa_operation* (*mute_set) (pa_context*, uint32_t, int, pa_context_success_cb_t, void*);
			} tmp;

			if (chooser_input >= 0) {
				sink_input_info *input = &g_array_index(g_array_index(sink_list, sink_info, chooser_sink).input_list, sink_input_info, chooser_input);
				tmp = (struct tmp_t) {
					.index    = input->index,
					.mute     = input->mute,
					.mute_set = pa_context_set_sink_input_mute
				};
			} else if (chooser_input == -1) {
				sink_info *sink = &g_array_index(sink_list, sink_info, chooser_sink);
				tmp = (struct tmp_t) {
					.index    = sink->index,
					.mute     = sink->mute,
					.mute_set = pa_context_set_sink_mute_by_index
				};
			} else
				break;

			pa_operation_unref(tmp.mute_set(context, tmp.index, !tmp.mute, change_callback, NULL));
			return;
		}
		case '\n':
		case ' ':
			if (chooser_input == -1)
				break;
			selected_index = g_array_index(g_array_index(sink_list, sink_info, chooser_sink).input_list, sink_input_info, chooser_input).index;
			if (chooser_sink < (gint)sink_list->len - 1)
				chooser_sink++;
			else
				chooser_sink = 0;

			chooser_input = -2; /* chooser_input needs to be derived from $selected_index */
			pa_operation_unref(pa_context_move_sink_input_by_index(context, selected_index,
						g_array_index(sink_list, sink_info, chooser_sink).index,
						change_callback, NULL));
			return;

		case 'q':
		default:
			printf("key: %d\n", c);
			quit();
			break;
	}
	collect_all_info();
}

void interface_clear(void)
{
	clrtoeol();
	refresh();
	endwin();
	exit(0);
}
