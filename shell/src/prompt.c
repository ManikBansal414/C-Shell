#include <stdio.h>
#include <string.h>
#include "../include/prompt.h"

void print_prompt(const char *username, const char *hostname,
                  const char *home_dir, const char *cwd) {
    printf("<%s@%s:", username, hostname);

    size_t home_len = strlen(home_dir);
    if (strncmp(cwd, home_dir, home_len) == 0 &&
        (cwd[home_len] == '/' || cwd[home_len] == '\0')) {
        if (cwd[home_len] == '\0') {
            printf("~");
        } else {
            printf("~%s", cwd + home_len);
        }
    } else {
        printf("%s", cwd);
    }
    printf("> ");
    fflush(stdout);
}
