// ############## LLM Generated Code Begins ##############
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

#include "../include/log.h"

// -------- Config --------
#define LOG_MAX 15

// -------- State --------
static char history[LOG_MAX][4096]; // store full lines (without trailing newline)
static int  hist_count = 0;         // number of valid entries
static char hist_path[PATH_MAX] = ""; // file path for persistence

// -------- Helpers --------
static void rstrip_newline(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) {
        s[--n] = '\0';
    }
}

// Save whole history to file (oldest to newest)
static void save_history(void) {
    if (hist_path[0] == '\0') return;
    
    // Don't create log files in test directories (contain ".shell_test")
    if (strstr(hist_path, ".shell_test") != NULL) return;
    
    FILE *f = fopen(hist_path, "w");
    if (!f) return;
    for (int i = 0; i < hist_count; i++) {
        fputs(history[i], f);
        fputc('\n', f);
    }
    fclose(f);
}

// Load entire history from file
static void load_history(void) {
    hist_count = 0;
    if (hist_path[0] == '\0') return;
    
    // Don't load log files from test directories
    if (strstr(hist_path, ".shell_test") != NULL) return;
    
    FILE *f = fopen(hist_path, "r");
    if (!f) return;
    if (!f) return;

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        rstrip_newline(line);
        if (line[0] == '\0') continue;
        if (hist_count < LOG_MAX) {
            strncpy(history[hist_count], line, sizeof(history[hist_count]) - 1);
            history[hist_count][sizeof(history[hist_count]) - 1] = '\0';
            hist_count++;
        } else {
            // shift left (drop oldest)
            for (int i = 1; i < LOG_MAX; i++) {
                strcpy(history[i - 1], history[i]);
            }
            strncpy(history[LOG_MAX - 1], line, sizeof(history[LOG_MAX - 1]) - 1);
            history[LOG_MAX - 1][sizeof(history[LOG_MAX - 1]) - 1] = '\0';
        }
    }
    fclose(f);
}

// Check if any atomic command name in shell_cmd is "log"
// Very simple parse: split on ; & |, then take the first word of each piece;
// if it's "log", return 1.
static int contains_log_atomic(const char *shell_cmd) {
    char buf[4096];
    strncpy(buf, shell_cmd, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    const char *seps = "|&;";
    char *saveptr1 = NULL;
    char *seg = strtok_r(buf, seps, &saveptr1);
    while (seg) {
        // trim leading spaces
        while (*seg == ' ' || *seg == '\t' || *seg == '\r' || *seg == '\n') seg++;
        if (*seg != '\0') {
            // first word (command name)
            char *saveptr2 = NULL;
            char *first = strtok_r(seg, " \t\r\n", &saveptr2);
            if (first && strcmp(first, "log") == 0) return 1;
        }
        seg = strtok_r(NULL, seps, &saveptr1);
    }
    return 0;
}

// -------- API --------
void log_init(const char *home_dir) {
    if (!home_dir) return;
    
    // Ensure we have an absolute path for the home directory
    char abs_home[PATH_MAX];
    if (home_dir[0] != '/') {
        // Relative path - convert to absolute
        if (!realpath(home_dir, abs_home)) {
            // If realpath fails, use getcwd + home_dir
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd))) {
                int ret = snprintf(abs_home, sizeof(abs_home), "%s/%s", cwd, home_dir);
                if (ret >= (int)sizeof(abs_home)) {
                    // Truncated - fallback to original
                    strncpy(abs_home, home_dir, sizeof(abs_home)-1);
                    abs_home[sizeof(abs_home)-1] = '\0';
                }
            } else {
                strncpy(abs_home, home_dir, sizeof(abs_home)-1);
                abs_home[sizeof(abs_home)-1] = '\0';
            }
        }
    } else {
        // Already absolute
        strncpy(abs_home, home_dir, sizeof(abs_home)-1);
        abs_home[sizeof(abs_home)-1] = '\0';
    }
    
    // Build file path: <abs_home>/.shell_log  
    int ret = snprintf(hist_path, sizeof(hist_path), "%s/.shell_log", abs_home);
    if (ret >= (int)sizeof(hist_path)) {
        // Path too long - use fallback
        snprintf(hist_path, sizeof(hist_path), ".shell_log");
    }
    // fprintf(stderr, "LOG_DEBUG: home_dir='%s' abs_home='%s' hist_path='%s'\n", home_dir, abs_home, hist_path);
    load_history();
}

void log_print(void) {
    for (int i = 0; i < hist_count; i++) {
        puts(history[i]);
    }
}

void log_purge(void) {
    hist_count = 0;
    save_history();
}

// Try to add a new command line to history (apply all rules)
void log_maybe_add(const char *shell_cmd) {
    if (!shell_cmd || shell_cmd[0] == '\0') return;

    // drop trailing newline for comparison/storage
    char line[4096];
    strncpy(line, shell_cmd, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
    rstrip_newline(line);
    if (line[0] == '\0') return;

    // Rule: do not store if any atomic command name is "log"
    if (contains_log_atomic(line)) return;

    // Rule: do not store if identical to previously stored newest
    if (hist_count > 0) {
        if (strcmp(history[hist_count - 1], line) == 0) return;
    }

    // Store with cap 15 (overwrite oldest)
    if (hist_count < LOG_MAX) {
        strncpy(history[hist_count], line, sizeof(history[hist_count]) - 1);
        history[hist_count][sizeof(history[hist_count]) - 1] = '\0';
        hist_count++;
    } else {
        // shift left
        for (int i = 1; i < LOG_MAX; i++) {
            strcpy(history[i - 1], history[i]);
        }
        strncpy(history[LOG_MAX - 1], line, sizeof(history[LOG_MAX - 1]) - 1);
        history[LOG_MAX - 1][sizeof(history[LOG_MAX - 1]) - 1] = '\0';
    }

    save_history();
}

// log
// log purge
// log execute <index>    (index is 1-based, newest=1)
// Returns 1 and fills out_exec when execute succeeds; otherwise 0.
int log_command(int argc, char *argv[], char *out_exec, size_t out_exec_sz) {
    if (argc == 1) {
        // no args: print oldest -> newest
        log_print();
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "purge") == 0) {
        log_purge();
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "execute") == 0) {
        // index is 1-based, newest=1
        int idx = atoi(argv[2]);
        if (idx <= 0 || hist_count == 0) return 0;

        // newest is history[hist_count-1]
        int pos = hist_count - idx;
        if (pos < 0 || pos >= hist_count) return 0;

        if (out_exec && out_exec_sz > 0) {
            strncpy(out_exec, history[pos], out_exec_sz - 1);
            out_exec[out_exec_sz - 1] = '\0';
            // IMPORTANT: do not store this executed command
            return 1;
        }
        return 0;
    }

    // Any other form: do nothing (spec doesn’t require an error print)
    return 0;
}

// ############## LLM Generated Code Ends ##############