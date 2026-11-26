#include "nwos_internal.h"
#include "network/e1000.h"
#include "network/rtl8139.h"
#include "network/rtl8169.h"
#include "network/ne2k.h"
#include "network/pcnet.h"
#include "network/rtl8822.h"

static unsigned long long boot_t0_ms;

static void boot_rule(void)
{
    set_color(TERM_COLOR_LIGHT_CYAN);
    print("  --------------------------------------------------------------------\n");
    set_color(base_color);
}

static void boot_banner(void)
{
    set_color(TERM_COLOR_LIGHT_CYAN);
    print("\n");
    boot_rule();
    set_color(TERM_COLOR_WHITE);
    print("                        " OS_NAME " " OS_VERSION_STR " x86_64\n");
    set_color(TERM_COLOR_LIGHT_GRAY);
    print("                     detailed kernel boot & diagnostics\n");
    set_color(TERM_COLOR_LIGHT_CYAN);
    boot_rule();
    set_color(base_color);
}

static void boot_stage(const char *name)
{
    set_color(TERM_COLOR_YELLOW);
    print("\n  [STAGE] ");
    set_color(TERM_COLOR_WHITE);
    print(name);
    print("\n");
    set_color(base_color);
}

static void boot_kv(const char *k, const char *v, unsigned char vc)
{
    set_color(TERM_COLOR_DARK_GRAY);
    print("    * ");
    set_color(TERM_COLOR_CYAN);
    print(k);
    set_color(TERM_COLOR_LIGHT_GRAY);
    print(": ");
    set_color(vc);
    print(v ? v : "(null)");
    print("\n");
    set_color(base_color);
}

static void boot_kv_int(const char *k, int v)
{
    set_color(TERM_COLOR_DARK_GRAY);
    print("    * ");
    set_color(TERM_COLOR_CYAN);
    print(k);
    set_color(TERM_COLOR_LIGHT_GRAY);
    print(": ");
    set_color(TERM_COLOR_WHITE);
    print_int(v);
    print("\n");
    set_color(base_color);
}

static void boot_kv_u64_mb(const char *k, unsigned long long bytes)
{
    set_color(TERM_COLOR_DARK_GRAY);
    print("    * ");
    set_color(TERM_COLOR_CYAN);
    print(k);
    set_color(TERM_COLOR_LIGHT_GRAY);
    print(": ");
    set_color(TERM_COLOR_WHITE);
    print_int((int)(bytes / (1024ULL * 1024ULL)));
    print(" MiB\n");
    set_color(base_color);
}

static void boot_step(const char *label)
{
    set_color(TERM_COLOR_DARK_GRAY);
    print("    -> ");
    set_color(TERM_COLOR_LIGHT_GRAY);
    print(label);
    set_color(TERM_COLOR_DARK_GRAY);
    print(" ...\n");
    set_color(base_color);
}

static void boot_ok(const char *msg)
{
    set_color(TERM_COLOR_GREEN);
    print("       [OK]   ");
    set_color(TERM_COLOR_WHITE);
    print(msg);
    print("\n");
    set_color(base_color);
}

static void boot_warn(const char *msg)
{
    set_color(TERM_COLOR_YELLOW);
    print("       [WARN] ");
    set_color(TERM_COLOR_WHITE);
    print(msg);
    print("\n");
    set_color(base_color);
}

static void boot_fail(const char *msg)
{
    set_color(TERM_COLOR_RED);
    print("       [FAIL] ");
    set_color(TERM_COLOR_WHITE);
    print(msg);
    print("\n");
    set_color(base_color);
}

static void boot_progress(const char *label, unsigned int total_ms, int steps)
{
    if (steps < 1) steps = 1;
    set_color(TERM_COLOR_DARK_GRAY);
    print("    [");
    set_color(TERM_COLOR_LIGHT_BLUE);
    for (int i = 0; i < steps; i++)
        putchar_os('.');
    set_color(TERM_COLOR_DARK_GRAY);
    print("] ");
    set_color(TERM_COLOR_LIGHT_GRAY);
    print(label);
    print("\n");
    set_color(base_color);

    unsigned int per = total_ms / (unsigned int)steps;
    for (int i = 0; i < steps; i++)
    {
        set_color(TERM_COLOR_GREEN);
        print("      #");
        set_color(TERM_COLOR_DARK_GRAY);
        print(" step ");
        print_int(i + 1);
        print("/");
        print_int(steps);
        print("\n");
        set_color(base_color);
        if (per > 0) sleep(per);
    }
}

void init_print(const char *str)
{
    set_color(TERM_COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print(str);
}

void pre_boot_init(void)
{
    timer_init();
    boot_t0_ms = timer_ms();

    clear();
    boot_banner();

    boot_stage("Early boot: timers, entropy, and input discovery");

    boot_step("Calibrate PIT/TSC timer");
    timer_init();
    boot_ok("timer initialized (TSC calibrated via PIT)");

    boot_step("Initialize entropy sources");
    nw_entropy_init();
    boot_ok("entropy pool mixed from available sources");

    boot_step("Probe PS/2 keyboard controller");
    set_color(TERM_COLOR_DARK_GRAY);
    print("       note: legacy PS/2 probing may take a while on some systems\n");
    set_color(base_color);
    keyboard_init();
    if (keyboard_ps2_init_responded())
    {
        if (keyboard_ps2_seen())
            boot_ok("PS/2 controller responded and scan data observed");
        else
            boot_ok("PS/2 controller responded (no scan data yet)");
    }
    else
    {
        boot_warn("PS/2 controller did not respond (USB keyboard may still work)");
    }
    boot_kv_int("PS/2 init responded", keyboard_ps2_init_responded());
    boot_kv_int("PS/2 scan data seen", keyboard_ps2_seen());

    boot_step("Query USB xHCI subsystem state");
    {
        const char *xs = xhci_status_string();
        boot_kv("xHCI status", (xs && xs[0]) ? xs : "unavailable", TERM_COLOR_WHITE);
        boot_kv_int("xHCI BIOS handoff released", xhci_bios_released());
        if (xs && xs[0]) boot_ok("xHCI status captured");
        else boot_warn("xHCI status unavailable (controller may be absent)");
    }

    boot_progress("preparing boot menu handoff", 120, 4);
}

void startingNWOS(void)
{
    int drive_count;
    char dims[24];

    boot_stage("Core subsystems: display, memory, and services");

    boot_step("Initialize framebuffer / text console backend");
    fb_init();
    if (fb_is_available())
    {
        int w = (int)fb_get_width();
        int h = (int)fb_get_height();
        /* build WxH string */
        {
            int i = 0;
            int tmp = w, len = 0; char rev[8];
            if (tmp == 0) rev[len++] = '0';
            while (tmp > 0 && len < 7) { rev[len++] = (char)('0' + (tmp % 10)); tmp /= 10; }
            while (len > 0 && i < 23) dims[i++] = rev[--len];
            if (i < 23) dims[i++] = 'x';
            tmp = h; len = 0;
            if (tmp == 0) rev[len++] = '0';
            while (tmp > 0 && len < 7) { rev[len++] = (char)('0' + (tmp % 10)); tmp /= 10; }
            while (len > 0 && i < 23) dims[i++] = rev[--len];
            dims[i] = 0;
        }
        boot_kv("Framebuffer resolution", dims, TERM_COLOR_WHITE);
        boot_ok("framebuffer ready");
    }
    else
    {
        boot_fail("framebuffer not available");
    }

    boot_step("Initialize timer service");
    timer_init();
    boot_ok("timer service ready");

    boot_step("Physical memory manager (PMM)");
    if (pmm_is_ready())
    {
        boot_ok("PMM ready (identity-mapped physical memory available)");
        boot_kv_u64_mb("Total RAM", pmm_total_bytes());
        boot_kv_u64_mb("Free RAM", pmm_total_free_bytes());
    }
    else
    {
        boot_warn("PMM not ready; memory reporting/features may be limited");
    }

    boot_step("Seed kernel random number generator");
    random_init();
    boot_ok("RNG initialized");

    boot_step("Load shell command history");
    history_load();
    boot_ok("history subsystem initialized");

    boot_stage("Storage: controllers, disks, and filesystem");

    boot_step("Initialize ATA/AHCI storage controllers");
    ata_init();
    drive_count = ata_drive_count();
    if (drive_count > 0)
        boot_ok("storage scan complete");
    else
        boot_warn("no ATA/AHCI drives detected");
    boot_kv_int("Detected drives", drive_count);

    for (int i = 0; i < drive_count && i < 8; i++)
    {
        ata_drive_t *d = ata_get_drive(i);
        if (!d || !d->exists) continue;
        set_color(TERM_COLOR_DARK_GRAY);
        print("       drive ");
        print_int(i);
        print(": ");
        set_color(TERM_COLOR_WHITE);
        putchar_os(d->letter ? d->letter : '?');
        print(": ");
        print(d->model[0] ? d->model : "(unknown model)");
        set_color(TERM_COLOR_DARK_GRAY);
        print(" | sectors=");
        print_int((int)d->sectors);
        print(" | type=");
        print(d->type == DISK_TYPE_AHCI ? "AHCI" : "ATA/PIO");
        print("\n");
        set_color(base_color);
    }

    boot_step("Mount/load NwOS filesystem structures");
    fs_load();
    boot_ok("filesystem layer initialized");

    boot_stage("Input devices: keyboard, mouse, and touchpad");

    boot_step("Finalize keyboard input paths (PS/2 + USB HID/xHCI)");
    boot_kv("PS/2", keyboard_ps2_seen() ? "active" : "no scan data", TERM_COLOR_WHITE);
    {
        const char *xs = xhci_status_string();
        boot_kv("xHCI", (xs && xs[0]) ? xs : "n/a", TERM_COLOR_WHITE);
    }
    boot_ok("keyboard subsystem ready");

    boot_step("Initialize pointing devices");
    mouse_init();
    boot_ok(mouse_is_touchpad() ? "touchpad detected" : "mouse subsystem initialized");
    boot_kv("Pointer device", mouse_device_name() ? mouse_device_name() : "unknown", TERM_COLOR_WHITE);

    boot_stage("Networking: wired NIC drivers and Wi-Fi PCI init");

    boot_step("Probe Intel e1000 driver");
    {
        int r = e1000_driver.probe();
        if (r == 0) boot_ok("Intel e1000 initialized");
        else boot_warn("Intel e1000 not present/probe failed");
    }

    boot_step("Probe Realtek RTL8169/8168/8111 driver");
    {
        int r = rtl8169_driver.probe();
        if (r == 0) boot_ok("Realtek RTL8169/8168 family initialized");
        else boot_warn("Realtek RTL8169/8168 family not present/probe failed");
    }

    boot_step("Probe Realtek RTL8139 driver");
    {
        int r = rtl8139_driver.probe();
        if (r == 0) boot_ok("Realtek RTL8139 initialized");
        else boot_warn("Realtek RTL8139 not present/probe failed");
    }

    boot_step("Probe NE2000 PCI (RTL8029) driver");
    {
        int r = ne2k_driver.probe();
        if (r == 0) boot_ok("NE2000 PCI (RTL8029) initialized");
        else boot_warn("NE2000 PCI not present/probe failed");
    }

    boot_step("Probe AMD PCnet-PCI II driver");
    {
        int r = pcnet_driver.probe();
        if (r == 0) boot_ok("AMD PCnet-PCI II initialized");
        else boot_warn("AMD PCnet not present/probe failed");
    }

    boot_step("Initialize Realtek RTL8822 Wi-Fi PCI driver");
    {
        int r = rtl8822_driver.probe();
        if (rtl8822_present())
        {
            boot_kv("RTL8822 device", rtl8822_detected_name(), TERM_COLOR_WHITE);
            if (r == 0 && rtl8822_initialized())
                boot_ok("RTL8822 PCI function enabled/MMIO mapped (firmware+802.11 stack required for Wi-Fi)");
            else
                boot_warn("RTL8822 detected but PCI/MMIO init incomplete");
        }
        else
        {
            boot_kv("RTL8822", "not detected", TERM_COLOR_DARK_GRAY);
            boot_ok("no RTL8822 Wi-Fi adapter found");
        }
    }

    boot_kv("Network note", "use `net`, `lspci`, `ping 10.0.2.2` in shell", TERM_COLOR_LIGHT_GRAY);

    boot_stage("User environment: shell runtime");

    boot_step("Prepare interactive shell runtime");
    boot_kv("Shell", "NwSH user-space UI loop", TERM_COLOR_WHITE);
    boot_kv("Syscall interface", "int 0x80 (prompt/read/exec/write)", TERM_COLOR_WHITE);
    boot_kv("Kernel role", "services, drivers, filesystem, command execution", TERM_COLOR_LIGHT_GRAY);
    boot_ok("interactive shell runtime prepared");

    {
        unsigned long long elapsed = timer_ms() - boot_t0_ms;
        boot_progress("finalizing boot environment", 160, 5);
        set_color(TERM_COLOR_LIGHT_CYAN);
        print("\n  Boot sequence complete");
        set_color(TERM_COLOR_DARK_GRAY);
        print(" in ");
        print_int((int)elapsed);
        print(" ms.\n");
        set_color(base_color);
    }

    sleep(300);
}
