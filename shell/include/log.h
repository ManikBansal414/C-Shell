#ifndef LOG_H
#define LOG_H

#include <stddef.h>

// Call once at startup with the shell's home_dir.
// (History file stored at <home_dir>/.shell_log)
void log_init(const char *home_dir);

// Try to add a command to history (applies all rules: size 15, skip duplicates,
// skip if any atomic command has name "log"). Pass the ORIGINAL shell_cmd line.
void log_maybe_add(const char *shell_cmd);

// Print history oldest -> newest (one line per entry)
void log_print(void);

// Clear the history
void log_purge(void);

// Handle "log" command:
// - argv: tokenized input (argv[0] == "log")
// - If it's "log execute <index>", this fills out_exec with the command to run
//   (newest=1, oldest=history_size). Returns 1 in that case.
// - Returns 0 for "log" (print) and "log purge" (cleared) or invalid forms.
// - out_exec is untouched if return 0.
// IMPORTANT: Caller must NOT store out_exec back into history.
int log_command(int argc, char *argv[], char *out_exec, size_t out_exec_sz);

#endif
