#ifndef NWOS_MOUSE_H
#define NWOS_MOUSE_H

#include <stdint.h>

void mouse_handle_byte(uint8_t byte);
int mouse_is_touchpad(void);
const char* mouse_device_name(void);
int mouse_buttons(void);
int mouse_y(void);
int mouse_x(void);
int mouse_poll(void);
void mouse_handle_byte(unsigned char byte);
void mouse_init(void);
void mouse_set_buttons(int buttons);
void mouse_set_state(int x, int y, int buttons);
void mouse_set_bounds(int width, int height);
static int mouse_read_ack(void);
static void mouse_write(unsigned char value);
static int ps2_wait_output(void);


#endif

