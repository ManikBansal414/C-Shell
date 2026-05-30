#ifndef IO_REDIRECTION_H
#define IO_REDIRECTION_H

// Execute argv[] with I/O redirection:
// - Input:  < file   or  <file
// - Output: > file   or  >file
// - Append: >> file  or  >>file
// Rules: last redirection wins; if input file can't open, prints exactly
// "No such file or directory" and does NOT exec the command.
void execute_with_redirection(char **argv);

#endif
