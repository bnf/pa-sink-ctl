#ifndef INTERFACE_H
#define INTERFACE_H

#include <pulse/pulseaudio.h>

#define VOLUME_BAR_LEN 50
#define WIDTH 80
#define HEIGHT 10

void print_sink_list(void);
void print_input_list(int sink_num);
void print_volume(pa_volume_t, int, int);
void get_input(void);
void interface_init(void);
void interface_clear(void);

#endif
