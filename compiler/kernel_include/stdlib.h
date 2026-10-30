#ifndef NWC_KERNEL_STDLIB_H
#define NWC_KERNEL_STDLIB_H
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
#endif
