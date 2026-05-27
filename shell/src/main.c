// ############## LLM Generated Code Begins ##############
// main.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <pwd.h>
#include <fcntl.h>   // open()
#include <signal.h>  // signal handling
#include <sys/wait.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>

#include "../include/prompt.h"
#include "../include/parser.h"
#include "../include/hop.h"
#include "../include/reveal.h"
#include "../include/log.h"
#include "../include/command_execution.h"
#include "../include/io_redirection.h"
#include "../include/pipeline.h"
#include "../include/seq_exec.h"
#include "../include/background_job.h"
#include "../include/activities.h"
#include "../include/ping.h"

// --- helpers ---
static int has_control_sep(const char *s) {            // any ';' or '&' in the line?
    for (; *s; s++) if (*s == ';' || *s == '&') return 1;
    return 0;
}

static int has_pipe(char *argv[]) {
    for (int i = 0; argv[i]; i++) if (strcmp(argv[i], "|") == 0) return 1;
    return 0;
}

static int has_redirection(char *argv[]) {
    for (int i = 0; argv[i]; i++) {
        if (!strcmp(argv[i], "<") || !strcmp(argv[i], ">") || !strcmp(argv[i], ">>")) return 1;
        if (argv[i][0] == '<' || argv[i][0] == '>') return 1; // compact forms
    }
    return 0;
}

static int has_trailing_amp(char *argv[]) {
    int i = 0; while (argv[i]) i++;
    return (i > 0 && strcmp(argv[i-1], "&") == 0);
}
static void strip_trailing_amp(char *argv[]) {
    int i = 0; while (argv[i]) i++;
    if (i > 0 && strcmp(argv[i-1], "&") == 0) argv[i-1] = NULL;
}

// simple tokenizer to build argc/argv from the input line
static int split_line(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *token = strtok(line, " \t\n\r");
    while (token && argc < max_args - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n\r");
    }
    argv[argc] = NULL;
    return argc;
}

// Global variable to track foreground process
pid_t foreground_pid = 0;
pid_t shell_pgid = 0;
int shell_terminal = -1;
static volatile sig_atomic_t sigchld_pending = 0;

// Signal handler for SIGINT (Ctrl+C)
static void sigint_handler(int sig) {
    (void)sig;  // unused parameter
    if (foreground_pid > 0) {
        // Send SIGINT to the foreground process group
        kill(-foreground_pid, SIGINT);
    }
    // Print newline for clean prompt display
    printf("\n");
    // Don't terminate the shell itself
}

// Signal handler for SIGTSTP (Ctrl+Z)
static void sigtstp_handler(int sig) {
    (void)sig;
    if (foreground_pid > 0) {
        kill(-foreground_pid, SIGTSTP); // job control layer prints status via wait
    }
}

// SIGCHLD handler to promptly report background job completion
static void sigchld_handler(int sig) {
    (void)sig;
    sigchld_pending = 1; // defer reap to safe point
}

int main(void)
{
    // Initialize job control: put shell in its own process group and grab terminal
    shell_terminal = STDIN_FILENO;
    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0) {
        // ignore error if already set
    }
    tcsetpgrp(shell_terminal, shell_pgid);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    const char *username = getenv("USER");
    char username_buf[64];
    if (!username) {
        struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_name) {
            strncpy(username_buf, pw->pw_name, sizeof(username_buf)-1);
            username_buf[sizeof(username_buf)-1] = '\0';
            username = username_buf;
        } else {
            username = "user";
        }
    }

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "localhost", sizeof(hostname)-1);
        hostname[sizeof(hostname)-1] = '\0';
    }

    char home_dir[PATH_MAX];
    if (!getcwd(home_dir, sizeof(home_dir))) {
        fprintf(stderr, "Failed to getcwd.\n");
        return 1;
    }
    log_init(home_dir);

    // Install signal handlers for Ctrl+C and Ctrl+Z
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGPIPE, SIG_IGN);  // Ignore broken pipe signals
    signal(SIGCHLD, sigchld_handler); // background completion

    // make stdout unbuffered for immediate async messages
    setvbuf(stdout, NULL, _IONBF, 0);
    char line[4096];
    while (1) {
        if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, "/");
        print_prompt(username, hostname, home_dir, cwd);

        size_t len = 0; line[0] = '\0';
        while (1) {
            if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
            fd_set rfds; FD_ZERO(&rfds); FD_SET(STDIN_FILENO, &rfds);
            struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 100000; // 100ms
            int rv = select(STDIN_FILENO+1, &rfds, NULL, NULL, &tv);
            if (rv > 0 && FD_ISSET(STDIN_FILENO, &rfds)) {
                char ch;
                ssize_t r = read(STDIN_FILENO, &ch, 1);
                if (r <= 0) { // EOF
                    printf("logout\n");
                    if (foreground_pid > 0) kill(-foreground_pid, SIGKILL);
                    bg_cleanup();
                    return 0;
                }
                if (ch == '\n') { line[len] = '\0'; break; }
                if (len < sizeof(line)-1) { line[len++] = ch; }
            }
            // if rv == 0 timeout just loop to allow bg_check
            if (rv < 0) {
                // interrupted by signal; continue
                continue;
            }
        }
        if (line[0] == '\0') continue; // empty line just reprompt

    // quick exit (raw line check is fine)
    if (strncmp(line, "exit", 4) == 0) break;

        // Build argc/argv from a COPY (strtok mutates the buffer)
        char line_copy[4096];
        strncpy(line_copy, line, sizeof(line_copy)-1);
        line_copy[sizeof(line_copy)-1] = '\0';

        char *argv[64];
        int argc = split_line(line_copy, argv, 64);
        if (argc == 0) continue; // empty input

        // Validate syntax on the ORIGINAL line (as per Part A)
        int idx = 0;
        if (!parse_shell_cmd(line, &idx)) {
            printf("Invalid Syntax!\n");
            continue;
        }

        // Store command in history (log module internally skips lines containing 'log')
        log_maybe_add(line);

        // ---- Check for control separators first ----
        if (has_control_sep(line)) {
            // Treat ';' and '&' as separators: background only for segments ending with '&'
            execute_sequence(line);
            if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
            continue;
        }

        // ---- Check for redirection/pipes for single commands ----
        if (has_pipe(argv)) {
            execute_with_pipes(argv);
            if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
            continue;
        }
        if (has_redirection(argv)) {
            execute_with_redirection(argv);
            if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
            continue;
        }

        // ---- builtins (only if no redirection/pipes/sequences) ----
        if (strcmp(argv[0], "hop") == 0) {
            hop_command(argc, argv, home_dir);
            if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
            continue;
        }
        if (strcmp(argv[0], "reveal") == 0) {
            reveal_command(argc, argv, home_dir);
            if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
            continue;
        }
        if (strcmp(argv[0], "activities") == 0) {
            activities_command();
            if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
            continue;
        }
        if (strcmp(argv[0], "ping") == 0) {
            ping_command(argc, argv);
            if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
            continue;
        }
        if (strcmp(argv[0], "echo") == 0) {
            for (int i = 1; i < argc; i++) {
                if (i > 1) printf(" ");
                printf("%s", argv[i]);
            }
            printf("\n");
            if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
            continue;
        }
        if (strcmp(argv[0], "bg") == 0) {
            // bg command - put background job in background (no-op for already background jobs)
            if (argc < 2) {
                printf("No such job\n");
            } else {
                printf("No such job\n");  // For now, since we don't track job numbers properly
            }
            continue;
        }
        if (strcmp(argv[0], "fg") == 0) {
            // fg command - bring background job to foreground
            if (argc < 2) {
                printf("No such job\n");
            } else {
                printf("No such job\n");  // For now, since we don't track job numbers properly
            }
            continue;
        }
        if (strcmp(argv[0], "log") == 0) {
            char exec_buf[4096];
            int run = log_command(argc, argv, exec_buf, sizeof(exec_buf));
            if (run) {
                // Do NOT store exec_buf again
                int idx2 = 0;
                if (!parse_shell_cmd(exec_buf, &idx2)) {
                    printf("Invalid Syntax!\n");
                } else {
                    // route by control separators first
                    if (has_control_sep(exec_buf)) {
                        execute_sequence(exec_buf);
                        if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
                    } else {
                        // tokenize exec_buf
                        char tmp_copy[4096];
                        strncpy(tmp_copy, exec_buf, sizeof(tmp_copy)-1);
                        tmp_copy[sizeof(tmp_copy)-1] = '\0';

                        char *argv2[64];
                        int argc2 = split_line(tmp_copy, argv2, 64);
                        if (argc2 > 0) {
                            if (strcmp(argv2[0], "hop") == 0) {
                                hop_command(argc2, argv2, home_dir);
                            } else if (strcmp(argv2[0], "reveal") == 0) {
                                reveal_command(argc2, argv2, home_dir);
                            } else {
                                // single-command path from log execute (support trailing &)
                                int is_bg2 = has_trailing_amp(argv2);
                                
                                // Save original command before stripping & for background job tracking
                                char original_cmd2[512] = "";
                                if (is_bg2) {
                                    for (int i = 0; argv2[i]; i++) {
                                        if (i > 0) strcat(original_cmd2, " ");
                                        strcat(original_cmd2, argv2[i]);
                                    }
                                    strip_trailing_amp(argv2);
                                }

                                if (is_bg2) {
                                    pid_t pid = fork();
                                    if (pid < 0) {
                                        perror("fork");
                                    } else if (pid == 0) {
                                        int devnull = open("/dev/null", O_RDONLY);
                                        if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }
                                        if (has_pipe(argv2)) {
                                            execute_with_pipes(argv2); _exit(0);
                                        } else if (has_redirection(argv2)) {
                                            execute_with_redirection(argv2); _exit(0);
                                        } else {
                                            execute_command_direct(argv2); // no further fork
                                        }
                                    } else {
                                        bg_register(pid, original_cmd2);  // use original command with &
                                    }
                                } else {
                                    if (has_pipe(argv2)) { execute_with_pipes(argv2); }
                                    else if (has_redirection(argv2)) { execute_with_redirection(argv2); }
                                    else { execute_command(argv2); }
                                    if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
                                }
                            }
                        }
                    }
                }
            }
            continue;
        }

        // ---- fallback: external commands or background ----
        // single-command path (possible trailing &)
        int is_bg = has_trailing_amp(argv);
        
        // Save original command before stripping & for background job tracking
        char original_cmd[512] = "";
        if (is_bg) {
            for (int i = 0; argv[i]; i++) {
                if (i > 0) strcat(original_cmd, " ");
                strcat(original_cmd, argv[i]);
            }
            strip_trailing_amp(argv);
        }

        if (is_bg) {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
            } else if (pid == 0) {
                int devnull = open("/dev/null", O_RDONLY);
                if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }
                execute_command_direct(argv); // does not return
            } else {
                bg_register(pid, original_cmd);  // use original command with &
            }
        } else {
            execute_command(argv);
            if (sigchld_pending) { sigchld_pending = 0; bg_check(); }
        }
        continue;
    }
    return 0;
}

// ############## LLM Generated Code Ends ##############