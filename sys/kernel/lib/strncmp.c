#include "nwos_internal.h"

unsigned int random_seed = 123456789;

void *memcpy(void *dest, const void *src, unsigned int n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    for (unsigned int i = 0; i < n; i++)
    {
        d[i] = s[i];
    }

    return dest;
}
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while (*src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dest;
}
int strcmp(const char* a, const char* b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }

    return *a - *b;
}

int strncmp(const char* a, const char* b, int n)
{
    for (int i = 0; i < n; i++)
    {
        if (a[i] != b[i])
        {
            return (unsigned char)a[i] -
                   (unsigned char)b[i];
        }

        if (a[i] == '\0')
        {
            return 0;
        }
    }

    return 0;
}

int starts_with(const char* text, const char* prefix)
{
    while (*prefix)
    {
        if (*text != *prefix)
            return 0;

        text++;
        prefix++;
    }

    return 1;
}

int atoi_simple(const char* text)
{
    int value = 0;

    while (*text >= '0' && *text <= '9')
    {
        value = value * 10 + (*text - '0');
        text++;
    }

    return value;
}

void cfg_strcpy(char* dest, const char* src, int max)
{
    int i = 0;

    while (src[i] != '\0' && i < max - 1)
    {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}

void delay(unsigned int count)
{
    unsigned int i;

    for (i = 0; i < count; i++)
    {
        __asm__ volatile ("nop");
    }
}

int random_range(int min, int max)
{
    return min +
        (rand_simple() % (max - min + 1));
}
