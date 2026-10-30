#ifndef NWC_KERNEL_STDIO_BACKEND_H
#define NWC_KERNEL_STDIO_BACKEND_H
void nwc_stdio_begin_output(unsigned char *buffer,
                            unsigned int capacity,
                            unsigned int *size_out);
void nwc_stdio_end_output(void);
#endif
