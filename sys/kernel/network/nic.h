#ifndef NIC_H
#define NIC_H

typedef struct nic_driver {
    const char *name;
    int (*probe)(void);                           /* 0 ok, <0 on failure/not found */
    int (*link_up)(void);                         /* 1 if link is up */
    const unsigned char *(*mac)(void);            /* 6-byte MAC */
    int (*send)(const void *frame, unsigned int len);
    int (*recv)(void *buf, unsigned int max);
} nic_driver_t;

#endif
