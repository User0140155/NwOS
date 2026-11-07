#ifndef INSTALLER_H
#define INSTALLER_H

typedef struct
{
    char user[32];
    char hostname[64];
    char root_salt[17];
    char root_hash[65];
    char invite[28];
    int shell_mode;
    int setup_complete;
    int require_login;
} installer_profile_snapshot_t;

extern int installer_cancelled;
extern int installer_completed;

void installer(void);
void installer_draw_preflight_warning(void);

int installer_collect_setup(void);
int installer_confirm_preflight(void);

void installer_profile_take(installer_profile_snapshot_t* snapshot);
void installer_profile_restore(installer_profile_snapshot_t* snapshot);
void installer_profile_discard(installer_profile_snapshot_t* snapshot);

int installer_verify_saved_account(void);

#endif
