// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include "parser.h"
#include "config.h"
#include "commands.h"
#include "signals.h"

char* expand_tilde(const char *path) {
    if (!path || path[0] != '~')
        return strdup(path);

    const char *home = getenv("HOME");
    if (!home) home = "/";

    char expanded[1024];
    snprintf(expanded, sizeof(expanded), "%s%s", home, path + 1);
    return strdup(expanded);
}

char* unescape_string(const char *s) {
    if (!s) return NULL;
    char *buf = malloc(strlen(s) + 1);
    if (!buf) return NULL;
    int j = 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\\' && s[i+1]) {
            i++;
            switch (s[i]) {
                case 'n': buf[j++] = '\n'; break;
                case 't': buf[j++] = '\t'; break;
                case 'r': buf[j++] = '\r'; break;
                case 'a': buf[j++] = '\a'; break;
                case 'v': buf[j++] = '\v'; break;
                case '\\': buf[j++] = '\\'; break;
                case '"': buf[j++] = '"'; break;
                case '\'': buf[j++] = '\''; break;
                case '$': buf[j++] = '$'; break;
                case 'x': {
                    int val = 0;
                    for (int k = 0; k < 2 && isxdigit((unsigned char)s[i+1]); k++, i++)
                        val = val*16 + (isdigit(s[i+1]) ? s[i+1]-'0' : (tolower(s[i+1])-'a'+10));
                    buf[j++] = (char)val;
                    break;
                }
                default: buf[j++] = s[i]; break;
            }
        } else {
            buf[j++] = s[i];
        }
    }
    buf[j] = '\0';
    return buf;
}

int split_args(const char *line, char *args[], int max_args) {
    int argc = 0;
    const char *p = line;
    char buffer[1024];
    int buf_i = 0;
    bool in_quotes = false;
    char quote_char = '\0';

    while (*p) {
        if (*p == '#' && !in_quotes) break;
        if (argc >= max_args - 1) break;

        if (*p == '\\' && p[1] != '\0') {
            buffer[buf_i++] = *p++;
            buffer[buf_i++] = *p++;
            continue;
        }

        if ((*p == '<' && p[1] == '<') || (*p == '>' && p[1] == '>')) {
            if (buf_i > 0) {
                buffer[buf_i] = '\0';
                args[argc++] = strdup(buffer);
                buf_i = 0;
            }
            char tmp[3] = { *p, *(p+1), '\0' };
            args[argc++] = strdup(tmp);
            p += 2;
            continue;
        }

        if (*p == '<' || *p == '>') {
            if (buf_i > 0) {
                buffer[buf_i] = '\0';
                args[argc++] = strdup(buffer);
                buf_i = 0;
            }
            char tmp[2] = { *p, '\0' };
            args[argc++] = strdup(tmp);
            p++;
            continue;
        }

        if ((*p == '"' || *p == '\'') && !in_quotes) {
            in_quotes = true;
            quote_char = *p;
            p++;
            continue;
        }
        if (*p == quote_char && in_quotes) {
            in_quotes = false;
            quote_char = '\0';
            p++;
            continue;
        }

        if (*p == ' ' && !in_quotes) {
            if (buf_i > 0) {
                buffer[buf_i] = '\0';
                args[argc++] = strdup(buffer);
                buf_i = 0;
            }
            p++;
            continue;
        }

        buffer[buf_i++] = *p;
        p++;
    }

    if (buf_i > 0) {
        buffer[buf_i] = '\0';
        args[argc++] = strdup(buffer);
    }

    args[argc] = NULL;
    return argc;
}

void free_args(char *args[], int argc) {
    for (int i = 0; i < argc; i++) {
        free(args[i]);
        args[i] = NULL;
    }
}

void handle_redirection(char *args[], int *argc) {
    int in_fd = -1, out_fd = -1;
    char heredoc_file[] = "/tmp/cvx_heredoc_XXXXXX";

    for (int i = 0; i < *argc; i++) {
        if (strcmp(args[i], "<") == 0 && i + 1 < *argc) {
            in_fd = open(args[i + 1], O_RDONLY);
            if (in_fd < 0) perror("open input");

            free(args[i]);
            free(args[i + 1]);
            for (int j = i; j + 2 <= *argc; j++) args[j] = args[j + 2];
            *argc -= 2;
            i--;
        } else if (strcmp(args[i], ">>") == 0 && i + 1 < *argc) {
            out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (out_fd < 0) perror("open append output");

            free(args[i]);
            free(args[i + 1]);
            for (int j = i; j + 2 <= *argc; j++) args[j] = args[j + 2];
            *argc -= 2;
            i--;
        } else if (strcmp(args[i], ">") == 0 && i + 1 < *argc) {
            out_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd < 0) perror("open output");

            free(args[i]);
            free(args[i + 1]);
            for (int j = i; j + 2 <= *argc; j++) args[j] = args[j + 2];
            *argc -= 2;
            i--;
        } else if (strcmp(args[i], "<<") == 0 && i + 1 < *argc) {
            int tmp_fd = mkstemp(heredoc_file);
            if (tmp_fd < 0) { perror("heredoc temp"); continue; }

            char *delimiter = args[i + 1];
            printf("> enter lines, end with '%s'\n", delimiter);
            fflush(stdout);

            while (1) {
                char buf[1024];
                printf("> ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin)) break;
                buf[strcspn(buf, "\n")] = 0;
                if (strcmp(buf, delimiter) == 0) break;
                write(tmp_fd, buf, strlen(buf));
                write(tmp_fd, "\n", 1);
            }

            lseek(tmp_fd, 0, SEEK_SET);
            in_fd = tmp_fd;
            unlink(heredoc_file);

            free(args[i]);
            free(args[i + 1]);
            for (int j = i; j + 2 <= *argc; j++) args[j] = args[j + 2];
            *argc -= 2;
            i--;
        }
    }

    if (in_fd != -1) { dup2(in_fd, STDIN_FILENO); close(in_fd); }
    if (out_fd != -1) { dup2(out_fd, STDOUT_FILENO); close(out_fd); }
}

void replace_alias(char *args[], int *argc) {
    if (!args[0]) return;

    for (int i = 0; i < alias_count; i++) {
        if (strcmp(args[0], aliases[i].name) == 0) {
            char *buf = strdup(aliases[i].command);
            char *tok;
            int new_argc = 0;
            char *tmp_args[64];

            tok = strtok(buf, " ");
            while (tok && new_argc < 63) {
                tmp_args[new_argc++] = strdup(tok);
                tok = strtok(NULL, " ");
            }

            for (int j = 1; j < *argc && new_argc < 64; j++)
                tmp_args[new_argc++] = strdup(args[j]);

            tmp_args[new_argc] = NULL;

            for (int j = 0; j < *argc; j++) free(args[j]);
            for (int j = 0; j < new_argc; j++) args[j] = tmp_args[j];
            args[new_argc] = NULL;
            *argc = new_argc;

            free(buf);
            break;
        }
    }
}

char* expand_variables(const char *input) {
    if (!input) return NULL;
    char buffer[4096];
    int j = 0;

    for (int i = 0; input[i]; i++) {
        if (input[i] == '$') {
            i++;
            if (input[i] == '{') {
                i++;
                char varname[128];
                int k = 0;
                while (input[i] && input[i] != '}' && k < 127) varname[k++] = input[i++];
                varname[k] = '\0';
                if (input[i] == '}') i++;
                char *val = getenv(varname);
                if (val) for (int l = 0; val[l]; l++) buffer[j++] = val[l];
            } else {
                char varname[128];
                int k = 0;
                while ((isalnum(input[i]) || input[i]=='_') && k < 127) varname[k++] = input[i++];
                varname[k] = '\0';
                i--;
                char *val = getenv(varname);
                if (val) for (int l = 0; val[l]; l++) buffer[j++] = val[l];
            }
        } else buffer[j++] = input[i];
    }

    buffer[j] = '\0';
    return strdup(buffer);
}

void unescape_args(char *args[], int argc) {
    for (int i = 0; i < argc; i++) {
        char *tmp = unescape_string(args[i]);
        free(args[i]);
        args[i] = tmp;
    }
}

void process_command_line(char *line) {
    if (!line) return;

    char *saveptr = NULL;
    char *cmd = strtok_r(line, ";\n", &saveptr);

    while (cmd) {
        while (*cmd == ' ' || *cmd == '\t') cmd++;

        char *end = cmd + strlen(cmd) - 1;
        while (end > cmd && (*end == ' ' || *end == '\n'))
            *end-- = '\0';

        if (*cmd != '\0') {
            process_single_command(cmd);
        }

        cmd = strtok_r(NULL, ";\n", &saveptr);
    }
}

char* expand_history(const char *line, const char *last_command) {
    if (!line) return NULL;
    if (!last_command) return strdup(line);

    char buffer[8192];
    int j = 0;
    int len = strlen(line);

    for (int i = 0; i < len; i++) {
        if (line[i] == '\\' && line[i+1] == '!' && line[i+2] == '!') {
            buffer[j++] = '!';
            buffer[j++] = '!';
            i += 2;
        } else if (line[i] == '!' && line[i+1] == '!') {
            const char *lc = last_command;
            while (*lc && j < 8191) {
                buffer[j++] = *lc++;
            }
            i++;
        } else {
            if (j < 8191) {
                buffer[j++] = line[i];
            }
        }
    }
    buffer[j] = '\0';
    return strdup(buffer);
}
