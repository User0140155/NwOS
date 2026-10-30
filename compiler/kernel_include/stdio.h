#ifndef NWC_KERNEL_STDIO_H
#define NWC_KERNEL_STDIO_H

#ifndef NWC_KERNEL_SIZE_T_DEFINED
#define NWC_KERNEL_SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif
#ifndef EOF
#define EOF (-1)
#endif
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct NwcKernelFile {
    unsigned char *data;
    unsigned int capacity;
    unsigned int size;
    unsigned int position;
    int mode;
    int open;
} FILE;

extern FILE *stderr;
FILE *tmpfile(void);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *file);
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *file);
size_t fread(void *ptr, size_t size, size_t count, FILE *file);
int fseek(FILE *file, long offset, int origin);
long ftell(FILE *file);
int fputc(int ch, FILE *file);
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int snprintf(char *buffer, size_t capacity, const char *format, ...);

#endif
