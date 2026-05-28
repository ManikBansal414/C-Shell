#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "../include/parser.h"

static int is_space(char c) {
    return (c==' ' || c=='\t' || c=='\n' || c=='\r');
}

static int is_metachar(char c) {
    return (c=='|' || c=='&' || c==';' || c=='<' || c=='>');
}

static void skip_spaces(const char *line, int *i) {
    while (is_space(line[*i])) (*i)++;
}

static int parse_name(const char *line, int *i) {
    skip_spaces(line, i);
    int start = *i;
    while (line[*i] != '\0' && !is_space(line[*i]) && !is_metachar(line[*i])) {
        (*i)++;
    }
    return (*i > start);
}

static int parse_input(const char *line, int *i) {
    int j = *i;
    skip_spaces(line, &j);
    if (line[j] == '<') {
        j++;
        if (!parse_name(line, &j)) return 0;
        *i = j;
        return 1;
    }
    return 0;
}

static int parse_output(const char *line, int *i) {
    int j = *i;
    skip_spaces(line, &j);
    if (line[j] == '>') {
        j++;
        if (line[j] == '>') j++;
        if (!parse_name(line, &j)) return 0;
        *i = j;
        return 1;
    }
    return 0;
}

static int parse_atomic(const char *line, int *i) {
    if (!parse_name(line, i)) return 0;
    while (1) {
        int before = *i;
        if (parse_name(line, i)) continue;
        if (parse_input(line, i)) continue;
        if (parse_output(line, i)) continue;
        *i = before;
        break;
    }
    return 1;
}

static int parse_cmd_group(const char *line, int *i) {
    if (!parse_atomic(line, i)) return 0;
    while (1) {
        int j = *i;
        skip_spaces(line, &j);
        if (line[j] == '|') {
            j++;
            if (!parse_atomic(line, &j)) return 0;
            *i = j;
        } else break;
    }
    return 1;
}

int parse_shell_cmd(const char *line, int *i) {
    if (!parse_cmd_group(line, i)) return 0;

    while (1) {
        int j = *i;
        skip_spaces(line, &j);

        if (line[j] == ';' || line[j] == '&') {
            char sep = line[j];
            j++;
            int k = j; skip_spaces(line, &k);
            if (sep == '&' && line[k] == '\0') {
                *i = k;
                return 1;
            }
            if (!parse_cmd_group(line, &j)) return 0;
            *i = j;
            continue;
        }
        break;
    }

    skip_spaces(line, i);
    if (line[*i] == '&') {
        int j2 = *i + 1;
        skip_spaces(line, &j2);
        if (line[j2] == '\0') {
            *i = j2;
        } else return 0;
    }

    skip_spaces(line, i);
    return (line[*i] == '\0');
}
