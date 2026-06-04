// ############## LLM Generated Code Begins ##############
#include "../include/seq_exec.h"
#include "../include/command_execution.h"
#include "../include/io_redirection.h"
#include "../include/pipeline.h"
#include "../include/background_job.h"
#include "../include/reveal.h"
#include "../include/hop.h"
#include "../include/activities.h"
#include "../include/ping.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

// --- helpers ---
static char *ltrim(char *s){ while(*s==' '||*s=='\t'||*s=='\r'||*s=='\n') s++; return s; }
static void rtrim_inplace(char *s){
    int n=(int)strlen(s);
    while(n>0){ char c=s[n-1]; if(c==' '||c=='\t'||c=='\r'||c=='\n') s[--n]='\0'; else break; }
}
static int split_argv(char *buf, char *argv[], int maxv){
    int argc=0; char *tok=strtok(buf," \t\r\n");
    while(tok && argc<maxv-1){ argv[argc++]=tok; tok=strtok(NULL," \t\r\n"); }
    argv[argc]=NULL; return argc;
}
static int has_pipe(char *argv[]){ for(int i=0;argv[i];i++) if(!strcmp(argv[i],"|")) return 1; return 0; }
static int has_redirection(char *argv[]){
    for(int i=0;argv[i];i++){
        if(!strcmp(argv[i],"<")||!strcmp(argv[i],">")||!strcmp(argv[i],">>")) return 1;
        if(argv[i][0]=='<'||argv[i][0]=='>') return 1;
    }
    return 0;
}
static int has_trailing_amp(char *argv[]){ int i=0; while(argv[i]) i++; return (i>0 && !strcmp(argv[i-1],"&")); }
static void strip_trailing_amp(char *argv[]){ int i=0; while(argv[i]) i++; if(i>0 && !strcmp(argv[i-1],"&")) argv[i-1]=NULL; }

// --- we record segments and whether they were separated by '&' (bg) or ';' (fg) ---
typedef struct { char *seg; int bg; } Seg;

// Execute a single command, checking for builtins first
static void execute_single_command(char *argv[], int needs_pipe, int needs_redir) {
    if (!argv || !argv[0]) return;

    // If redirection or piping is needed, defer to generic execution path so
    // builtins are handled in a forked child with redirections applied. This
    // ensures commands like: echo hi > file ; cat < file | wc -c work.
    if (needs_pipe) {
        execute_with_pipes(argv);
        return;
    }
    if (needs_redir) {
        execute_with_redirection(argv);
        return;
    }

    // Builtins only when no pipes/redirections.
    if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; argv[i]; i++) {
            if (i > 1) printf(" ");
            printf("%s", argv[i]);
        }
        printf("\n");
        return;
    }
    if (strcmp(argv[0], "hop") == 0) {
        const char *home_dir = getenv("HOME");
        if (!home_dir) home_dir = "/";
        int argc = 0; while (argv[argc]) argc++;
        hop_command(argc, argv, home_dir);
        return;
    }
    if (strcmp(argv[0], "reveal") == 0) {
        const char *home_dir = getenv("HOME");
        if (!home_dir) home_dir = "/";
        int argc = 0; while (argv[argc]) argc++;
        reveal_command(argc, argv, home_dir);
        return;
    }
    if (strcmp(argv[0], "activities") == 0) {
        activities_command();
        return;
    }
    if (strcmp(argv[0], "ping") == 0) {
        int argc = 0; while (argv[argc]) argc++;
        ping_command(argc, argv);
        return;
    }
    if (strcmp(argv[0], "bg") == 0) { printf("No such job\n"); return; }
    if (strcmp(argv[0], "fg") == 0) { printf("No such job\n"); return; }

    // External command fallback
    execute_command(argv);
}

void execute_sequence(const char *line){
    if(!line) return;

    char buf[4096];
    strncpy(buf, line, sizeof(buf)-1);
    buf[sizeof(buf)-1]='\0';

    Seg segs[256]; int nsegs=0;

    // Pass: split by ; and &, and remember the separator type
    const char *src = buf;
    char *writep = buf;
    const char *p = buf;
    char sep = 0;
    (void)src;

    char *seg_start = writep;
    for (; *p; p++){
        if (*p == ';' || *p == '&') {
            sep = *p;
            *writep = '\0'; // terminate current segment
            // trim and record
            char *s = ltrim(seg_start);
            rtrim_inplace(s);
            if (*s != '\0' && nsegs < (int)(sizeof(segs)/sizeof(segs[0]))) {
                segs[nsegs].seg = s;
                segs[nsegs].bg  = (sep == '&');
                nsegs++;
            }
            // next segment starts after this char
            writep = (char*)(p+1);
            seg_start = writep;
        } else {
            // keep character (already in place)
            if (writep != p) *writep = *p;
            writep++;
        }
    }
    *writep = '\0';
    // last segment (no trailing sep)
    {
        char *s = ltrim(seg_start);
        rtrim_inplace(s);
        if (*s != '\0' && nsegs < (int)(sizeof(segs)/sizeof(segs[0]))) {
            segs[nsegs].seg = s;
            segs[nsegs].bg  = 0; // default fg; may become bg if trailing '&' token inside seg
            nsegs++;
        }
    }

    // Execute in order
    for (int si = 0; si < nsegs; si++) {
        char seg_copy[4096];
        strncpy(seg_copy, segs[si].seg, sizeof(seg_copy)-1);
        seg_copy[sizeof(seg_copy)-1] = '\0';

        char *argv[128];
        int argc = split_argv(seg_copy, argv, 128);
        if (argc == 0) continue;

        int is_bg = segs[si].bg;
        // also support trailing '&' token inside the segment
        char original_cmd[512] = "";
        if (!is_bg && has_trailing_amp(argv)) { 
            // Save original command before stripping & for background job tracking
            for (int i = 0; argv[i]; i++) {
                if (i > 0) strcat(original_cmd, " ");
                strcat(original_cmd, argv[i]);
            }
            strip_trailing_amp(argv); 
            is_bg = 1; 
        } else if (is_bg) {
            // Command already marked as bg by separator, save original
            for (int i = 0; argv[i]; i++) {
                if (i > 0) strcat(original_cmd, " ");
                strcat(original_cmd, argv[i]);
            }
            strcat(original_cmd, " &");  // Add & since it was stripped by separator parsing
        }

        int needs_pipe  = has_pipe(argv);
        int needs_redir = has_redirection(argv);

        if (is_bg) {
            pid_t pid = fork();
            if (pid < 0) { perror("fork"); continue; }
            if (pid == 0) {
                // child: no input from terminal
                int devnull = open("/dev/null", O_RDONLY);
                if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }
                if (needs_pipe) { execute_with_pipes(argv); _exit(0); }
                else if (needs_redir) { execute_with_redirection(argv); _exit(0); }
                else execute_command_direct(argv); // does not return on success
                _exit(0); // safety
            } else {
                bg_register(pid, original_cmd); // parent: report [job] pid with original command
            }
        } else {
            execute_single_command(argv, needs_pipe, needs_redir);
        }
    }
}

// ############## LLM Generated Code Ends ##############