// ############## LLM Generated Code Begins ##############
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <limits.h>
#include <unistd.h>
#include <ctype.h>
#include "../include/reveal.h"
#include "../include/hop.h"     // for hop_prev_dir()

// small dynamic vector for names
static int cmp_str(const void *a, const void *b) {
    const char *A = *(const char *const *)a;
    const char *B = *(const char *const *)b;
    return strcmp(A, B); // pure ASCII ordering
}

static int is_hidden(const char *name) {
    return name[0] == '.';
}

static int build_target_path(char out[PATH_MAX],
                             const char *arg,
                             const char *home_dir) {
    // No arg given handled by caller (will pass ".")
    if (strcmp(arg, "~") == 0) {
        // home
        if (snprintf(out, PATH_MAX, "%s", home_dir) >= PATH_MAX) return 0;
        return 1;
    } else if (strcmp(arg, ".") == 0) {
        if (snprintf(out, PATH_MAX, ".") >= PATH_MAX) return 0;
        return 1;
    } else if (strcmp(arg, "..") == 0) {
        if (snprintf(out, PATH_MAX, "..") >= PATH_MAX) return 0;
        return 1;
    } else if (strcmp(arg, "-") == 0) {
        const char *prev = hop_prev_dir();
        if (!prev || prev[0] == '\0') return -1; // unset prev
        if (snprintf(out, PATH_MAX, "%s", prev) >= PATH_MAX) return 0;
        return 1;
    } else {
        // absolute or relative path as-is
        if (snprintf(out, PATH_MAX, "%s", arg) >= PATH_MAX) return 0;
        return 1;
    }
}

void reveal_command(int argc, char *argv[], const char *home_dir) {
    int show_all = 0; // -a
    int one_per_line = 0; // -l
    const char *arg_path = NULL;

    // parse flags: one or more -[a|l]* groups; unknown letters ignored
    for (int i = 1; i < argc; i++) {
        char *s = argv[i];
        if (s[0] == '-' && s[1] != '\0') {
            // it's a flag group, e.g., -alalala
            for (int j = 1; s[j] != '\0'; j++) {
                if (s[j] == 'a') show_all = 1;
                else if (s[j] == 'l') one_per_line = 1;
                else {
                    // ignore other letters per examples/spec behavior
                }
            }
        } else {
            // non-flag argument (path-like); accept only ONE
            if (arg_path == NULL) arg_path = s;
            else {
                printf("reveal: Invalid Syntax!\n");
                return;
            }
        }
    }

    // default target = "." when no path arg given
    char target[PATH_MAX];
    if (arg_path == NULL) {
        snprintf(target, sizeof(target), ".");
    } else {
        int r = build_target_path(target, arg_path, home_dir);
        if (r == -1) { // '-' but no prev_dir yet
            printf("No such directory!\n");
            return;
        }
        if (r == 0) {  // overflow or error
            printf("No such directory!\n");
            return;
        }
    }

    // open directory
    DIR *dir = opendir(target);
    if (!dir) {
        printf("No such directory!\n");
        return;
    }

    // collect entries
    size_t cap = 64, n = 0;
    char **names = (char **)malloc(cap * sizeof(char *));
    if (!names) { closedir(dir); perror("malloc"); return; }

    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        const char *name = de->d_name;

        // skip hidden unless -a
        if (!show_all && is_hidden(name)) continue;

        // push copy
        size_t len = strlen(name);
        char *copy = (char *)malloc(len + 1);
        if (!copy) { /* low memory: clean up */ break; }
        memcpy(copy, name, len + 1);

        if (n == cap) {
            cap *= 2;
            char **tmp = (char **)realloc(names, cap * sizeof(char *));
            if (!tmp) { free(copy); break; }
            names = tmp;
        }
        names[n++] = copy;
    }
    closedir(dir);

    // sort (ASCII lexicographic)
    qsort(names, n, sizeof(char *), cmp_str);

    // print
    if (one_per_line) {
        for (size_t i = 0; i < n; i++) {
            printf("%s\n", names[i]);
        }
    } else {
        // space-separated like simple ls
        for (size_t i = 0; i < n; i++) {
            if (i) printf(" ");
            printf("%s", names[i]);
        }
        if (n > 0) printf("\n");
    }

    // free
    for (size_t i = 0; i < n; i++) free(names[i]);
    free(names);
}
// ############## LLM Generated Code Ends ##############