#ifndef RTL8169_H
#define RTL8169_H

#include "nic.h"

/* Realtek RTL8169/8168/8111/8136 PCIe Gigabit Ethernet (basic polled driver) */

int rtl8169_probe(void);
int rtl8169_link_up(void);
const unsigned char *rtl8169_mac(void);
int rtl8169_send(const void *frame, unsigned int len);
int rtl8169_recv(void *buf, unsigned int max);

extern const nic_driver_t rtl8169_driver;

#endif
