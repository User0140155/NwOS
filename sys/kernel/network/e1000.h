#ifndef E1000_H
#define E1000_H

#include "nic.h"

#define E1000_ERR_NO_DEVICE   (-1)
#define E1000_ERR_BAD_BAR     (-2)
#define E1000_ERR_RESET       (-3)
#define E1000_ERR_NO_MAC      (-4)

int  e1000_probe(void);
int  e1000_link_up(void);
const unsigned char *e1000_mac(void);
int  e1000_send(const void *frame, unsigned int len);
int  e1000_recv(void *buf, unsigned int max);

extern const nic_driver_t e1000_driver;

#endif
