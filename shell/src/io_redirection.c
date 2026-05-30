// ############## LLM Generated Code Begins ##############
#include "../include/io_redirection.h"
#include "../include/reveal.h"
#include "../include/hop.h"
#include "../include/activities.h"
#include "../include/ping.h"
#include "../include/command_execution.h"  // for foreground tracking
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

extern pid_t shell_pgid;

// Sequentially process redirections left-to-right. Abort on first failure.
// Build cleaned argv (no redirection tokens) in place.
static int process_redirections(char **argv) {
    int w = 0; // write index for cleaned argv
    for (int r = 0; argv[r]; r++) {
        char *tok = argv[r];
        if (strcmp(tok, "<") == 0) {
            if (!argv[r+1]) { printf("No such file or directory!\n"); return -1; }
            int fd = open(argv[r+1], O_RDONLY);
            if (fd < 0) { printf("No such file or directory!\n"); return -1; }
            if (dup2(fd, STDIN_FILENO) < 0) { close(fd); printf("No such file or directory!\n"); return -1; }
            close(fd);
            r++; // skip filename
            continue;
        } else if (strcmp(tok, ">") == 0 || strcmp(tok, ">>") == 0) {
            int is_append = (tok[1] == '>');
            if (!argv[r+1]) { printf("Unable to create file for writing\n"); return -1; }
            int flags = O_WRONLY | O_CREAT | (is_append ? O_APPEND : O_TRUNC);
            int fd = open(argv[r+1], flags, 0666);
            if (fd < 0) { printf("Unable to create file for writing\n"); return -1; }
            if (dup2(fd, STDOUT_FILENO) < 0) { close(fd); printf("Unable to create file for writing\n"); return -1; }
            close(fd);
            r++;
            continue;
        } else if (tok[0] == '<') { // compact form <file
            if (!tok[1]) { printf("No such file or directory!\n"); return -1; }
            int fd = open(tok+1, O_RDONLY);
            if (fd < 0) { printf("No such file or directory!\n"); return -1; }
            if (dup2(fd, STDIN_FILENO) < 0) { close(fd); printf("No such file or directory!\n"); return -1; }
            close(fd);
            continue;
        } else if (tok[0] == '>') { // >file or >>file
            int is_append = 0;
            const char *name = NULL;
            if (tok[1] == '>') { is_append = 1; name = tok+2; }
            else name = tok+1;
            if (!name || !*name) { printf("Unable to create file for writing\n"); return -1; }
            int flags = O_WRONLY | O_CREAT | (is_append ? O_APPEND : O_TRUNC);
            int fd = open(name, flags, 0666);
            if (fd < 0) { printf("Unable to create file for writing\n"); return -1; }
            if (dup2(fd, STDOUT_FILENO) < 0) { close(fd); printf("Unable to create file for writing\n"); return -1; }
            close(fd);
            continue;
        } else {
            argv[w++] = tok; // keep normal arg
        }
    }
    argv[w] = NULL;
    return 0;
}

void execute_with_redirection(char **argv) {
    if (!argv || !argv[0]) return;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        // Child: create new process group and reset signals
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        
        // Sequential redirection processing
        if (process_redirections(argv) < 0) {
            _exit(127);
        }

        // Builtins after redirection applied
        if (argv[0]) {
            if (strcmp(argv[0], "echo") == 0) {
                for (int i = 1; argv[i]; i++) { if (i > 1) printf(" "); printf("%s", argv[i]); }
                printf("\n");
                _exit(0);
            }
            if (strcmp(argv[0], "reveal") == 0) {
                const char *home_dir = getenv("HOME"); if (!home_dir) home_dir = "/";
                int argc = 0; while (argv[argc]) argc++;
                reveal_command(argc, argv, home_dir); _exit(0);
            }
            if (strcmp(argv[0], "hop") == 0) {
                const char *home_dir = getenv("HOME"); if (!home_dir) home_dir = "/";
                int argc = 0; while (argv[argc]) argc++;
                hop_command(argc, argv, home_dir); _exit(0);
            }
            if (strcmp(argv[0], "activities") == 0) { activities_command(); _exit(0); }
            if (strcmp(argv[0], "ping") == 0) { int argc=0; while(argv[argc]) argc++; ping_command(argc, argv); _exit(0); }
            if (strcmp(argv[0], "bg") == 0) { printf("No such job\n"); _exit(0); }
            if (strcmp(argv[0], "fg") == 0) { printf("No such job\n"); _exit(0); }
        }

        execvp(argv[0], argv);
        printf("Command not found!\n");
        _exit(127);
    } else {
        // Parent: track foreground process
        setpgid(pid, pid);
        set_foreground_pid(pid);
        tcsetpgrp(STDIN_FILENO, pid);
        int status; (void)waitpid(pid, &status, 0);
        clear_foreground_pid();
        tcsetpgrp(STDIN_FILENO, shell_pgid);
    }
}
// ############## LLM Generated Code Ends ##############
