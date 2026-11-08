#include "nwos_internal.h"

int root_password_is_set(void)
{
    return cfg_root_salt[0] != '\0' &&
           cfg_root_hash[0] != '\0';
}

int verify_root_password(const char* password)
{
    if (!password || !root_password_is_set())
        return 0;

    char hash[65];
    unsigned int diff = 0;

    nw_password_hash(password, cfg_root_salt, hash);

    for (int i = 0; i < 64; i++)
        diff |= (unsigned char)hash[i] ^
                (unsigned char)cfg_root_hash[i];

    for (int i = 0; i < 65; i++)
        hash[i] = '\0';

    return diff == 0;
}
void wipe_secret(char* secret, int size)
{
    volatile char* p = secret;

    for (int i = 0; i < size; i++)
        p[i] = '\0';
}

int authenticate_root_password(void)
{
    if (!root_password_is_set())
    {
        print_error(
            "Root password is not configured. Run passwd first.\n"
        );
        return 0;
    }

    for (int attempt = 0; attempt < 3; attempt++)
    {
        char password[64];

        print("Root password: ");
        read_password(password, sizeof(password));

        int valid = verify_root_password(password);
        wipe_secret(password, sizeof(password));

        if (valid)
        {
            print_success("Authentication successful.\n");
            return 1;
        }

        print_error("Incorrect password.\n");
        sleep(750);
    }

    print_error("Authentication failed.\n");
    return 0;
}

int authorize_sensitive_action(
    const char* description,
    const char* confirmation
)
{
    print_error("\nWARNING: dangerous operation!\n");
    print(description);
    print("\nThis action may make NwOS unusable.\n");

    if (!cmd_sudo && !authenticate_root_password())
        return 0;

    print("To confirm, type exactly: ");
    print(confirmation);
    print("\n> ");

    char answer[40];
    read_line(answer, sizeof(answer));

    if (strcmp(answer, confirmation) != 0)
    {
        print("Operation cancelled.\n");
        return 0;
    }

    return 1;
}

void cmd_passwd(void)
{
    if (root_password_is_set() &&
        !authenticate_root_password())
    {
        return;
    }

    char password[64];
    char confirm[64];

    for (int attempt = 0; attempt < 3; attempt++)
    {
        print("New root password: ");

        int length = read_password(password, sizeof(password));

        print("Confirm new password: ");

        int confirm_length =
            read_password(confirm, sizeof(confirm));

        if (length != confirm_length ||
            strcmp(password, confirm) != 0)
        {
            wipe_secret(password, sizeof(password));
            wipe_secret(confirm, sizeof(confirm));

            print_error("Passwords do not match.\n");
            continue;
        }

        make_root_salt(cfg_root_salt);
        nw_password_hash(password, cfg_root_salt, cfg_root_hash);

        wipe_secret(password, sizeof(password));
        wipe_secret(confirm, sizeof(confirm));

        cfg_setup_complete = 1;

        settings_save();

        print_success("Root password updated.\n");
        return;
    }

    print_error("Password was not changed.\n");
}
