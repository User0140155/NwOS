#ifndef NWOS_XHCI_H
#define NWOS_XHCI_H

int xhci_init(void);
void xhci_poll(void);
int xhci_keyboard_ready(void);
const char *xhci_status_string(void);
int xhci_bios_released(void);
//int pci_set_power_d0(const pci_dev_t *dev);

#endif