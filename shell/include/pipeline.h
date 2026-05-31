#ifndef PIPELINE_H
#define PIPELINE_H

// Execute argv[] that contains one or more '|' tokens.
// Supports redirections *inside stages*:
//   < file / <file
//   > file / >file
//   >> file / >>file
// Rules: last redirection wins in that stage.
// Parent waits for all pipeline children.
void execute_with_pipes(char **argv);

#endif
