#include "nwos_internal.h"

int login_prompt(void)
{
    if (!cfg_require_login)
        return 1;

    if (!root_password_is_set())
    {
        print_error("Login is enabled but no account password is configured.\n");
        return 0;
    }

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        char username[32] = {0};
        char password[64] = {0};
        int valid = 0;

        print("\n" OS_NAME " login: ");
        read_line(username, sizeof(username));
        if (strcmp(username, cfg_user) == 0)
        {
            print("Password: ");
            if (read_password(password, sizeof(password)) > 0)
                valid = verify_root_password(password);
        }

        wipe_secret(password, sizeof(password));
        wipe_secret(username, sizeof(username));

        if (valid)
        {
            print_success("Login successful.\n");
            return 1;
        }

        print_error("Login incorrect.\n");
        sleep(750);
    }

    print_error("Too many failed login attempts. Reboot to try again.\n");
    return 0;
}
