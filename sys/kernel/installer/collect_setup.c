#include "nwos_internal.h"

int installer_collect_setup(void)
{
    char input[64];
    char password[64];
    char confirm[64];
    char old_user[32];
    char old_hostname[64];
    char old_salt[17];
    char old_hash[65];
    char old_invite[28];
    int old_shell_p = shell_p;
    int old_setup_complete = cfg_setup_complete;
    int old_require_login = cfg_require_login;
    int w = 700, h = 500;
    int x = ((int)fb_get_width() - w) / 2;
    int y = ((int)fb_get_height() - h) / 2;

    cfg_strcpy(old_user, cfg_user, sizeof(old_user));
    cfg_strcpy(old_hostname, cfg_hostname, sizeof(old_hostname));
    cfg_strcpy(old_salt, cfg_root_salt, sizeof(old_salt));
    cfg_strcpy(old_hash, cfg_root_hash, sizeof(old_hash));
    cfg_strcpy(old_invite, invite_user, sizeof(old_invite));

    gui_draw_text(x + 202, y + 580,
                  "copyright (c) User0140155 2026", COLOR_WHITE);
    gui_draw_text(x + 276, y + 595,
                  "NwOS", COLOR_WHITE);

    while (1)
    {
        if (!installer_read_setup_field(
                "Account setup - Hostname",
                "Hostname [nwos]:", input, sizeof(input),
                "Use lowercase letters, digits and hyphens."))
            goto cancel_setup;

        gui_draw_text(x + 202, y + 580,
                    "copyright (c) User0140155 2026", COLOR_WHITE);
        gui_draw_text(x + 276, y + 595,
                    "NwOS", COLOR_WHITE);

        if (input[0] == '\0')
        {
            cfg_strcpy(cfg_hostname, "nwos", sizeof(cfg_hostname));
            break;
        }
        if (normalize_hostname(input))
        {
            cfg_strcpy(cfg_hostname, input, sizeof(cfg_hostname));
            break;
        }
        installer_setup_error("Invalid hostname. Use letters, digits and '-'.");
    }

    while (1)
    {
        gui_draw_text(x + 202, y + 580,
                    "copyright (c) User0140155 2026", COLOR_WHITE);
        gui_draw_text(x + 276, y + 595,
                    "NwOS", COLOR_WHITE);
        if (!installer_read_setup_field(
                "Account setup - Username",
                "Username [user]:", input, sizeof(input),
                "1-31 characters: a-z, 0-9, underscore or hyphen; root is reserved."))
            goto cancel_setup;

        if (input[0] == '\0')
        {
            cfg_strcpy(cfg_user, "user", sizeof(cfg_user));
            break;
        }
        if (normalize_username(input))
        {
            cfg_strcpy(cfg_user, input, sizeof(cfg_user));
            break;
        }
        installer_setup_error("Invalid username. Use 1-31 valid characters; root is reserved.");
    }

    while (1)
    {
        gui_draw_text(x + 202, y + 580,
                    "copyright (c) User0140155 2026", COLOR_WHITE);
        gui_draw_text(x + 276, y + 595,
                    "NwOS", COLOR_WHITE);
        if (!installer_read_secret_field(
                "Account setup - Password",
                "Account / administrator password:", password,
                sizeof(password),
                "The password is stored as a salted hash."))
        {
            wipe_secret(password, sizeof(password));
            goto cancel_setup;
        }

        if (password[0] == '\0')
        {
            installer_setup_error("Password cannot be empty.");
            continue;
        }

        if (!installer_read_secret_field(
                "Account setup - Confirm password",
                "Confirm password:", confirm, sizeof(confirm),
                "Re-enter the same password."))
        {
            wipe_secret(password, sizeof(password));
            wipe_secret(confirm, sizeof(confirm));
            goto cancel_setup;
        }

        if (strcmp(password, confirm) == 0)
            break;

        wipe_secret(password, sizeof(password));
        wipe_secret(confirm, sizeof(confirm));
        installer_setup_error("Passwords do not match. Enter them again.");
    }

    if (!installer_choose_login_requirement())
    {
        wipe_secret(password, sizeof(password));
        wipe_secret(confirm, sizeof(confirm));
        goto cancel_setup;
    }

    make_root_salt(cfg_root_salt);
    nw_password_hash(password, cfg_root_salt, cfg_root_hash);
    if (!verify_root_password(password))
    {
        wipe_secret(password, sizeof(password));
        wipe_secret(confirm, sizeof(confirm));
        installer_setup_error("Password verification failed internally. Please enter it again.");
        goto cancel_setup;
    }
    wipe_secret(password, sizeof(password));
    wipe_secret(confirm, sizeof(confirm));

    cfg_setup_complete = 1;
    shell_p = 4;

    fb_clear(COLOR_BLUE);
    {
        int w = 700;
        int h = 500;
        int x = ((int)fb_get_width() - w) / 2;
        int y = ((int)fb_get_height() - h) / 2;
        char prompt_preview[128];
        int prompt_pos = 0;

        prompt_preview[0] = '\0';
        prompt_append(prompt_preview, sizeof(prompt_preview), &prompt_pos, cfg_user);
        prompt_append(prompt_preview, sizeof(prompt_preview), &prompt_pos, "@");
        prompt_append(prompt_preview, sizeof(prompt_preview), &prompt_pos, cfg_hostname);
        prompt_append(prompt_preview, sizeof(prompt_preview), &prompt_pos, ":~/path$ ");

        gui_draw_window(x, y, w, h, "Account setup complete");
        gui_draw_text(x + 202, y + 580,
                    "copyright (c) User0140155 2026", COLOR_WHITE);
        gui_draw_text(x + 276, y + 595,
                    "NwOS", COLOR_WHITE);
        gui_draw_text(x + 24, y + 65, "Hostname:", COLOR_DARK_GRAY);
        gui_draw_text(x + 160, y + 65, cfg_hostname, COLOR_BLACK);
        gui_draw_text(x + 24, y + 100, "Username:", COLOR_DARK_GRAY);
        gui_draw_text(x + 160, y + 100, cfg_user, COLOR_BLACK);
        gui_draw_text(x + 24, y + 145, "Shell prompt:", COLOR_DARK_GRAY);
        gui_draw_text(x + 160, y + 145, prompt_preview, COLOR_GREEN);
        gui_draw_text(x + 24, y + 185, "Startup login:", COLOR_DARK_GRAY);
        gui_draw_text(x + 160, y + 185,
                      cfg_require_login ? "Enabled" : "Disabled",
                      cfg_require_login ? COLOR_GREEN : COLOR_BLACK);
        gui_draw_text(x + 24, y + h - 42,
                      "Press any key to continue to disk selection.",
                      COLOR_DARK_GRAY);
    }
    (void)keyboard_getkey();
    wipe_secret(old_salt, sizeof(old_salt));
    wipe_secret(old_hash, sizeof(old_hash));
    return 1;

cancel_setup:
    cfg_strcpy(cfg_user, old_user, sizeof(cfg_user));
    cfg_strcpy(cfg_hostname, old_hostname, sizeof(cfg_hostname));
    cfg_strcpy(cfg_root_salt, old_salt, sizeof(cfg_root_salt));
    cfg_strcpy(cfg_root_hash, old_hash, sizeof(cfg_root_hash));
    cfg_strcpy(invite_user, old_invite, sizeof(invite_user));
    shell_p = old_shell_p;
    cfg_setup_complete = old_setup_complete;
    cfg_require_login = old_require_login;
    wipe_secret(old_salt, sizeof(old_salt));
    wipe_secret(old_hash, sizeof(old_hash));
    wipe_secret(password, sizeof(password));
    wipe_secret(confirm, sizeof(confirm));
    return 0;
}
