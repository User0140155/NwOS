#ifndef NWOS_PCI_H
#define NWOS_PCI_H

#include <stdint.h>

#define XHCI_MAX_CONTROLLERS 16

typedef struct pci_dev {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
} pci_dev_t;

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
int pci_scan(pci_dev_t *list, int max_devices);
int pci_find(uint16_t vendor_id, uint16_t device_id, pci_dev_t *out);
int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, pci_dev_t *out);

int pci_scan_xhci(pci_dev_t *controllers, int max_controllers);
int pci_enable_mem_busmaster(const pci_dev_t *dev);
int pci_get_mmio_bar(const pci_dev_t *dev, unsigned int bar_idx, uint64_t *bar_out, int *is_64_out);

#endif
