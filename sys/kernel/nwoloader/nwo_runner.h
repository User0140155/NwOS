#ifndef NWO_RUNNER_H
#define NWO_RUNNER_H

void nwo_run(const char* filename);
int nwo_run_console(const char* filename);
int nwo_run_in_window(const char* filename, int x, int y, int w, int h);
int nwo_default_window(const char* filename, const char* title);
int nwo_program_is_gui(const char* filename);

#endif