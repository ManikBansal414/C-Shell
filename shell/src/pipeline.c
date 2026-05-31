// ############## LLM Generated Code Begins ##############
#include "../include/pipeline.h"
#include "../include/reveal.h"
#include "../include/hop.h"
#include <sys/types.h>
extern pid_t shell_pgid;
#include "../include/activities.h"
#include "../include/ping.h"
#include "../include/log.h"
#include "../include/parser.h"
#include "../include/command_execution.h"  // for foreground tracking
#include "../include/background_job.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

#define MAX_STAGES 64
#define MAX_STAGE_ARGS 128

// --- remove redirection tokens in a *single stage* argv, and report last redirs ---
static void parse_stage_redirs(char **argv,
                               char **infile, char **outfile, int *append)
{
    *infile = NULL;
    *outfile = NULL;
    *append = 0;

    // find last redirections in this stage
    for (int i = 0; argv[i]; i++) {
        if (strcmp(argv[i], "<") == 0) {                     // "< file"
            if (argv[i+1]) { *infile = argv[i+1]; }
            i++;
        } else if (strcmp(argv[i], ">") == 0) {              // "> file"
            if (argv[i+1]) { *outfile = argv[i+1]; *append = 0; }
            i++;
        } else if (strcmp(argv[i], ">>") == 0) {             // ">> file"
            if (argv[i+1]) { *outfile = argv[i+1]; *append = 1; }
            i++;
        } else if (argv[i][0] == '<') {                      // "<file"
            if (argv[i][1] != '\0') *infile = argv[i] + 1;
        } else if (argv[i][0] == '>') {                      // ">file" or ">>file"
            if (argv[i][1] == '>') {
                if (argv[i][2] != '\0') { *outfile = argv[i] + 2; *append = 1; }
            } else if (argv[i][1] != '\0') {
                *outfile = argv[i] + 1; *append = 0;
            }
        }
    }

    // compact argv[] to remove *all* redirection tokens (and filename after <,>,>>)
    int w = 0;
    for (int r = 0; argv[r]; r++) {
        if (strcmp(argv[r], "<") == 0 || strcmp(argv[r], ">") == 0 || strcmp(argv[r], ">>") == 0) {
            if (argv[r+1]) r++; // skip filename
            continue;
        }
        if (argv[r][0] == '<') continue;                     // skip "<file"
        if (argv[r][0] == '>' && argv[r][1] == '>') continue;// skip ">>file"
        if (argv[r][0] == '>' && argv[r][1] != '\0') continue;// skip ">file"
        argv[w++] = argv[r];
    }
    argv[w] = NULL;
}

// Split the full argv by '|' into stage argv arrays.
// Returns number of stages; each stage_argv[i] is NULL-terminated.
static int split_stages(char **argv, char *stage_argv[MAX_STAGES][MAX_STAGE_ARGS]) {
    int stage = 0, pos = 0;

    for (int i = 0; argv[i]; i++) {
        if (strcmp(argv[i], "|") == 0) {
            // end current stage
            if (stage >= MAX_STAGES) break;
            stage_argv[stage][pos] = NULL;
            stage++;
            pos = 0;
        } else {
            if (pos < MAX_STAGE_ARGS - 1) {
                stage_argv[stage][pos++] = argv[i];
            }
        }
    }
    stage_argv[stage][pos] = NULL;
    return stage + 1; // number of stages
}

void execute_with_pipes(char **argv) {
    // Build per-stage argv
    char *stage_argv[MAX_STAGES][MAX_STAGE_ARGS];
    int nstages = split_stages(argv, stage_argv);
    if (nstages <= 0) return;

    int pipes[MAX_STAGES - 1][2];
    for (int i = 0; i < nstages - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            // try to continue anyway; but without pipes it won't work properly
        }
    }

    pid_t pids[MAX_STAGES];

    pid_t pgid = 0;
    for (int i = 0; i < nstages; i++) {
        // parse redirections in *this stage* (mutates stage_argv[i])
        char *infile = NULL, *outfile = NULL;
        int append = 0;
        parse_stage_redirs(stage_argv[i], &infile, &outfile, &append);

        // empty stage guard (shouldn't happen with your parser)
        if (!stage_argv[i][0]) {
            // spawn a no-op child for consistency (or skip)
            pid_t dummy = fork();
            if (dummy == 0) _exit(0);
            pids[i] = dummy;
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            // still try to continue with others
            continue;
        }
        if (pid == 0) {
            // ---------- Child: wire up pipes ----------
            if (i == 0) {
                setpgid(0, 0); // leader creates new process group
            } else {
                setpgid(0, pgid); // join existing group
            }
            
            // Reset signal handlers in child
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGPIPE, SIG_DFL);
            
            // If not first stage: stdin <- read end of previous pipe
            if (i > 0) {
                if (dup2(pipes[i-1][0], STDIN_FILENO) < 0) _exit(127);
            }
            // If not last stage: stdout -> write end of this pipe
            if (i < nstages - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) _exit(127);
            }

            // Close all pipe fds in child
            for (int k = 0; k < nstages - 1; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }

            // ---------- Child: apply redirections for this stage ----------
            if (infile) {
                int fd_in = open(infile, O_RDONLY);
                if (fd_in < 0) {
                    // spec: for input open failure, print exact message & do not exec
                    printf("No such file or directory!\n");
                    _exit(127);
                }
                if (dup2(fd_in, STDIN_FILENO) < 0) { close(fd_in); _exit(127); }
                close(fd_in);
            }
            if (outfile) {
                int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
                int fd_out = open(outfile, flags, 0666);
                if (fd_out < 0) { perror("open failed"); _exit(127); }
                if (dup2(fd_out, STDOUT_FILENO) < 0) { close(fd_out); _exit(127); }
                close(fd_out);
            }

            // ---------- Exec this stage ----------
            // Check if it's a builtin command first
            if (strcmp(stage_argv[i][0], "reveal") == 0) {
                const char *home_dir = getenv("HOME");
                if (!home_dir) home_dir = "/";
                
                int argc = 0;
                while (stage_argv[i][argc]) argc++;
                
                reveal_command(argc, stage_argv[i], home_dir);
                _exit(0);
            }
            if (strcmp(stage_argv[i][0], "hop") == 0) {
                // hop inside a pipeline is a no-op (like POSIX shells); just exit success
                _exit(0);
            }
            if (strcmp(stage_argv[i][0], "ping") == 0) {
                int argc = 0;
                while (stage_argv[i][argc]) argc++;
                
                ping_command(argc, stage_argv[i]);
                _exit(0);
            }
            if (strcmp(stage_argv[i][0], "bg") == 0) {
                printf("No such job\n");
                _exit(0);
            }
            if (strcmp(stage_argv[i][0], "fg") == 0) { printf("No such job\n"); _exit(0); }
            if (strcmp(stage_argv[i][0], "echo") == 0) {
                for (int j = 1; stage_argv[i][j]; j++) {
                    if (j > 1) printf(" ");
                    printf("%s", stage_argv[i][j]);
                }
                printf("\n");
                _exit(0);
            }
            if (strcmp(stage_argv[i][0], "log") == 0) {
                // Handle log command in pipeline
                int argc = 0;
                while (stage_argv[i][argc]) argc++;
                
                char exec_buf[4096];
                int run = log_command(argc, stage_argv[i], exec_buf, sizeof(exec_buf));
                if (run) {
                    // For log execute in pipeline, execute the retrieved command
                    // Reset signal handlers before exec
                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
                    signal(SIGPIPE, SIG_DFL);
                    
                    // Use execl with /bin/sh to execute the command in current context
                    execl("/bin/sh", "sh", "-c", exec_buf, NULL);
                    // If execl fails
                    perror("execl");
                    _exit(127);
                } else {
                    // For log purge, no output needed
                }
                _exit(0);
            }

            execvp(stage_argv[i][0], stage_argv[i]);
            // On failure, print error but let remaining pipeline run
            printf("Command not found!\n");
            _exit(127);
        } else {
            pids[i] = pid;
            if (i == 0) {
                setpgid(pid, pid);
                pgid = pid;
                set_foreground_pid(pid);
                // transfer terminal control to pipeline group
                tcsetpgrp(STDIN_FILENO, pgid);
            } else {
                setpgid(pid, pgid);
            }
        }
    }

    // ---------- Parent: close all pipes ----------
    for (int i = 0; i < nstages - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // ---------- Parent: wait for all children ----------
    int stopped = 0;
    for (int i = 0; i < nstages; i++) {
        int status; pid_t w = waitpid(pids[i], &status, WUNTRACED);
        if (w > 0 && WIFSTOPPED(status)) stopped = 1; // mark pipeline stopped
    }

    clear_foreground_pid();
    // Restore terminal to shell (if not already restored by a stop path)
    tcsetpgrp(STDIN_FILENO, shell_pgid);
    if (stopped) {
        printf("[%d] Stopped %s\n", pgid, stage_argv[0][0]);
        
        // Build command string for silent registration  
        char cmd_str[256] = "";
        for (int i = 0; stage_argv[0][i]; i++) {
            if (i > 0) strcat(cmd_str, " ");
            strcat(cmd_str, stage_argv[0][i]);
        }
        bg_register_silent(pgid, cmd_str);
    }
}
// ############## LLM Generated Code Ends ##############