#ifndef AHCI_H
#define AHCI_H

typedef struct {
    int exists;
    int port;
    unsigned int sectors;
    char model[41];
} ahci_drive_t;

int ahci_init(void);
int ahci_drive_count(void);
ahci_drive_t* ahci_get_drive(int index);

int ahci_read_sector(int index, unsigned int lba, unsigned char* buffer);
int ahci_write_sector(int index, unsigned int lba, const unsigned char* buffer);

#endif
