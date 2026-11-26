#ifndef RTL8822_H
#define RTL8822_H

#include "nic.h"

int rtl8822_probe(void);
int rtl8822_present(void);
int rtl8822_link_up(void);
const unsigned char *rtl8822_mac(void);
int rtl8822_send(const void *frame, unsigned int len);
int rtl8822_recv(void *buf, unsigned int max);
const char *rtl8822_detected_name(void);
int rtl8822_initialized(void);

extern const nic_driver_t rtl8822_driver;

#endif
