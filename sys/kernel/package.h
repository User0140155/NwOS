#ifndef PACKAGE_H
#define PACKAGE_H

void package_handler(const char* args);
int package_run_command(const char* command);
void package_scan_shell_exports(void);
void package_list_shell_commands(void);
int package_run_shell_command(const char* command);

#endif
