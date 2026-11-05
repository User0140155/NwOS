#ifndef TOUCHPAD_H
#define TOUCHPAD_H

int touchpad_init(int width, int height);
int touchpad_active(void);
void touchpad_set_bounds(int width, int height);
void touchpad_handle_byte(unsigned char byte);
void touchpad_clear_tap(void);
const char* touchpad_name(void);

#endif
