#ifndef NWOS_XHCI_H
#define NWOS_XHCI_H

int xhci_init(void);
void xhci_poll(void);
int xhci_keyboard_ready(void);
const char *xhci_status_string(void);

#endif