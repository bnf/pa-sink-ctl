#include <stdio.h>
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

extern int sink_counter;
extern int sink_max;
extern sink_info** sink_list;

extern pa_context* context;

void interface_init(void)
{
	// ncurses
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

void print_sink_list(void) {
	int i = 0;
	int x = 2;
	int y = 2;
	int offset = 0;
	
	werase(menu_win);
	box(menu_win, 0, 0);

//	printf("print sinks: %d\n", sink_input_counter);

//	qsort(sink_input_list, sink_input_counter, sizeof(sink_input_info*), cmp_sink_input_list);
	
	for (i = 0; i < sink_counter; ++i) {
		
		if (i == chooser_sink && chooser_input == -1)
			wattron(menu_win, A_REVERSE);

		mvwprintw(menu_win, y+i+offset, x, "%d\t%-10s",
			sink_list[i]->index,
			sink_list[i]->name);
		
		if (i == chooser_sink && chooser_input == -1)
			wattroff(menu_win, A_REVERSE);
		
		print_input_list(i);

		offset += sink_list[i]->input_counter;
	}
	y += i;
/*	for (i = 0; i < sink_input_counter; ++i) {
		if (i == chooser)
			wattron(menu_win, A_REVERSE);

		mvwprintw(menu_win, y+i, x, "%d\t%s\t",
			sink_input_list[i]->sink,
			sink_input_list[i]->name);

		if (i == chooser)
			wattroff(menu_win, A_REVERSE);

		print_volume(sink_input_list[i]->vol, y+i);
	}*/
//	clear();
//	refresh();
}

void print_input_list(int sink_num) {
	int offset = sink_num + 1 + 2;
	for (int i = 0; i < sink_num; ++i)
		offset += sink_list[i]->input_counter;

	for (int i = 0; i < sink_list[sink_num]->input_counter; ++i) {

		if (chooser_sink == sink_num && chooser_input == i)
			wattron(menu_win, A_REVERSE);

		mvwprintw(menu_win, offset + i, 2, "\t %-9s",
			sink_list[sink_num]->input_list[i]->name);

		if (chooser_sink == sink_num && chooser_input == i)
			wattroff(menu_win, A_REVERSE);

		print_volume(sink_list[sink_num]->input_list[i]->vol, offset + i);
	}
		
}

void print_volume(pa_volume_t volume, int y) {

	int x = 20;

	unsigned int vol = (unsigned int) ( (((double)volume) / ((double)VOLUME_MAX)) * VOLUME_BAR_LEN );
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
//	uint32_t sink;
	c = wgetch(menu_win);
	switch (c) {
		case KEY_UP:
			if (chooser_input == -1 && chooser_sink > 0) {
				--chooser_sink;
				chooser_input = sink_list[chooser_sink]->input_counter - 1;
			}

			else if (chooser_input >= 0)
				--chooser_input;
			break;

		case KEY_DOWN:
			if (chooser_input == sink_list[chooser_sink]->input_counter - 1 && chooser_sink < sink_counter - 1) {
					++chooser_sink;
					chooser_input = -1;
			}
			else if (chooser_input < sink_list[chooser_sink]->input_counter - 1)
				++chooser_input;
			break;

		case KEY_LEFT:
			break;

		case KEY_RIGHT:
			break;

		case 32:
			
/*			if (chooser_sink < sink_counter - 1)
				sink = chooser_sink + 1;
			else
				sink = 0;

			pa_operation_unref(
				pa_context_move_sink_input_by_index(
					context, 
					sink_input_list[chooser]->index,
					sink, 
					change_callback, 
					NULL));
			return;*/
			break;

		default:
			printf("key: %d\n", c);
			quit();
			break;
	}
	
	collect_all_info();
//	pa_operation_unref(pa_context_get_sink_info_list(context, get_sink_info_callback, NULL));
//	sink_input_counter = 0;
//	pa_operation_unref(pa_context_get_sink_input_info_list(context, get_sink_input_info_callback, NULL));
}

void interface_clear(void)
{
	clrtoeol();
	refresh();
	endwin();
	exit(0);
}
