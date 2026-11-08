#include "nwos_internal.h"

static char fs_path_buf[256];
const char* fs_current_path(void)
{
    char stack[MAX_FILES][MAX_FILENAME];
    int depth = 0;
    int cur   = current_dir;
    while (cur != 0 && depth < MAX_FILES)
    {
        strcpy(stack[depth], directory[cur].name);
        depth++;
        cur = directory[cur].parent;
    }
    int pos = 0;
    fs_path_buf[pos++] = '/';
    for (int i = depth - 1; i >= 0; i--)
    {
        int len = 0; while (stack[i][len]) len++;
        if (pos + len + 2 >= 256) break;
        for (int j = 0; j < len; j++) fs_path_buf[pos++] = stack[i][j];
        if (i > 0) fs_path_buf[pos++] = '/';
    }
    fs_path_buf[pos] = '\0';
    return fs_path_buf;
}

void fs_enter_user_home(void)
{
    char home[80];

    if (!fs_build_home_path(home, sizeof(home)))
    {
        current_dir = 0;
        return;
    }

    int dir = fs_resolve_dir_path(home);

    if (dir < 0)
        dir = fs_ensure_dir_path(home);

    current_dir = (dir >= 0) ? (uint16_t)dir : 0;
}
