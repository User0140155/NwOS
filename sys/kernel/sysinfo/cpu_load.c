#include "nwos_internal.h"

int nwf_tz_get(void)
{
    return timezone_offset;
}

void nwf_tz_set(int hours)
{
    if (hours < -12) hours = -12;
    if (hours > 14)  hours = 14;
    timezone_offset = hours;
}

int nwf_cpu_load(void)
{
    static unsigned long long prev_total = 0;
    static unsigned long long prev_idle  = 0;
    unsigned long long total = timer_us();
    unsigned long long idle  = idle_us_accum;
    unsigned long long dt;
    unsigned long long di;
    int load;

    if (prev_total == 0)
    {
        prev_total = total;
        prev_idle  = idle;
        return 0;
    }

    dt = total - prev_total;
    di = idle - prev_idle;
    prev_total = total;
    prev_idle  = idle;

    if (dt == 0)
        return 0;
    if (di > dt)
        di = dt;

    load = (int)(((dt - di) * 100ULL) / dt);
    if (load < 0) load = 0;
    if (load > 100) load = 100;
    return load;
}

int nwf_gpu_load(void)
{
    static unsigned long long prev_total = 0;
    static unsigned long long prev_gpu   = 0;
    unsigned long long total = timer_us();
    unsigned long long gpu   = fb_get_present_us();
    unsigned long long dt;
    unsigned long long dg;
    int load;

    if (prev_total == 0)
    {
        prev_total = total;
        prev_gpu   = gpu;
        return 0;
    }

    dt = total - prev_total;
    dg = gpu - prev_gpu;
    prev_total = total;
    prev_gpu   = gpu;

    if (dt == 0)
        return 0;
    if (dg > dt)
        dg = dt;

    load = (int)((dg * 100ULL) / dt);
    if (load < 0) load = 0;
    if (load > 100) load = 100;
    return load;
}

unsigned long kernel_static_memory_kb(void)
{
    unsigned long long used = 0;

    used += (unsigned long long)sizeof(line_buffer);
    used += (unsigned long long)sizeof(cmd_history);
    used += (unsigned long long)sizeof(directory);
    used += (unsigned long long)sizeof(fs_removed);
    used += (unsigned long long)sizeof(fs_scratch);

    if (fs_ram_mode)
    {
        unsigned long long fs_used =
            (unsigned long long)FS_DIR_SECTORS * 512ULL;

        for (int i = 0; i < MAX_FILES; i++)
        {
            if (directory[i].used && directory[i].type == FS_TYPE_FILE)
                fs_used += (unsigned long long)FS_FILE_SECTORS * 512ULL;
        }

        used += fs_used;
    }

    return (unsigned long)(used / 1024ULL);
}
