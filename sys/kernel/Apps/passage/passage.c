#include "nwos_internal.h"

#define NW_BIBLE_PASSAGE_VERSES 8u

void nw_god_bible_passage(void)
{
    uint32_t total;
    uint32_t line_count;
    uint32_t start;

    if (!nw_passage_module_is_ready())
    {
        print("\n[NwPassage] Bible text module is not loaded.\n");
        print("Place the unchanged WEB verse-per-line text at:\n");
        print("  data/passages/world_english_bible.txt\n");
        return;
    }

    total = nw_passage_module_count();
    if (total == 0u)
    {
        print("\n[NwPassage] No usable verses were found.\n");
        return;
    }

    line_count = (total < NW_BIBLE_PASSAGE_VERSES) ? total : NW_BIBLE_PASSAGE_VERSES;
    start = (uint32_t)nw_god_random_bounded(total - line_count + 1u);

    print("\n=== NwOS RANDOM PASSAGE (WORLD ENGLISH BIBLE) ===\n\n");
    for (uint32_t i = 0; i < line_count; ++i)
    {
        const char *line = 0;
        uint32_t length = 0;

        if (!nw_passage_module_get(start + i, &line, &length))
            break;

        for (uint32_t j = 0; j < length; ++j)
            putchar_os(line[j]);
        putchar_os('\n');
    }
    putchar_os('\n');
}
