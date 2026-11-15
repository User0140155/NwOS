#ifndef NWO_VM_H
#define NWO_VM_H

#include "../nwo_loader.h"
int nwo_execute(NwoProgram* program);

void nwo_vm_window_begin(int x, int y, int w, int h);
void nwo_vm_window_end(void);
void nwo_vm_cancel_begin(void);
void nwo_vm_cancel_end(void);
int nwo_vm_cancelled(void);
void nwo_vm_request_cancel(void);

#endif
