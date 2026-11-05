#include "touchpad.h"
#include "mouse.h"

#define PS2_DATA_PORT          0x60
#define PS2_STATUS_PORT        0x64
#define PS2_COMMAND_PORT       0x64

#define PS2_STATUS_OUTPUT      0x01
#define PS2_STATUS_INPUT       0x02

#define PS2_CMD_WRITE_AUX      0xD4

#define PS2_CMD_DISABLE        0xF5
#define PS2_CMD_ENABLE         0xF4
#define PS2_CMD_SET_RATE       0xF3
#define PS2_CMD_GET_INFO       0xE9
#define PS2_CMD_SET_SCALE      0xE6
#define PS2_CMD_SET_RES        0xE8

#define PS2_ACK                0xFA

#define SYN_SAMPLE_RATE        0x14
#define SYN_MODE_ABSOLUTE      0x80
#define SYN_MODE_HIGH_RATE     0x40

#define SYN_QUERY_MODEL        0x03
#define SYN_MODEL_NEWABS       0x80

#define TP_X_MIN               1472
#define TP_X_MAX               5472
#define TP_Y_MIN               1408
#define TP_Y_MAX               4448

#define TP_PRESSURE            20
#define TP_TAP_PACKETS         20
#define TP_TAP_MOVE            300

static int touchpad_ready = 0;
static int touchpad_width = 640;
static int touchpad_height = 480;

static unsigned char touchpad_packet[6];
static int touchpad_cycle = 0;

static int touchpad_buttons = 0;
static int touchpad_screen_x = 0;
static int touchpad_screen_y = 0;

static int touchpad_finger = 0;
static int touchpad_tap_packets = 0;
static int touchpad_tap_start_x = 0;
static int touchpad_tap_start_y = 0;
static int touchpad_tap_moved = 0;
static int touchpad_tap_click = 0;

static unsigned char tp_inb(unsigned short port)
{
    unsigned char value;

    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));

    return value;
}

static void tp_outb(unsigned short port, unsigned char value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static int tp_wait_input(void)
{
    for (unsigned int i = 0; i < 100000; i++)
    {
        if (!(tp_inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT))
            return 1;
    }

    return 0;
}

static int tp_wait_output(void)
{
    for (unsigned int i = 0; i < 100000; i++)
    {
        if (tp_inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT)
            return 1;
    }

    return 0;
}

static void tp_drain(void)
{
    for (unsigned int i = 0; i < 100000; i++)
    {
        if (!(tp_inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT))
            return;

        (void)tp_inb(PS2_DATA_PORT);
    }
}

static int tp_send(unsigned char value)
{
    if (!tp_wait_input())
        return 0;

    tp_outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_AUX);

    if (!tp_wait_input())
        return 0;

    tp_outb(PS2_DATA_PORT, value);

    return 1;
}

static int tp_read(unsigned char* value)
{
    if (!tp_wait_output())
        return 0;

    *value = tp_inb(PS2_DATA_PORT);

    return 1;
}

static int tp_ack(void)
{
    unsigned char value;

    if (!tp_read(&value))
        return 0;

    return value == PS2_ACK;
}

static int tp_command(unsigned char command)
{
    if (!tp_send(command))
        return 0;

    return tp_ack();
}

static int tp_command_arg(unsigned char command, unsigned char arg)
{
    if (!tp_command(command))
        return 0;

    if (!tp_send(arg))
        return 0;

    return tp_ack();
}

static int tp_get_info(unsigned char info[3])
{
    if (!tp_command(PS2_CMD_GET_INFO))
        return 0;

    for (int i = 0; i < 3; i++)
    {
        if (!tp_read(&info[i]))
            return 0;
    }

    return 1;
}

static int tp_sliced_command(unsigned char command)
{
    if (!tp_command(PS2_CMD_SET_SCALE))
        return 0;

    for (int bit = 6; bit >= 0; bit -= 2)
    {
        if (!tp_command_arg(PS2_CMD_SET_RES,
                            (unsigned char)((command >> bit) & 0x03)))
            return 0;
    }

    return 1;
}

static int tp_query(unsigned char query, unsigned char out[3])
{
    if (!tp_sliced_command(query))
        return 0;

    return tp_get_info(out);
}

static int tp_detect(void)
{
    unsigned char info[3];

    for (int i = 0; i < 4; i++)
    {
        if (!tp_command_arg(PS2_CMD_SET_RES, 0x00))
            return 0;
    }

    if (!tp_get_info(info))
        return 0;

    if (info[1] != 0x47)
        return 0;

    return 1;
}

static int tp_enter_absolute_mode(void)
{
    unsigned char model[3];
    unsigned int model_id;

    if (!tp_query(SYN_QUERY_MODEL, model))
        return 0;

    model_id = ((unsigned int)model[0] << 16) |
               ((unsigned int)model[1] << 8) |
               (unsigned int)model[2];

    if (!(model_id & SYN_MODEL_NEWABS))
        return 0;

    if (!tp_sliced_command(SYN_MODE_ABSOLUTE | SYN_MODE_HIGH_RATE))
        return 0;

    if (!tp_command_arg(PS2_CMD_SET_RATE, SYN_SAMPLE_RATE))
        return 0;

    return 1;
}

static int tp_clamp(int value, int min, int max)
{
    if (value < min)
        return min;

    if (value > max)
        return max;

    return value;
}

static int tp_abs(int value)
{
    if (value < 0)
        return -value;

    return value;
}

static int tp_map_x(int x)
{
    x = tp_clamp(x, TP_X_MIN, TP_X_MAX);

    return (x - TP_X_MIN) * (touchpad_width - 1) / (TP_X_MAX - TP_X_MIN);
}

static int tp_map_y(int y)
{
    y = tp_clamp(y, TP_Y_MIN, TP_Y_MAX);

    return (TP_Y_MAX - y) * (touchpad_height - 1) / (TP_Y_MAX - TP_Y_MIN);
}

int touchpad_init(int width, int height)
{
    touchpad_ready = 0;
    touchpad_cycle = 0;
    touchpad_buttons = 0;
    touchpad_finger = 0;
    touchpad_tap_packets = 0;
    touchpad_tap_moved = 0;
    touchpad_tap_click = 0;

    touchpad_set_bounds(width, height);

    touchpad_screen_x = touchpad_width / 2;
    touchpad_screen_y = touchpad_height / 2;

    tp_drain();
    tp_command(PS2_CMD_DISABLE);
    tp_drain();

    if (!tp_detect())
        return 0;

    if (!tp_enter_absolute_mode())
        return 0;

    tp_drain();

    if (!tp_command(PS2_CMD_ENABLE))
        return 0;

    tp_drain();

    touchpad_cycle = 0;
    touchpad_ready = 1;

    return 1;
}

int touchpad_active(void)
{
    return touchpad_ready;
}

void touchpad_set_bounds(int width, int height)
{
    if (width > 0)
        touchpad_width = width;

    if (height > 0)
        touchpad_height = height;
}

const char* touchpad_name(void)
{
    if (!touchpad_ready)
        return "none";

    return "Synaptics";
}

void touchpad_handle_byte(unsigned char byte)
{
    int x;
    int y;
    int z;
    int buttons;

    if (!touchpad_ready)
        return;

    if (touchpad_cycle == 0 && (byte & 0xC8) != 0x80)
        return;

    touchpad_packet[touchpad_cycle++] = byte;

    if (touchpad_cycle < 6)
        return;

    touchpad_cycle = 0;

    if ((touchpad_packet[3] & 0xC8) != 0xC0)
        return;

    x = (((int)(touchpad_packet[3] & 0x10) << 8) |
         ((int)(touchpad_packet[1] & 0x0F) << 8) |
         (int)touchpad_packet[4]);

    y = (((int)(touchpad_packet[3] & 0x20) << 7) |
         ((int)(touchpad_packet[1] & 0xF0) << 4) |
         (int)touchpad_packet[5]);

    z = touchpad_packet[2];
    buttons = touchpad_packet[0] & 0x07;

    touchpad_buttons = buttons;

    if (z > TP_PRESSURE)
    {
        if (!touchpad_finger)
        {
            touchpad_finger = 1;
            touchpad_tap_packets = 0;
            touchpad_tap_start_x = x;
            touchpad_tap_start_y = y;
            touchpad_tap_moved = 0;
        }
        else
        {
            if (touchpad_tap_packets < 0x7FFFFFFF)
                touchpad_tap_packets++;

            if (tp_abs(x - touchpad_tap_start_x) > TP_TAP_MOVE ||
                tp_abs(y - touchpad_tap_start_y) > TP_TAP_MOVE)
                touchpad_tap_moved = 1;
        }

        touchpad_screen_x = tp_map_x(x);
        touchpad_screen_y = tp_map_y(y);
    }
    else
    {
        if (touchpad_finger)
        {
            if (!touchpad_tap_moved &&
                touchpad_tap_packets <= TP_TAP_PACKETS)
                touchpad_tap_click = 1;

            touchpad_finger = 0;
        }
    }

    if (touchpad_tap_click)
        buttons |= 0x01;

    mouse_set_state(touchpad_screen_x,
                    touchpad_screen_y,
                    buttons);
}

void touchpad_clear_tap(void)
{
    if (!touchpad_ready)
        return;

    if (!touchpad_tap_click)
        return;

    touchpad_tap_click = 0;
    mouse_set_buttons(touchpad_buttons);
}
