// ############## LLM Generated Code Begins ##############
#ifndef BACKGROUND_JOB_H
#define BACKGROUND_JOB_H

#include <sys/types.h>

// Register a background job (called by the parent after fork()).
// Prints: [job_number] pid
void bg_register(pid_t pid, const char *cmd_name);

// Register a background job with argv array
void bg_register_argv(pid_t pid, char **argv);

// Register a background job silently (no job number print) - for stopped processes
void bg_register_silent(pid_t pid, const char *cmd_name);

// Reap finished background jobs; call this *before parsing* each new input.
// Prints exit messages for each finished job:
//   "<cmd> with pid <pid> exited normally"
//   "<cmd> with pid <pid> exited abnormally"
void bg_check(void);

// ---- Snapshot API used by `activities` ----
typedef struct {
    pid_t pid;
    char  name[128];   // first token of the command (as stored in bg_register)
} BgInfo;

// Fill out[] with up to maxn *active* jobs; return count.
int bg_snapshot(BgInfo out[], int maxn);

// Kill all background jobs (used on shell exit)
void bg_cleanup(void);

#endif

// ############## LLM Generated Code Ends ##############
