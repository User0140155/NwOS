#ifndef PCNET_H
#define PCNET_H

#include "nic.h"

/* AMD PCnet-PCI II (Am79C970A, 1022:2000) basic polled driver */

int pcnet_probe(void);
int pcnet_link_up(void);
const unsigned char *pcnet_mac(void);
int pcnet_send(const void *frame, unsigned int len);
int pcnet_recv(void *buf, unsigned int max);

extern const nic_driver_t pcnet_driver;

#endif
