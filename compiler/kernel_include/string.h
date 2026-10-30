#ifndef NWC_KERNEL_STRING_H
#define NWC_KERNEL_STRING_H
#ifndef NWC_KERNEL_SIZE_T_DEFINED
#define NWC_KERNEL_SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif
size_t strlen(const char *text);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *dst, const char *src);
char *strchr(const char *text, int ch);
const char *strrchr(const char *text, int ch);
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int value, size_t n);
#endif
