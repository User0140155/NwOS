#ifndef RTL8139_H
#define RTL8139_H

#include "nic.h"

int  rtl8139_probe(void);
int  rtl8139_link_up(void);
const unsigned char *rtl8139_mac(void);
int  rtl8139_send(const void *frame, unsigned int len);
int  rtl8139_recv(void *buf, unsigned int max);

extern const nic_driver_t rtl8139_driver;

#endif
