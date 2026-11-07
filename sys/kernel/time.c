#include "../../drivers/keyboard.h"
#include "../../drivers/mouse.h"
#include "../../drivers/disk.h"
#include "../../include/kernelpanic.h"

#include "Apps/wordgenerator/wordgenerator.h"
#include "Apps/textredactor/textredactor.h"
#include "nwoloader/nwo_vm.h"
#include "nwoloader/nwo_runner.h"
#include "installer.h"
#include "Apps/Games/slot.h"
#include "time.h"
#include "Apps/Games/game_memory.h"
#include "Apps/Games/snake.h"
#include "kernel.h"
#include "../../compiler/nwc_kernel.h"
#include "memory/pmm.h"
#include "random/word_module.h"
#include "random/entropy.h"
#include "random/passage_module.h"
#include "graphics.h"
#include "memory/mem.h"
#include "Apps/fileStats/filestats.h"
#include "../../drivers/usb/xhci.h"
#include "Apps/baseconverter/baseconverter.h"
#include "rootfs_data.h"

#include "package.h"
#include "words_data.h"


#include "network/net.h"

#include "nwo_loader.h"

#include "network/timer.h"
#include "zones.h"

#include "../../include/LogoNwOS.h"

#include "font.h"
#include "char_symbol.h"
#include "utf8.h"

#include "fb.h"
#include "video_config.h"

#include <stdint.h>

void sleep(unsigned int ms)
{
    if (ms == 0) return;

    unsigned long long idle_start = timer_us();
    unsigned long long start = timer_ms();
    unsigned long long target = start + ms;

    while (timer_ms() < target)
    {
        __asm__ volatile ("pause");
    }

    idle_us_accum += timer_us() - idle_start;
}