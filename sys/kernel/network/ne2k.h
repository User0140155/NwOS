#ifndef NE2K_H
#define NE2K_H

#include "nic.h"

int ne2k_probe(void);
int ne2k_link_up(void);
const unsigned char *ne2k_mac(void);
int ne2k_send(const void *frame, unsigned int len);
int ne2k_recv(void *buf, unsigned int max);

extern const nic_driver_t ne2k_driver;

#endif
