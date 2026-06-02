#ifndef ACTIVITIES_H
#define ACTIVITIES_H

// Print all active processes spawned by the shell that are still running or stopped.
// Format per line: [pid] : command_name - Running|Stopped
// Sorted lexicographically by command_name.
void activities_command(void);

#endif
