#ifndef PING_H
#define PING_H

// Syntax: ping <pid> <signal_number>
// Sends (signal_number % 32) to <pid>.
// Prints success or "No such process found".
void ping_command(int argc, char *argv[]);

#endif
