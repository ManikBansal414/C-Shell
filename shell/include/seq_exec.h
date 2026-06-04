#ifndef SEQ_EXEC_H
#define SEQ_EXEC_H

// Execute a full line that may contain ';' separated commands.
// Each segment is treated as an independent command line.
void execute_sequence(const char *line);

#endif
