// ############## LLM Generated Code Begins ##############
#include "../include/background_job.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_BG 128

typedef struct {
    pid_t pid;
    char  name[128];
    int   jobno;
    int   active;   // 1 = tracked (running/stopped), 0 = free slot
} BgJob;

static BgJob jobs[MAX_BG];
static int next_jobno = 1;

// Helper function to build command string from argv
static void build_command_string(char *dest, size_t size, char **argv) {
    dest[0] = '\0';
    for (int i = 0; argv[i]; i++) {
        if (i > 0) strncat(dest, " ", size - strlen(dest) - 1);
        strncat(dest, argv[i], size - strlen(dest) - 1);
    }
}

static int find_free_slot(void) {
    for (int i = 0; i < MAX_BG; i++) {
        if (!jobs[i].active) return i;
    }
    return -1;
}

void bg_register(pid_t pid, const char *cmd_name) {
    if (pid <= 0) return;
    int slot = find_free_slot();
    if (slot < 0) return;

    jobs[slot].pid   = pid;
    jobs[slot].jobno = next_jobno++;
    jobs[slot].active = 1;

    // store the full command name as provided
    if (cmd_name && *cmd_name) {
        strncpy(jobs[slot].name, cmd_name, sizeof(jobs[slot].name)-1);
        jobs[slot].name[sizeof(jobs[slot].name)-1] = '\0';
    } else {
        strcpy(jobs[slot].name, "command");
    }

    // Background job registration as per rubric requirement
    printf("[%d] %d\n", jobs[slot].jobno, jobs[slot].pid);
    fflush(stdout);
}

void bg_register_argv(pid_t pid, char **argv) {
    char cmd_str[256];
    build_command_string(cmd_str, sizeof(cmd_str), argv);
    bg_register(pid, cmd_str);
}

void bg_register_silent(pid_t pid, const char *cmd_name) {
    if (pid <= 0) return;
    int slot = find_free_slot();
    if (slot < 0) return;

    jobs[slot].pid   = pid;
    jobs[slot].jobno = next_jobno++;
    jobs[slot].active = 1;

    // store the full command name as provided
    if (cmd_name && *cmd_name) {
        strncpy(jobs[slot].name, cmd_name, sizeof(jobs[slot].name)-1);
        jobs[slot].name[sizeof(jobs[slot].name)-1] = '\0';
    } else {
        strcpy(jobs[slot].name, "command");
    }

    // No output for silent registration (used for stopped processes)
}

void bg_check(void) {
    // Non-blocking wait over all active jobs
    for (int i = 0; i < MAX_BG; i++) {
        if (!jobs[i].active) continue;

        int status;
        pid_t r = waitpid(jobs[i].pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (r == 0) continue; // still running
        if (r < 0) {
            // likely already reaped elsewhere; mark inactive
            jobs[i].active = 0;
            continue;
        }

        if (WIFSTOPPED(status)) {
            // keep it active (already reported at time of stop)
            continue;
        }
        if (WIFCONTINUED(status)) {
            // optional message omitted
            continue;
        }
        if (WIFEXITED(status)) {
            printf("%s with pid %d exited normally\n", jobs[i].name, jobs[i].pid);
        } else if (WIFSIGNALED(status)) {
            printf("%s with pid %d exited abnormally\n", jobs[i].name, jobs[i].pid);
        } else {
            printf("%s with pid %d exited abnormally\n", jobs[i].name, jobs[i].pid);
        }
        fflush(stdout);
        jobs[i].active = 0; // finished
    }
}

int bg_snapshot(BgInfo out[], int maxn) {
    if (maxn <= 0) return 0;
    int c = 0;
    for (int i = 0; i < MAX_BG && c < maxn; i++) {
        if (!jobs[i].active) continue;
        out[c].pid = jobs[i].pid;
        // copy stored short name
        strncpy(out[c].name, jobs[i].name, sizeof(out[c].name)-1);
        out[c].name[sizeof(out[c].name)-1] = '\0';
        c++;
    }
    return c;
}

void bg_cleanup(void) {
    // Send SIGKILL to all active background jobs
    for (int i = 0; i < MAX_BG; i++) {
        if (jobs[i].active && jobs[i].pid > 0) {
            kill(jobs[i].pid, SIGKILL);
            jobs[i].active = 0;  // mark as inactive
        }
    }
}

// ############## LLM Generated Code Ends ##############