#ifndef COMMAND_EXECUTION_H
#define COMMAND_EXECUTION_H

#include <sys/types.h>

// Execute argv[] as a program (waits for it to finish)
// argv must be NULL-terminated (argv[n] == NULL)
void execute_command(char **args);

// Execute command replacing current process (no extra fork). Used for
// background job child processes so the registered PID is the one that
// actually runs the command.
void execute_command_direct(char **args);

// Functions to manage foreground process for signal handling
void set_foreground_pid(pid_t pid);
void clear_foreground_pid(void);

#endif
