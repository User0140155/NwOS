#ifndef NWC_RUNTIME_H
#define NWC_RUNTIME_H

#ifdef NWC_KERNEL
#include <stdio.h>
#endif

#ifndef NWC_KERNEL_SIZE_T_DEFINED
#define NWC_KERNEL_SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void free(void *ptr);
long long strtoll(const char *text, char **endptr, int base);
double strtod(const char *text, char **endptr);
size_t strlen(const char *text);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *dst, const char *src);
const char *strrchr(const char *text, int ch);
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int value, size_t n);

int printf(const char *format, ...);
#ifdef NWC_KERNEL
int fprintf(FILE *stream, const char *format, ...);
extern FILE *stderr;
#else
int fprintf(void *stream, const char *format, ...);
extern void *stderr;
#endif

/* Redirect compiler diagnostics into a buffer while the editor is active. */
void nwc_capture_begin(char *buffer, unsigned int capacity);
void nwc_capture_end(void);

void nwc_arena_reset(void);
void nwc_output_begin(unsigned char *buffer,
                      unsigned int capacity,
                      unsigned int *size_out);
int nwc_output_write(const void *data, unsigned int size);
unsigned int nwc_output_size(void);

#endif
