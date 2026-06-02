// ############## LLM Generated Code Begins ##############
#include "../include/activities.h"
#include "../include/background_job.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// One row of the activities table
typedef struct {
    pid_t pid;
    char  name[128];
    int   is_stopped;   // 1 = Stopped, 0 = Running
} ActRow;

// Safe parse of /proc/<pid>/stat (handles spaces in comm)
// Returns 1 if stopped ('T'), 0 otherwise. If /proc missing, assume not stopped;
// finished processes are removed by bg_check().
static int is_process_stopped(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[1024];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);

    // Find last ')' of the (comm) field, then the state char is after ") "
    char *rp = strrchr(line, ')');
    if (!rp || rp[1] != ' ' || rp[2] == '\0') return 0;
    char state = rp[2];
    return (state == 'T');
}

// comparator for qsort (lexicographic by name)
static int cmp_activities(const void *a, const void *b) {
    const ActRow *ra = (const ActRow*)a;
    const ActRow *rb = (const ActRow*)b;
    return strcmp(ra->name, rb->name);
}

void activities_command(void) {
    // Requirement: remove terminated processes — ensure bg_check runs now too.
    bg_check();

    BgInfo list[256];
    int n = bg_snapshot(list, 256);
    if (n <= 0) return;

    ActRow rows[256];
    int rc = 0;
    for (int i = 0; i < n; i++) {
        rows[rc].pid = list[i].pid;
        strncpy(rows[rc].name, list[i].name, sizeof(rows[rc].name)-1);
        rows[rc].name[sizeof(rows[rc].name)-1] = '\0';
        rows[rc].is_stopped = is_process_stopped(list[i].pid);
        rc++;
    }

    // sort by command name
    qsort(rows, rc, sizeof(ActRow), cmp_activities);

    // print in required format
    for (int i = 0; i < rc; i++) {
        printf("[%d] : %s - %s\n",
               rows[i].pid,
               rows[i].name,
               rows[i].is_stopped ? "Stopped" : "Running");
    }
}

// ############## LLM Generated Code Ends ##############