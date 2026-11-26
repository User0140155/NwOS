#include "fb.h"
#include "../../include/LogoNwOS.h"
#include "../../include/kernelpanic.h"

void dead_boot(void) {
    KPANIC_CRITICAL(DISK_DEATH, "Installed filesystem is corrupted or unreadable.");
}