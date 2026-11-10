#include "nwos_internal.h"

void installer_profile_take(installer_profile_snapshot_t* snapshot)
{
    if (!snapshot) return;
    cfg_strcpy(snapshot->user, cfg_user, sizeof(snapshot->user));
    cfg_strcpy(snapshot->hostname, cfg_hostname, sizeof(snapshot->hostname));
    cfg_strcpy(snapshot->root_salt, cfg_root_salt, sizeof(snapshot->root_salt));
    cfg_strcpy(snapshot->root_hash, cfg_root_hash, sizeof(snapshot->root_hash));
    cfg_strcpy(snapshot->invite, invite_user, sizeof(snapshot->invite));
    snapshot->shell_mode = shell_p;
    snapshot->setup_complete = cfg_setup_complete;
    snapshot->require_login = cfg_require_login;
}

void installer_profile_restore(installer_profile_snapshot_t* snapshot)
{
    if (!snapshot) return;
    cfg_strcpy(cfg_user, snapshot->user, sizeof(cfg_user));
    cfg_strcpy(cfg_hostname, snapshot->hostname, sizeof(cfg_hostname));
    cfg_strcpy(cfg_root_salt, snapshot->root_salt, sizeof(cfg_root_salt));
    cfg_strcpy(cfg_root_hash, snapshot->root_hash, sizeof(cfg_root_hash));
    cfg_strcpy(invite_user, snapshot->invite, sizeof(invite_user));
    shell_p = snapshot->shell_mode;
    cfg_setup_complete = snapshot->setup_complete;
    cfg_require_login = snapshot->require_login;
    wipe_secret(snapshot->root_salt, sizeof(snapshot->root_salt));
    wipe_secret(snapshot->root_hash, sizeof(snapshot->root_hash));
}

int installer_confirm_preflight(void)
{
    while (1)
    {
        installer_draw_preflight_warning();
        int key = keyboard_getkey();
        if (key == KEY_ENTER || key == 'y' || key == 'Y') return 1;
        if (key == KEY_ESC || key == 27 || key == 'n' || key == 'N') return 0;
    }
}

int installer_verify_saved_account(void)
{
    char buffer[SETTINGS_MAX];
    char value[80];
    int previous_dir = current_dir;
    int sys_dir = -1;
    int ok = 0;

    for (int i = 0; i < fs_dir_cap; ++i)
    {
        if (directory[i].used && directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 && strcmp(directory[i].name, "sys") == 0)
        {
            sys_dir = i;
            break;
        }
    }
    if (sys_dir < 0) return 0;

    current_dir = (uint16_t)sys_dir;
    if (!fs_read_text(SETTINGS_FILE, buffer, sizeof(buffer)))
        goto done;

    if (!cfg_find_key(buffer, "name", value, sizeof(value)) || strcmp(value, cfg_user) != 0)
        goto done;
    if (!cfg_find_key(buffer, "hostname", value, sizeof(value)) || strcmp(value, cfg_hostname) != 0)
        goto done;
    if (!cfg_find_key(buffer, "root_salt", value, sizeof(value)) || strcmp(value, cfg_root_salt) != 0)
        goto done;
    if (!cfg_find_key(buffer, "root_hash", value, sizeof(value)) || strcmp(value, cfg_root_hash) != 0)
        goto done;
    if (!cfg_find_key(buffer, "require_login", value, sizeof(value)) ||
        ((strcmp(value, "1") == 0) != (cfg_require_login != 0)))
        goto done;

    ok = 1;
done:
    current_dir = (uint16_t)previous_dir;
    wipe_secret(value, sizeof(value));
    wipe_secret(buffer, sizeof(buffer));
    return ok;
}

void cmd_install(void)
{
    if (root_password_is_set() &&
        !cmd_sudo &&
        !authenticate_root_password())
    {
        return;
    }
    installer_cancelled = 0;
    installer_completed = 0;

    installer();

    if (installer_completed)
    {
        int status = fs_load_status();
        if (status == 1)
            print_success("Installation verified and detected.\n");
        else
            print_error("Install finished, but the boot scan cannot detect the new filesystem.\n");
    }
    else if (installer_cancelled)
    {
        print("Installation cancelled. The system keeps running.\n");
    }
}
