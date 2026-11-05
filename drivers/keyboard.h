#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/* Existing NwOS key values are kept unchanged for source compatibility. */
#define KEY_NONE       0
#define KEY_ESC        255
#define KEY_ENTER      256
#define KEY_BACKSPACE  257
#define KEY_TAB        258

#define KEY_UP         259
#define KEY_DOWN       260
#define KEY_LEFT       261
#define KEY_RIGHT      262

#define KEY_SUPER_S    263
#define KEY_F5         264
#define KEY_NUMPAD_8   265
#define KEY_NUMPAD_2   266
#define KEY_F7         267

/* Additional non-ASCII keys. */
#define KEY_F1         268
#define KEY_F2         269
#define KEY_F3         270
#define KEY_F4         271
#define KEY_F6         272
#define KEY_F8         273
#define KEY_F9         274
#define KEY_F10        275
#define KEY_F11        276
#define KEY_F12        277
#define KEY_HOME       278
#define KEY_END        279
#define KEY_PAGE_UP    280
#define KEY_PAGE_DOWN  281
#define KEY_INSERT     282
#define KEY_DELETE     283
#define KEY_PRINT_SCREEN 284
#define KEY_PAUSE      285
#define KEY_NUMPAD_0   286
#define KEY_NUMPAD_1   287
#define KEY_NUMPAD_3   288
#define KEY_NUMPAD_4   289
#define KEY_NUMPAD_5   290
#define KEY_NUMPAD_6   291
#define KEY_NUMPAD_7   292
#define KEY_NUMPAD_9   293
#define KEY_NUMPAD_DOT 294
#define KEY_NUMPAD_PLUS 295
#define KEY_NUMPAD_MINUS 296
#define KEY_NUMPAD_MULTIPLY 297
#define KEY_NUMPAD_DIVIDE 298

/* Modifier bits returned by keyboard_get_modifiers(). */
#define KEY_MOD_LCTRL  (1u << 0)
#define KEY_MOD_LSHIFT (1u << 1)
#define KEY_MOD_LALT   (1u << 2)
#define KEY_MOD_LSUPER (1u << 3)
#define KEY_MOD_RCTRL  (1u << 4)
#define KEY_MOD_RSHIFT (1u << 5)
#define KEY_MOD_RALT   (1u << 6)
#define KEY_MOD_RSUPER (1u << 7)

void keyboard_init(void);

/* Compatibility API used by the existing shell, menus and applications. */
int keyboard_getkey(void);  /* blocking; returns a key-down event */
int keyboard_pollkey(void); /* non-blocking; returns KEY_NONE if no key */

/* Unified input API. The native xHCI driver submits each completed
 * 8-byte HID Boot Keyboard input report here after USB enumeration.
 */
void keyboard_usb_hid_submit_boot_report(const uint8_t report[8]);

/* Current modifier state, independent of the input transport. */
uint8_t keyboard_get_modifiers(void);

/* Returns nonzero when the PS/2 polling path has seen data since init. */
int keyboard_ps2_seen(void);

/* Nonzero if the i8042 keyboard channel answered during initialization.
 * This is not proof that later key scan codes are reaching the OS. */
int keyboard_ps2_init_responded(void);

#ifdef KEYBOARD_UNIT_TEST
void keyboard_test_submit_ps2_scancode(uint8_t scancode);
#endif

#endif