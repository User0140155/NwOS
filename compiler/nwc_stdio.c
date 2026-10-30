#include <stdio.h>
#include <string.h>
#include "nwc_stdio.h"

#define NWC_TMP_CAPACITY 20480u

static unsigned char temp_data[NWC_TMP_CAPACITY];
static FILE temp_file;
static FILE output_file;
static unsigned char *active_output;
static unsigned int active_capacity;
static unsigned int *active_size_out;
static int output_active;

static void reset_file(FILE *f, unsigned char *data,
                       unsigned int capacity, int mode)
{
    f->data = data;
    f->capacity = capacity;
    f->size = 0;
    f->position = 0;
    f->mode = mode;
    f->open = 1;
}

void nwc_stdio_begin_output(unsigned char *buffer,
                            unsigned int capacity,
                            unsigned int *size_out)
{
    active_output = buffer;
    active_capacity = capacity;
    active_size_out = size_out;
    output_active = (buffer != 0 && capacity != 0);
    if (size_out) *size_out = 0;
    reset_file(&output_file, buffer, capacity, 2);
    output_file.open = output_active;
}

void nwc_stdio_end_output(void)
{
    if (active_size_out)
        *active_size_out = output_file.size;
    output_active = 0;
    active_output = 0;
    active_capacity = 0;
    active_size_out = 0;
    output_file.open = 0;
}

FILE *tmpfile(void)
{
    reset_file(&temp_file, temp_data, NWC_TMP_CAPACITY, 1);
    return &temp_file;
}

FILE *fopen(const char *path, const char *mode)
{
    if (!output_active || path == 0 || mode == 0)
        return NULL;
    if (strcmp(path, "memory.nwo") != 0 || strcmp(mode, "wb") != 0)
        return NULL;
    reset_file(&output_file, active_output, active_capacity, 2);
    return &output_file;
}

int fclose(FILE *file)
{
    if (file == 0) return EOF;
    file->open = 0;
    return 0;
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *file)
{
    const unsigned char *src = (const unsigned char *)ptr;
    unsigned int bytes;
    unsigned int i;
    if (!file || !file->open || !ptr || size == 0 || count == 0)
        return 0;
    if (count > 0xFFFFFFFFu / size) return 0;
    bytes = size * count;
    if (file->position > file->capacity ||
        bytes > file->capacity - file->position)
        return 0;
    for (i = 0; i < bytes; ++i)
        file->data[file->position + i] = src[i];
    file->position += bytes;
    if (file->position > file->size) file->size = file->position;
    return count;
}

size_t fread(void *ptr, size_t size, size_t count, FILE *file)
{
    unsigned char *dst = (unsigned char *)ptr;
    unsigned int want, available, bytes, i;
    if (!file || !file->open || !ptr || size == 0 || count == 0)
        return 0;
    if (count > 0xFFFFFFFFu / size) return 0;
    want = size * count;
    if (file->position >= file->size) return 0;
    available = file->size - file->position;
    bytes = want < available ? want : available;
    bytes -= bytes % size;
    for (i = 0; i < bytes; ++i)
        dst[i] = file->data[file->position + i];
    file->position += bytes;
    return bytes / size;
}

int fseek(FILE *file, long offset, int origin)
{
    long base, next;
    if (!file || !file->open) return -1;
    if (origin == SEEK_SET) base = 0;
    else if (origin == SEEK_CUR) base = (long)file->position;
    else if (origin == SEEK_END) base = (long)file->size;
    else return -1;
    next = base + offset;
    if (next < 0 || (unsigned long)next > file->size) return -1;
    file->position = (unsigned int)next;
    return 0;
}

long ftell(FILE *file)
{
    if (!file || !file->open) return -1;
    return (long)file->position;
}

int fputc(int ch, FILE *file)
{
    unsigned char b = (unsigned char)ch;
    if (fwrite(&b, 1, 1, file) != 1) return EOF;
    return (int)b;
}

size_t strlen(const char *text)
{
    size_t n = 0;
    if (!text) return 0;
    while (text[n] != '\0') ++n;
    return n;
}

char *strchr(const char *text, int ch)
{
    unsigned char target = (unsigned char)ch;
    if (!text) return NULL;
    while (*text) {
        if ((unsigned char)*text == target) return (char *)text;
        ++text;
    }
    return target == 0 ? (char *)text : NULL;
}

static void append_char(char *dst, size_t cap, size_t *pos, char c)
{
    if (cap && *pos < cap - 1) dst[*pos] = c;
    ++*pos;
}

static void append_unsigned(char *dst, size_t cap, size_t *pos,
                            unsigned int value)
{
    char digits[16];
    int n = 0;
    do { digits[n++] = (char)('0' + value % 10u); value /= 10u; }
    while (value && n < (int)sizeof(digits));
    while (n > 0) append_char(dst, cap, pos, digits[--n]);
}

int snprintf(char *buffer, size_t capacity, const char *format, ...)
{
    /* Used by the parser for %s plus a few integer/string annotations. */
    __builtin_va_list ap;
    size_t pos = 0;
    size_t i = 0;
    if (!format || (!buffer && capacity)) return -1;
    __builtin_va_start(ap, format);
    while (format[i]) {
        if (format[i] != '%') {
            append_char(buffer, capacity, &pos, format[i++]);
            continue;
        }
        ++i;
        if (format[i] == '%') {
            append_char(buffer, capacity, &pos, '%'); ++i;
        } else if (format[i] == 's') {
            const char *s = __builtin_va_arg(ap, const char *);
            if (s) while (*s) append_char(buffer, capacity, &pos, *s++);
            ++i;
        } else if (format[i] == 'd' || format[i] == 'i') {
            int v = __builtin_va_arg(ap, int);
            if (v < 0) { append_char(buffer, capacity, &pos, '-'); append_unsigned(buffer, capacity, &pos, (unsigned int)(-(v + 1)) + 1u); }
            else append_unsigned(buffer, capacity, &pos, (unsigned int)v);
            ++i;
        } else if (format[i] == 'u') {
            append_unsigned(buffer, capacity, &pos, __builtin_va_arg(ap, unsigned int)); ++i;
        } else if (format[i] == 'c') {
            append_char(buffer, capacity, &pos, (char)__builtin_va_arg(ap, int)); ++i;
        } else {
            append_char(buffer, capacity, &pos, '%');
            if (format[i]) append_char(buffer, capacity, &pos, format[i++]);
        }
    }
    __builtin_va_end(ap);
    if (capacity) buffer[pos < capacity ? pos : capacity - 1] = '\0';
    return pos > 0x7FFFFFFFu ? 0x7FFFFFFF : (int)pos;
}
