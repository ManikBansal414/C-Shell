// ############## LLM Generated Code Begins ##############
#include "../include/ping.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

void ping_command(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ping <pid> <signal_number>\n");
        return;
    }

    // Parse pid
    pid_t pid = (pid_t)atoi(argv[1]);
    if (pid <= 0) {
        printf("No such process found\n");
        return;
    }

    // Parse signal number and validate it's a number
    char *endptr;
    long sig_long = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || endptr == argv[2] || argv[2][0] == '\0') {
        printf("Invalid syntax!\n");
        return;
    }
    
    int sig = (int)sig_long;
    int actual_sig = sig % 32;

    // Try to send the signal
    if (kill(pid, actual_sig) == -1) {
        if (errno == ESRCH) {
            printf("No such process found\n");
        } else {
            perror("kill failed");
        }
    } else {
        printf("Sent signal %d to process with pid %d\n", sig, pid);
    }
}
// ############## LLM Generated Code Ends ##############