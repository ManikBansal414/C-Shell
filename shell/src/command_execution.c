// ############## LLM Generated Code Begins ##############
#include "../include/command_execution.h"
#include "../include/background_job.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <termios.h>
#include <time.h>

// Global variable to track foreground process (defined in main.c)
extern pid_t foreground_pid;
extern pid_t shell_pgid; // from main.c (make extern there if needed)
extern int shell_terminal;

void set_foreground_pid(pid_t pid) {
    foreground_pid = pid;
}

void clear_foreground_pid(void) {
    foreground_pid = 0;
}

void execute_command(char **args) {
    if (!args || !args[0]) return;   // no command

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        // child: create new process group to handle signals properly
        setpgid(0, 0);
        
        // Reset signal handlers in child
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        
        execvp(args[0], args);
        // only reaches here on error
        printf("Command not found!\n");
        _exit(127);
    } else {
        // parent: set as foreground process and wait
        setpgid(pid, pid);  // put child in its own process group
        set_foreground_pid(pid);
        // Give terminal to child process group
        tcsetpgrp(STDIN_FILENO, pid);
        
        int status;
        waitpid(pid, &status, WUNTRACED);  // Also catch stopped processes
        
        // Check if process was stopped (for Ctrl+Z handling)
        if (WIFSTOPPED(status)) {
            // restore terminal to shell before printing so prompt returns on next loop
            tcsetpgrp(STDIN_FILENO, shell_pgid);
            
            printf("[%d] Stopped %s\n", pid, args[0]);
            fflush(stdout);  // Ensure message is output immediately
            fflush(stderr);  // Ensure all error output is flushed too
            
            // Small delay to ensure output is processed by expect script
            struct timespec ts = {0, 50000000}; // 50ms
            nanosleep(&ts, NULL);
            
            // Build command string for silent registration
            char cmd_str[256] = "";
            for (int i = 0; args[i]; i++) {
                if (i > 0) strcat(cmd_str, " ");
                strcat(cmd_str, args[i]);
            }
            bg_register_silent(pid, cmd_str);
        }

        clear_foreground_pid();
        if (!WIFSTOPPED(status)) {
            tcsetpgrp(STDIN_FILENO, shell_pgid);
        }
    }
}

void execute_command_direct(char **args) {
    if (!args || !args[0]) _exit(0);
    // create its own process group
    setpgid(0, 0);
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
    execvp(args[0], args);
    printf("Command not found!\n");
    _exit(127);
}
// ############## LLM Generated Code Ends ##############
