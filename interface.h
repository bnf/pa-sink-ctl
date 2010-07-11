#ifndef INTERFACE_H
#define INTERFACE_H

#include <ncurses.h>

#define VOLUME_MAX UINT16_MAX
#define VOLUME_BAR_LEN 50
#define WIDTH 80
#define HEIGHT 10

void print_sinks(void);
void print_volume(pa_volume_t, int);
void get_input(void);
void interface_init(void);
void interface_clear(void);

#endif
