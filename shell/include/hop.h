#ifndef HOP_H
#define HOP_H

void hop_command(int argc, char *argv[], const char *home_dir);

// NEW: expose previous directory for reveal '-'
const char *hop_prev_dir(void);

#endif
