// ############## LLM Generated Code Begins ##############
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "../include/hop.h"

static char prev_dir[PATH_MAX] = "";   // remember last directory

const char *hop_prev_dir(void) {
    // empty string means "unset" → reveal should treat as "No such directory!"
    return prev_dir;
}

void hop_command(int argc, char *argv[], const char *home_dir) {
    char cwd[PATH_MAX];

    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd");
        return;
    }

    // If no args: same as "~"
    if (argc == 1) {
        if (chdir(home_dir) != 0) {
            printf("No such directory!\n");
            return;
        }
        strcpy(prev_dir, cwd);
        return;
    }

    // Loop through each argument
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (strcmp(arg, "~") == 0) {
            if (chdir(home_dir) != 0) {
                printf("No such directory!\n");
                return;
            }
        } 
        else if (strcmp(arg, ".") == 0) {
            // do nothing
        } 
        else if (strcmp(arg, "..") == 0) {
            if (chdir("..") != 0) {
                // no parent, ignore
            }
        } 
        else if (strcmp(arg, "-") == 0) {
            if (strlen(prev_dir) > 0) {
                if (chdir(prev_dir) != 0) {
                    printf("No such directory!\n");
                    return;
                }
            }
            // if prev_dir is empty, do nothing
        } 
        else { // treat as path
            if (chdir(arg) != 0) {
                printf("No such directory!\n");
                return;
            }
        }

        // Update prev_dir after a successful change (except .)
        if (strcmp(arg, ".") != 0) {
            strcpy(prev_dir, cwd);
            if (!getcwd(cwd, sizeof(cwd))) {
                perror("getcwd");
                return;
            }
        }
    }
}

// ############## LLM Generated Code Ends ##############