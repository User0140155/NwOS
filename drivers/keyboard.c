#include "keyboard.h"


#if __has_include("mouse.h")

#include "mouse.h"

#else

void mouse_handle_byte(uint8_t byte);

#endif


#if __has_include("usb/xhci.h")

#include "usb/xhci.h"

#else

#include "xhci.h"

#endif

#ifndef KEYBOARD_UNIT_TEST
#include "random/entropy.h"
#endif

#ifndef KEYBOARD_UNIT_TEST

void poweroff(void);

#endif


#define PS2_DATA_PORT       0x60u

#define PS2_STATUS_PORT     0x64u

#define PS2_STATUS_OUTPUT   0x01u

#define PS2_STATUS_INPUT    0x02u

#define PS2_STATUS_AUX      0x20u

#define PS2_STATUS_TIMEOUT  0x40u

#define PS2_STATUS_PARITY   0x80u

#define KEY_QUEUE_SIZE      128u

#define KEY_QUEUE_MASK      (KEY_QUEUE_SIZE - 1u)

#define PS2_WAIT_LIMIT      2000000u


#if (KEY_QUEUE_SIZE & KEY_QUEUE_MASK) != 0

#error KEY_QUEUE_SIZE_must_be_a_power_of_two

#endif


typedef struct key_queue_entry {

    int key;

    uint8_t modifiers;

} key_queue_entry_t;


static volatile uint32_t queue_head;

static volatile uint32_t queue_tail;

static key_queue_entry_t key_queue[KEY_QUEUE_SIZE];

static uint8_t modifiers_state;
static uint8_t last_event_modifiers;
static uint8_t ps2_modifiers_state;
static uint8_t usb_modifiers_state;

static uint8_t caps_lock_state;

static uint8_t num_lock_state;

static uint8_t ps2_extended;

static uint8_t ps2_pause_bytes;

static uint8_t ps2_scan_set;

static uint8_t ps2_translation;

static uint8_t ps2_set2_break;

static uint8_t ps2_set2_extended;

static volatile int ps2_seen_data;

static volatile int ps2_init_responded;

static uint8_t usb_previous_keys[6];

static uint8_t usb_caps_lock_state;

static uint8_t usb_num_lock_state;


#ifdef KEYBOARD_UNIT_TEST

static inline uint8_t kb_inb(uint16_t port) { (void)port; return 0; }

static inline void kb_outb(uint16_t port, uint8_t value) { (void)port; (void)value; }

static inline void cpu_relax(void) { }

static inline uint64_t keyboard_irq_save(void) { return 0; }

static inline void keyboard_irq_restore(uint64_t flags) { (void)flags; }

static int ps2_controller_init(void) { return 0; }

#else

static inline uint8_t kb_inb(uint16_t port)

{

    uint8_t value;

    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));

    return value;

}

static inline void kb_outb(uint16_t port, uint8_t value)

{

    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));

}

static inline void cpu_relax(void)

{

    __asm__ volatile ("pause" ::: "memory");

}

static inline uint64_t keyboard_irq_save(void)

{

    uint64_t flags;

    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");

    return flags;

}

static inline void keyboard_irq_restore(uint64_t flags)

{

    if (flags & (1ull << 9))

        __asm__ volatile ("sti" ::: "memory");

}


static int ps2_wait_input_empty(void)

{

    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; ++i) {

        uint8_t status = kb_inb(PS2_STATUS_PORT);

        if (status == 0xFFu) return 0;

        if (!(status & PS2_STATUS_INPUT)) return 1;

        cpu_relax();

    }

    return 0;

}


static int ps2_wait_output_full(uint8_t *status_out)

{

    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; ++i) {

        uint8_t status = kb_inb(PS2_STATUS_PORT);

        if (status == 0xFFu) return 0;

        if (status & PS2_STATUS_OUTPUT) {

            if (status_out) *status_out = status;

            return 1;

        }

        cpu_relax();

    }

    return 0;

}


static int ps2_write_controller(uint8_t command)

{

    if (!ps2_wait_input_empty()) return 0;

    kb_outb(PS2_STATUS_PORT, command);

    return 1;

}


static int ps2_write_data(uint8_t value)

{

    if (!ps2_wait_input_empty()) return 0;

    kb_outb(PS2_DATA_PORT, value);

    return 1;

}


static int ps2_read_controller_data(uint8_t *value)

{

    if (!value) return 0;

    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; ++i) {

        uint8_t status, byte;

        if (!ps2_wait_output_full(&status)) return 0;

        byte = kb_inb(PS2_DATA_PORT);

        if (status & (PS2_STATUS_TIMEOUT | PS2_STATUS_PARITY)) continue;

        if (status & PS2_STATUS_AUX) {

            mouse_handle_byte(byte);

            continue;

        }

        *value = byte;

        return 1;

    }

    return 0;

}


static void ps2_drain_output(void)

{

    for (unsigned int i = 0; i < 32u; ++i) {

        uint8_t status = kb_inb(PS2_STATUS_PORT);

        if (status == 0xFFu || !(status & PS2_STATUS_OUTPUT)) break;

        (void)kb_inb(PS2_DATA_PORT);

    }

}


static int ps2_wait_keyboard_ack(void)

{

    for (uint32_t i = 0; i < PS2_WAIT_LIMIT; ++i) {

        uint8_t status = kb_inb(PS2_STATUS_PORT);

        uint8_t byte;

        if (status == 0xFFu) return 0;

        if (!(status & PS2_STATUS_OUTPUT)) { cpu_relax(); continue; }

        byte = kb_inb(PS2_DATA_PORT);

        if (status & (PS2_STATUS_TIMEOUT | PS2_STATUS_PARITY)) continue;

        if (status & PS2_STATUS_AUX) { mouse_handle_byte(byte); continue; }

        if (byte == 0xFAu) return 1;

        if (byte == 0xFEu) return 0;

        if (byte == 0xAAu || byte == 0xFCu || byte == 0xFDu) continue;

        ps2_seen_data = 1;

        /* A real scan code means the keyboard is already streaming. */

        return 2;

    }

    return 0;

}


static int ps2_controller_init(void)

{

    uint8_t command_byte = 0;

    int got_config = 0;

    int got_ack = 0;


    if (kb_inb(PS2_STATUS_PORT) == 0xFFu) return 0;


    /* Do not leave the keyboard interface disabled if a later step fails. */

    (void)ps2_write_controller(0xADu);

    ps2_drain_output();


    if (ps2_write_controller(0x20u) &&

        ps2_read_controller_data(&command_byte)) {

        got_config = 1;

        /* Polling mode; request Set-2 -> Set-1 translation.  Keep the clock

           enabled (bit 4 clear) and interrupts disabled (bit 0 clear). */

        command_byte |= 0x40u;

        command_byte &= (uint8_t)~0x11u;

        if (ps2_write_controller(0x60u) && ps2_write_data(command_byte))

            ps2_translation = (command_byte & 0x40u) ? 1u : 0u;

    }

    (void)ps2_write_controller(0xAEu);


    /* Ask the keyboard to start scanning.  Retry a few times: some keyboards

       need longer after power-on and reply 0xFE (resend). */

    for (unsigned int attempt = 0; attempt < 3u && !got_ack; ++attempt) {

        int ack;

        if (!ps2_write_data(0xF4u)) break;

        ack = ps2_wait_keyboard_ack();

        if (ack == 1) got_ack = 1;

        else if (ack == 2) { got_ack = 1; ps2_seen_data = 1; }

    }


    /* A keyboard left in an undefined state (common after a warm reboot on

       real hardware) can be revived with a reset. */

    if (!got_ack) {

        (void)ps2_write_data(0xFFu);

        if (ps2_wait_keyboard_ack() != 0) {

            if (ps2_write_data(0xF4u) && ps2_wait_keyboard_ack() != 0)

                got_ack = 1;

        }

    }


    if (!got_config)

        ps2_translation = 1u;

    ps2_scan_set = ps2_translation ? 1u : 2u;

    return got_ack || got_config || ps2_seen_data;

}

#endif


static int queue_is_empty(void) { return queue_tail == queue_head; }


#ifndef KEYBOARD_UNIT_TEST
static uint64_t keyboard_read_tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("lfence; rdtsc" : "=a"(lo), "=d"(hi) : : "memory");
    return ((uint64_t)hi << 32) | lo;
}
#endif

static void queue_push(int key)

{

    uint32_t head, next;

    if (key == KEY_NONE) return;

#ifndef KEYBOARD_UNIT_TEST
    /* PS/2 and USB HID input both reach this shared queue function. */
    nw_entropy_add_keyboard_event(keyboard_read_tsc(), key);
#endif

    head = queue_head;

    next = (head + 1u) & KEY_QUEUE_MASK;

    if (next == queue_tail) queue_tail = (queue_tail + 1u) & KEY_QUEUE_MASK;

    key_queue[head].key = key;

    key_queue[head].modifiers = modifiers_state;

    __asm__ volatile ("" ::: "memory");

    queue_head = next;

}


static int queue_pop(uint8_t *modifiers_out)

{

    uint32_t tail;

    int key;

    if (queue_is_empty()) return KEY_NONE;

    tail = queue_tail;

    __asm__ volatile ("" ::: "memory");

    key = key_queue[tail].key;

    if (modifiers_out) *modifiers_out = key_queue[tail].modifiers;

    queue_tail = (tail + 1u) & KEY_QUEUE_MASK;

    return key;

}


static int shift_down(void)

{

    return (modifiers_state & (KEY_MOD_LSHIFT | KEY_MOD_RSHIFT)) != 0;

}

static int ctrl_down(void)

{

    return (modifiers_state & (KEY_MOD_LCTRL | KEY_MOD_RCTRL)) != 0;

}

static int super_down(void)

{

    return (modifiers_state & (KEY_MOD_LSUPER | KEY_MOD_RSUPER)) != 0;

}


static int apply_shortcuts(int key)

{

    if (super_down() && (key == 's' || key == 'S')) return KEY_SUPER_S;

    if (ctrl_down() && key >= 'a' && key <= 'z') return key - 'a' + 1;

    if (ctrl_down() && key >= 'A' && key <= 'Z') return key - 'A' + 1;

    if (ctrl_down() && (key == 's' || key == 'S')) return 19;

    if (ctrl_down() && (key == 'x' || key == 'X')) return 24;

    return key;

}


static const uint8_t ps2_normal_map[128] = {

    [0x01]=27, [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',

    [0x07]='6',[0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',[0x0C]='-',

    [0x0D]='=',[0x0E]='\b',[0x0F]='\t',[0x10]='q',[0x11]='w',[0x12]='e',

    [0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',

    [0x19]='p',[0x1A]='[',[0x1B]=']',[0x1C]='\n',[0x1E]='a',[0x1F]='s',

    [0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',[0x25]='k',

    [0x26]='l',[0x27]=';',[0x28]='\'',[0x29]='`',[0x2B]='\\',[0x2C]='z',

    [0x2D]='x',[0x2E]='c',[0x2F]='v',[0x30]='b',[0x31]='n',[0x32]='m',

    [0x33]=',',[0x34]='.',[0x35]='/',[0x37]='*',[0x39]=' '

};


static const uint8_t ps2_shift_map[128] = {

    [0x01]=27,[0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',

    [0x07]='^',[0x08]='&',[0x09]='*',[0x0A]='(',[0x0B]=')',[0x0C]='_',

    [0x0D]='+',[0x0E]='\b',[0x0F]='\t',[0x10]='Q',[0x11]='W',[0x12]='E',

    [0x13]='R',[0x14]='T',[0x15]='Y',[0x16]='U',[0x17]='I',[0x18]='O',

    [0x19]='P',[0x1A]='{',[0x1B]='}',[0x1C]='\n',[0x1E]='A',[0x1F]='S',

    [0x20]='D',[0x21]='F',[0x22]='G',[0x23]='H',[0x24]='J',[0x25]='K',

    [0x26]='L',[0x27]=':',[0x28]='"',[0x29]='~',[0x2B]='|',[0x2C]='Z',

    [0x2D]='X',[0x2E]='C',[0x2F]='V',[0x30]='B',[0x31]='N',[0x32]='M',

    [0x33]='<',[0x34]='>',[0x35]='?',[0x37]='*',[0x39]=' '

};


static void keyboard_refresh_modifiers(void)
{
    /* Keep PS/2 and USB state independent so one input backend cannot
       accidentally release a modifier held by the other backend. */
    modifiers_state = (uint8_t)(ps2_modifiers_state | usb_modifiers_state);
}

static void set_modifier(uint8_t bit, int down)
{
    uint8_t old_state = ps2_modifiers_state;

    if (down)
        ps2_modifiers_state |= bit;
    else
        ps2_modifiers_state &= (uint8_t)~bit;

    keyboard_refresh_modifiers();

#ifndef KEYBOARD_UNIT_TEST
    if (old_state != ps2_modifiers_state)
        nw_entropy_add_keyboard_event(keyboard_read_tsc(), 0x100 | bit);
#else
    (void)old_state;
#endif
}

static void ps2_clear_modifiers(void)
{
    set_modifier(KEY_MOD_LSHIFT, 0);
    set_modifier(KEY_MOD_RSHIFT, 0);
    set_modifier(KEY_MOD_LCTRL, 0);
    set_modifier(KEY_MOD_RCTRL, 0);
    set_modifier(KEY_MOD_LALT, 0);
    set_modifier(KEY_MOD_RALT, 0);
    set_modifier(KEY_MOD_LSUPER, 0);
    set_modifier(KEY_MOD_RSUPER, 0);
}

static int ps2_is_response_byte(uint8_t byte)
{
    /* 0xAA is deliberately NOT ignored here: in Set 1 it is left-Shift-up. */
    return byte == 0xFAu || byte == 0xFEu ||
           byte == 0xFCu || byte == 0xFDu;
}


static int ps2_decode(uint8_t scancode)

{

    uint8_t code;

    int released;

    int key = KEY_NONE;


    if (ps2_pause_bytes) {

        --ps2_pause_bytes;

        if (!ps2_pause_bytes) queue_push(KEY_PAUSE);

        return KEY_NONE;

    }

    if (scancode == 0xE1u) { ps2_extended = 0; ps2_pause_bytes = 5u; return KEY_NONE; }

    if (scancode == 0xE0u) { ps2_extended = 1; return KEY_NONE; }

    released = (scancode & 0x80u) != 0;

    code = scancode & 0x7Fu;


    if (ps2_extended) {

        ps2_extended = 0;

        if (code == 0x1D) { set_modifier(KEY_MOD_RCTRL, !released); return KEY_NONE; }

        if (code == 0x38) { set_modifier(KEY_MOD_RALT, !released); return KEY_NONE; }

        if (code == 0x5B) { set_modifier(KEY_MOD_LSUPER, !released); return KEY_NONE; }

        if (code == 0x5C) { set_modifier(KEY_MOD_RSUPER, !released); return KEY_NONE; }

        if (released) return KEY_NONE;

        switch (code) {

        case 0x48:key=KEY_UP;break; case 0x50:key=KEY_DOWN;break;

        case 0x4B:key=KEY_LEFT;break; case 0x4D:key=KEY_RIGHT;break;

        case 0x47:key=KEY_HOME;break; case 0x4F:key=KEY_END;break;

        case 0x49:key=KEY_PAGE_UP;break; case 0x51:key=KEY_PAGE_DOWN;break;

        case 0x52:key=KEY_INSERT;break; case 0x53:key=KEY_DELETE;break;

        case 0x35:key=KEY_NUMPAD_DIVIDE;break; case 0x1C:key=KEY_ENTER;break;

        default:break;

        }

        if (key != KEY_NONE) queue_push(key);

        return key;

    }


    if (code == 0x2A) { set_modifier(KEY_MOD_LSHIFT,!released); return KEY_NONE; }

    if (code == 0x36) { set_modifier(KEY_MOD_RSHIFT,!released); return KEY_NONE; }

    if (code == 0x1D) { set_modifier(KEY_MOD_LCTRL,!released); return KEY_NONE; }

    if (code == 0x38) { set_modifier(KEY_MOD_LALT,!released); return KEY_NONE; }

    if (released) return KEY_NONE;

    if (code == 0x3A) { caps_lock_state ^= 1u; return KEY_NONE; }

    if (code == 0x45) { num_lock_state ^= 1u; return KEY_NONE; }


    switch (code) {

    case 0x01:key=KEY_ESC;break; case 0x0E:key=KEY_BACKSPACE;break;

    case 0x0F:key=KEY_TAB;break; case 0x1C:key=KEY_ENTER;break;

    case 0x3B:key=KEY_F1;break; case 0x3C:key=KEY_F2;break;

    case 0x3D:key=KEY_F3;break; case 0x3E:key=KEY_F4;break;

    case 0x3F:key=KEY_F5;break; case 0x40:key=KEY_F6;break;

    case 0x41:key=KEY_F7;break; case 0x42:key=KEY_F8;break;

    case 0x43:key=KEY_F9;break; case 0x44:key=KEY_F10;break;

    case 0x57:key=KEY_F11;break; case 0x58:key=KEY_F12;break;

    case 0x48:key=KEY_NUMPAD_8;break; case 0x50:key=KEY_NUMPAD_2;break;

    case 0x47:key=KEY_NUMPAD_7;break; case 0x49:key=KEY_NUMPAD_9;break;

    case 0x4B:key=KEY_NUMPAD_4;break; case 0x4C:key=KEY_NUMPAD_5;break;

    case 0x4D:key=KEY_NUMPAD_6;break; case 0x4F:key=KEY_NUMPAD_1;break;

    case 0x51:key=KEY_NUMPAD_3;break; case 0x52:key=KEY_NUMPAD_0;break;

    case 0x53:key=KEY_NUMPAD_DOT;break; case 0x4A:key=KEY_NUMPAD_MINUS;break;

    case 0x4E:key=KEY_NUMPAD_PLUS;break;

    default:break;

    }

    if (key == KEY_NONE && code < 128u) {

        uint8_t plain = ps2_normal_map[code], ch;

        if (plain >= 'a' && plain <= 'z')

            ch = (caps_lock_state ^ shift_down()) ? (uint8_t)(plain - 'a' + 'A') : plain;

        else

            ch = shift_down() ? ps2_shift_map[code] : plain;

        if (ch) key = apply_shortcuts((int)ch);

    }

    if (key != KEY_NONE) queue_push(key);

    return key;

}


static uint8_t ps2_set2_to_set1(uint8_t c)

{

    switch (c) {

    case 0x1C:return 0x1E; case 0x32:return 0x30; case 0x21:return 0x2E;

    case 0x23:return 0x20; case 0x24:return 0x12; case 0x2B:return 0x21;

    case 0x34:return 0x22; case 0x33:return 0x23; case 0x43:return 0x17;

    case 0x3B:return 0x24; case 0x42:return 0x25; case 0x4B:return 0x26;

    case 0x3A:return 0x32; case 0x31:return 0x31; case 0x44:return 0x18;

    case 0x4D:return 0x19; case 0x15:return 0x10; case 0x2D:return 0x13;

    case 0x1B:return 0x1F; case 0x2C:return 0x14; case 0x3C:return 0x16;

    case 0x2A:return 0x2F; case 0x1D:return 0x11; case 0x22:return 0x2D;

    case 0x35:return 0x15; case 0x1A:return 0x2C; case 0x16:return 0x02;

    case 0x1E:return 0x03; case 0x26:return 0x04; case 0x25:return 0x05;

    case 0x2E:return 0x06; case 0x36:return 0x07; case 0x3D:return 0x08;

    case 0x3E:return 0x09; case 0x46:return 0x0A; case 0x45:return 0x0B;

    case 0x4E:return 0x0C; case 0x55:return 0x0D; case 0x54:return 0x1A;

    case 0x5B:return 0x1B; case 0x5D:return 0x2B; case 0x4C:return 0x27;

    case 0x52:return 0x28; case 0x0E:return 0x29; case 0x41:return 0x33;

    case 0x49:return 0x34; case 0x4A:return 0x35; case 0x29:return 0x39;

    case 0x76:return 0x01; case 0x66:return 0x0E; case 0x0D:return 0x0F;

    case 0x5A:return 0x1C; case 0x12:return 0x2A; case 0x59:return 0x36;

    case 0x14:return 0x1D; case 0x11:return 0x38; case 0x58:return 0x3A;

    case 0x77:return 0x45; case 0x05:return 0x3B; case 0x06:return 0x3C;

    case 0x04:return 0x3D; case 0x0C:return 0x3E; case 0x03:return 0x3F;

    case 0x0B:return 0x40; case 0x83:return 0x41; case 0x0A:return 0x42;

    case 0x01:return 0x43; case 0x09:return 0x44; case 0x78:return 0x57;

    case 0x07:return 0x58; case 0x6C:return 0x47; case 0x75:return 0x48;

    case 0x7D:return 0x49; case 0x6B:return 0x4B; case 0x73:return 0x4C;

    case 0x74:return 0x4D; case 0x69:return 0x4F; case 0x72:return 0x50;

    case 0x7A:return 0x51; case 0x70:return 0x52; case 0x71:return 0x53;

    case 0x7B:return 0x4A; case 0x79:return 0x4E; default:return 0;

    }

}

static uint8_t ps2_set2_extended_to_set1(uint8_t c)

{

    switch (c) {

    case 0x14:return 0x1D; case 0x11:return 0x38; case 0x1F:return 0x5B;

    case 0x27:return 0x5C; case 0x75:return 0x48; case 0x72:return 0x50;

    case 0x6B:return 0x4B; case 0x74:return 0x4D; case 0x6C:return 0x47;

    case 0x69:return 0x4F; case 0x7D:return 0x49; case 0x7A:return 0x51;

    case 0x70:return 0x52; case 0x71:return 0x53; case 0x4A:return 0x35;

    case 0x5A:return 0x1C; default:return 0;

    }

}

static int ps2_process_scancode(uint8_t byte)

{

    uint8_t translated;

    if (byte == 0xF0u) { ps2_scan_set=2u; ps2_set2_break=1u; return KEY_NONE; }

    if (ps2_scan_set != 2u) return ps2_decode(byte);

    if (byte == 0xE0u) { ps2_set2_extended=1u; return KEY_NONE; }

    if (byte == 0xE1u) { ps2_set2_extended=0; ps2_set2_break=0; ps2_pause_bytes=7u; return KEY_NONE; }

    if (ps2_pause_bytes) { --ps2_pause_bytes; if (!ps2_pause_bytes) queue_push(KEY_PAUSE); return KEY_NONE; }

    translated = ps2_set2_extended ? ps2_set2_extended_to_set1(byte) : ps2_set2_to_set1(byte);

    if (ps2_set2_extended) {

        ps2_set2_extended=0;

        if (!translated) { ps2_set2_break=0; return KEY_NONE; }

        (void)ps2_decode(0xE0u);

    }

    if (!translated) { ps2_set2_break=0; return KEY_NONE; }

    if (ps2_set2_break) translated |= 0x80u;

    ps2_set2_break=0;

    return ps2_decode(translated);

}


#ifndef KEYBOARD_UNIT_TEST

static void ps2_poll_input(void)

{

    for (unsigned int n=0; n<32u; ++n) {

        uint8_t status=kb_inb(PS2_STATUS_PORT), byte;

        if (status==0xFFu) {
            /* No usable legacy controller. Do not retain stale PS/2 modifiers. */
            ps2_clear_modifiers();
            break;
        }
        if (!(status&PS2_STATUS_OUTPUT)) break;

        byte=kb_inb(PS2_DATA_PORT);

        if (status&(PS2_STATUS_TIMEOUT|PS2_STATUS_PARITY)) {
            /* A corrupted byte may have been a modifier break code. Reset the
               partial scan-code parser and release PS/2 modifiers. */
            ps2_extended = 0;
            ps2_pause_bytes = 0;
            ps2_set2_break = 0;
            ps2_set2_extended = 0;
            ps2_clear_modifiers();
            continue;
        }

        if (status&PS2_STATUS_AUX) { mouse_handle_byte(byte); continue; }

        if (ps2_is_response_byte(byte)) continue;

        ps2_seen_data=1;

        (void)ps2_process_scancode(byte);

    }

}

#endif


static int usb_report_has_usage(uint8_t usage, const uint8_t keys[6])

{

    for (unsigned int i=0;i<6u;++i) if (keys[i]==usage) return 1;

    return 0;

}


static uint8_t usb_hid_modifiers_to_internal(uint8_t hid_modifiers)
{
    uint8_t result = 0;

    /* USB HID boot-keyboard order is Ctrl, Shift, Alt, GUI, then right-side
       equivalents. NwOS's KEY_MOD_* bit order is different. */
    if (hid_modifiers & 0x01u) result |= KEY_MOD_LCTRL;
    if (hid_modifiers & 0x02u) result |= KEY_MOD_LSHIFT;
    if (hid_modifiers & 0x04u) result |= KEY_MOD_LALT;
    if (hid_modifiers & 0x08u) result |= KEY_MOD_LSUPER;
    if (hid_modifiers & 0x10u) result |= KEY_MOD_RCTRL;
    if (hid_modifiers & 0x20u) result |= KEY_MOD_RSHIFT;
    if (hid_modifiers & 0x40u) result |= KEY_MOD_RALT;
    if (hid_modifiers & 0x80u) result |= KEY_MOD_RSUPER;

    return result;
}

static int usb_usage_to_key(uint8_t usage, uint8_t mods)

{

    int shift=(mods&(KEY_MOD_LSHIFT|KEY_MOD_RSHIFT))!=0;

    int ch=0;

    if (usage>=0x04u && usage<=0x1Du) {

        ch='a'+(usage-0x04u);

        if (usb_caps_lock_state^shift) ch=ch-'a'+'A';

        return apply_shortcuts(ch);

    }

    if (usage>=0x1Eu && usage<=0x27u) {

        static const char plain[]="1234567890";

        static const char shifted[]="!@#$%^&*()";

        unsigned int i=(unsigned int)(usage-0x1Eu);

        return apply_shortcuts(shift?shifted[i]:plain[i]);

    }

    switch (usage) {

    case 0x28:return KEY_ENTER; case 0x29:return KEY_ESC;

    case 0x2A:return KEY_BACKSPACE; case 0x2B:return KEY_TAB; case 0x2C:return ' ';

    case 0x2D:return shift?'_':'-'; case 0x2E:return shift?'+':'=';

    case 0x2F:return shift?'{':'['; case 0x30:return shift?'}':']';

    case 0x31:return shift?'|':'\\'; case 0x32:return shift?'~':'`';

    case 0x33:return shift?':':';'; case 0x34:return shift?'"':'\'';

    case 0x35:return shift?'~':'`'; case 0x36:return shift?'<':',';

    case 0x37:return shift?'>':'.'; case 0x38:return shift?'?':'/';

    case 0x3A:return KEY_F1; case 0x3B:return KEY_F2; case 0x3C:return KEY_F3;

    case 0x3D:return KEY_F4; case 0x3E:return KEY_F5; case 0x3F:return KEY_F6;

    case 0x40:return KEY_F7; case 0x41:return KEY_F8; case 0x42:return KEY_F9;

    case 0x43:return KEY_F10; case 0x44:return KEY_F11; case 0x45:return KEY_F12;

    case 0x46:return KEY_PRINT_SCREEN; case 0x48:return KEY_PAUSE;

    case 0x49:return KEY_INSERT; case 0x4A:return KEY_HOME; case 0x4B:return KEY_PAGE_UP;

    case 0x4C:return KEY_DELETE; case 0x4D:return KEY_END; case 0x4E:return KEY_PAGE_DOWN;

    case 0x4F:return KEY_RIGHT; case 0x50:return KEY_LEFT; case 0x51:return KEY_DOWN;

    case 0x52:return KEY_UP; case 0x54:return KEY_NUMPAD_DIVIDE;

    case 0x55:return KEY_NUMPAD_MULTIPLY; case 0x56:return KEY_NUMPAD_MINUS;

    case 0x57:return KEY_NUMPAD_PLUS; case 0x58:return KEY_ENTER;

    case 0x59:return usb_num_lock_state?'1':KEY_NUMPAD_1;

    case 0x5A:return usb_num_lock_state?'2':KEY_NUMPAD_2;

    case 0x5B:return usb_num_lock_state?'3':KEY_NUMPAD_3;

    case 0x5C:return usb_num_lock_state?'4':KEY_NUMPAD_4;

    case 0x5D:return usb_num_lock_state?'5':KEY_NUMPAD_5;

    case 0x5E:return usb_num_lock_state?'6':KEY_NUMPAD_6;

    case 0x5F:return usb_num_lock_state?'7':KEY_NUMPAD_7;

    case 0x60:return usb_num_lock_state?'8':KEY_NUMPAD_8;

    case 0x61:return usb_num_lock_state?'9':KEY_NUMPAD_9;

    case 0x62:return usb_num_lock_state?'0':KEY_NUMPAD_0;

    case 0x63:return usb_num_lock_state?'.':KEY_NUMPAD_DOT;

    default:break;

    }

    return ch;

}


void keyboard_usb_hid_submit_boot_report(const uint8_t report[8])

{

    uint64_t flags;

    uint8_t mods, keys[6];

    int rollover=0;

    if (!report) return;

    flags=keyboard_irq_save();

    mods=report[0];

    for (unsigned int i=0;i<6u;++i) { keys[i]=report[2u+i]; if (keys[i]==1u) rollover=1; }

    if (rollover) { keyboard_irq_restore(flags); return; }

    {
        uint8_t translated_modifiers = usb_hid_modifiers_to_internal(mods);
        if (translated_modifiers != usb_modifiers_state) {
            usb_modifiers_state = translated_modifiers;
            keyboard_refresh_modifiers();
#ifndef KEYBOARD_UNIT_TEST
            nw_entropy_add_keyboard_event(keyboard_read_tsc(),
                                          0x200 | translated_modifiers);
#endif
        }
    }

    if (!usb_report_has_usage(0x39u,usb_previous_keys) && usb_report_has_usage(0x39u,keys))

        usb_caps_lock_state^=1u;

    if (!usb_report_has_usage(0x53u,usb_previous_keys) && usb_report_has_usage(0x53u,keys))

        usb_num_lock_state^=1u;

    for (unsigned int i=0;i<6u;++i) {

        uint8_t usage=keys[i];

        int key;

        if (!usage || usb_report_has_usage(usage,usb_previous_keys)) continue;

        key=usb_usage_to_key(usage,usb_modifiers_state);

        if (key!=KEY_NONE) queue_push(key);

    }

    for (unsigned int i=0;i<6u;++i) usb_previous_keys[i]=keys[i];

    keyboard_irq_restore(flags);

}


void keyboard_init(void)

{

    uint64_t flags=keyboard_irq_save();

    queue_head=queue_tail=0;

    modifiers_state=0;
    last_event_modifiers=0;
    ps2_modifiers_state=0;
    usb_modifiers_state=0;

    caps_lock_state=num_lock_state=0;

    ps2_extended=ps2_pause_bytes=0;

    ps2_scan_set=1;

    ps2_translation=1;

    ps2_set2_break=ps2_set2_extended=0;

    ps2_seen_data=0;

    ps2_init_responded=0;

    usb_caps_lock_state=usb_num_lock_state=0;

    for (unsigned int i=0;i<6u;++i) usb_previous_keys[i]=0;

    keyboard_irq_restore(flags);


    /* Keep legacy PS/2 input available, but never let a mere PS/2 ACK

       suppress native USB initialization.  ACK only confirms that a command

       was acknowledged; it does not prove that scan codes reach the kernel. */

    ps2_init_responded=ps2_controller_init();

    (void)xhci_init();

}


int keyboard_pollkey(void)

{

    int key;

    uint8_t event_modifiers=0;

    /* If the first call happened before PMM became available, xhci_init()

       deliberately leaves itself retryable. Once attempted, it returns fast. */

    if (!xhci_keyboard_ready()) (void)xhci_init();

    xhci_poll();

    {

        uint64_t flags=keyboard_irq_save();

#ifndef KEYBOARD_UNIT_TEST

        ps2_poll_input();

#endif

        key=queue_pop(&event_modifiers);

        if (key != KEY_NONE)
            last_event_modifiers = event_modifiers;

        keyboard_irq_restore(flags);

    }

#ifndef KEYBOARD_UNIT_TEST

    if ((key=='r'||key=='R') && (event_modifiers&(KEY_MOD_LALT|KEY_MOD_RALT)))

        poweroff();

#endif

    return key;

}


int keyboard_getkey(void)

{

    int key;

    while ((key=keyboard_pollkey())==KEY_NONE) cpu_relax();

    return key;

}


uint8_t keyboard_get_modifiers(void) { return modifiers_state; }

/* Modifier snapshot captured when the last returned key was pressed.
   Unlike keyboard_get_modifiers(), this remains correct after Shift is released. */
uint8_t keyboard_get_last_event_modifiers(void) { return last_event_modifiers; }

int keyboard_ps2_seen(void) { return ps2_seen_data; }

int keyboard_ps2_init_responded(void) { return ps2_init_responded; }


#ifdef KEYBOARD_UNIT_TEST

void keyboard_test_submit_ps2_scancode(uint8_t scancode) { (void)ps2_decode(scancode); }
void keyboard_test_submit_ps2_byte(uint8_t byte)
{
    if (!ps2_is_response_byte(byte))
        (void)ps2_process_scancode(byte);
}
void keyboard_test_set_ps2_scan_set(uint8_t scan_set)
{
    ps2_scan_set = (scan_set == 2u) ? 2u : 1u;
    ps2_set2_break = 0;
    ps2_set2_extended = 0;
}

#endif
