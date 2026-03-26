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
#include <glob.h>
#include "utils.h"
#include "config.h"
#include "commands.h"
#include "signals.h"
#include "functions.h"

char* expand_tilde(const char *path) {
    if (!path || strchr(path, '~') == NULL)
        return path ? strdup(path) : NULL;

    const char *home = getenv("HOME");
    if (!home) home = "/";

    char expanded[8192] = "";
    const char *p = path;
    
    while (*p) {
        if (*p == '~' && (p == path || *(p-1) == ':')) {
            strncat(expanded, home, sizeof(expanded) - strlen(expanded) - 1);
            p++;
        } else {
            int len = strlen(expanded);
            if (len < 8191) {
                expanded[len] = *p;
                expanded[len+1] = '\0';
            }
            p++;
        }
    }
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
    char buffer[4096];
    int buf_i = 0;
    bool in_sq = false;
    bool in_dq = false;

    while (*p) {
        if (!in_sq && !in_dq && *p == '#' && (p == line || isspace(*(p-1)))) break;
        if (argc >= max_args - 1) break;

        if (!in_sq && !in_dq) {
            const char *peek = p;
            int fd_prefix = -1;
            if (isdigit((unsigned char)*peek) && (peek[1] == '<' || peek[1] == '>')) {
                fd_prefix = *peek - '0';
            }

            if (*p == '<' || *p == '>' || fd_prefix != -1) {
                if (buf_i > 0) {
                    buffer[buf_i] = '\0';
                    args[argc++] = strdup(buffer);
                    buf_i = 0;
                }

                char op_buf[16];
                int op_j = 0;
                if (fd_prefix != -1) op_buf[op_j++] = *p++;
                
                op_buf[op_j++] = *p++;
                if ((*p == '<' || *p == '>' || *p == '&') && (*p == op_buf[op_j-1] || *p == '&')) {
                    op_buf[op_j++] = *p++;
                }
                op_buf[op_j] = '\0';
                args[argc++] = strdup(op_buf);
                continue;
            }

            if (*p == '\\') {
                if (p[1]) {
                    buffer[buf_i++] = '\x10';
                    buffer[buf_i++] = p[1];
                    p += 2;
                } else {
                    p++;
                }
                continue;
            }
            if (*p == '\'') {
                in_sq = true;
                buffer[buf_i++] = '\x04';
                p++;
                continue;
            }
            if (*p == '"') {
                in_dq = true;
                buffer[buf_i++] = '\x06';
                p++;
                continue;
            }
            if (isspace((unsigned char)*p)) {
                if (buf_i > 0) {
                    buffer[buf_i] = '\0';
                    args[argc++] = strdup(buffer);
                    buf_i = 0;
                }
                while (*p && isspace((unsigned char)*p)) p++;
                continue;
            }
            if (*p == '*' && (p == line || *(p-1) != '$')) { buffer[buf_i++] = '\x01'; p++; continue; }
            if (*p == '?' && (p == line || *(p-1) != '$')) { buffer[buf_i++] = '\x02'; p++; continue; }
            if (*p == '[') { buffer[buf_i++] = '\x03'; p++; continue; }
        } else if (in_sq) {
            if (*p == '\'') {
                in_sq = false;
                buffer[buf_i++] = '\x05';
                p++;
                continue;
            }
            buffer[buf_i++] = *p++;
            continue;
        } else if (in_dq) {
            if (*p == '\\') {
                if (p[1] == '$' || p[1] == '"' || p[1] == '\\' || p[1] == '`' || p[1] == '\n') {
                    buffer[buf_i++] = '\x10';
                    buffer[buf_i++] = p[1];
                    p += 2;
                } else {
                    buffer[buf_i++] = *p++;
                }
                continue;
            }
            if (*p == '"') {
                in_dq = false;
                buffer[buf_i++] = '\x07';
                p++;
                continue;
            }
            buffer[buf_i++] = *p++;
            continue;
        }

        buffer[buf_i++] = *p++;
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
    for (int i = 0; i < *argc; i++) {
        char *arg = args[i];
        if (!arg) continue;

        int src_fd = -1;
        char *op = arg;
        if (isdigit((unsigned char)arg[0])) {
            if (arg[1] == '<' || arg[1] == '>') {
                src_fd = arg[0] - '0';
                op = arg + 1;
            }
        }

        if (op[0] != '<' && op[0] != '>') continue;

        if (strcmp(op, ">") != 0 && strcmp(op, ">>") != 0 && strcmp(op, "<") != 0 &&
            strcmp(op, "<<") != 0 && strcmp(op, ">&") != 0 && strcmp(op, "<&") != 0) {
            continue;
        }

        if (i + 1 >= *argc) {
            fprintf(stderr, "cvx: syntax error near unexpected token 'newline'\n");
            return;
        }

        char *target = args[i + 1];
        if (src_fd == -1) src_fd = (op[0] == '<') ? 0 : 1;

        if (strcmp(op, ">&") == 0 || strcmp(op, "<&") == 0) {
            if (strcmp(target, "-") == 0) {
                close(src_fd);
            } else {
                bool is_num = true;
                for (int k = 0; target[k]; k++) if (!isdigit((unsigned char)target[k])) is_num = false;
                if (is_num && target[0] != '\0') {
                    int target_fd = atoi(target);
                    if (dup2(target_fd, src_fd) < 0) perror("dup2");
                } else {
                    fprintf(stderr, "cvx: %s: ambiguous redirect\n", target);
                }
            }
        } else if (strcmp(op, ">>") == 0) {
            int fd = open(target, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) { dup2(fd, src_fd); close(fd); }
            else perror(target);
        } else if (strcmp(op, ">") == 0) {
            int fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) { dup2(fd, src_fd); close(fd); }
            else perror(target);
        } else if (strcmp(op, "<") == 0) {
            int fd = open(target, O_RDONLY);
            if (fd >= 0) { dup2(fd, src_fd); close(fd); }
            else perror(target);
        } else if (strcmp(op, "<<") == 0) {
            char heredoc_file[] = "/tmp/cvx_heredoc_XXXXXX";
            int tmp_fd = mkstemp(heredoc_file);
            if (tmp_fd >= 0) {
                printf("> enter lines, end with '%s'\n", target);
                fflush(stdout);
                while (1) {
                    char buf[1024];
                    printf("> ");
                    fflush(stdout);
                    if (!fgets(buf, sizeof(buf), stdin)) break;
                    buf[strcspn(buf, "\n")] = 0;
                    if (strcmp(buf, target) == 0) break;
                    write(tmp_fd, buf, strlen(buf));
                    write(tmp_fd, "\n", 1);
                }
                lseek(tmp_fd, 0, SEEK_SET);
                dup2(tmp_fd, src_fd);
                close(tmp_fd);
                unlink(heredoc_file);
            } else perror("mkstemp");
        }

        free(args[i]);
        free(args[i + 1]);
        for (int j = i; j + 2 <= *argc; j++) args[j] = args[j + 2];
        *argc -= 2;
        i--;
    }
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

extern int last_exit_status;

static ParamFrame *param_stack = NULL;

void push_param_frame(int argc, char **argv) {
    ParamFrame *frame = malloc(sizeof(ParamFrame));
    frame->argc = argc;
    frame->argv = malloc(argc * sizeof(char *));
    for (int i = 0; i < argc; i++) {
        frame->argv[i] = strdup(argv[i]);
    }
    frame->next = param_stack;
    param_stack = frame;
}

void pop_param_frame() {
    if (!param_stack) return;
    ParamFrame *tmp = param_stack;
    param_stack = param_stack->next;
    for (int i = 0; i < tmp->argc; i++) {
        free(tmp->argv[i]);
    }
    free(tmp->argv);
    free(tmp);
}

void set_current_param_frame(int argc, char **argv) {
    if (!param_stack) {
        char *dummy_argv[] = { "cvx" };
        push_param_frame(1, dummy_argv);
    }
    
    char *old_zero = strdup(param_stack->argv[0]);
    for (int i = 0; i < param_stack->argc; i++) {
        free(param_stack->argv[i]);
    }
    free(param_stack->argv);
    
    param_stack->argc = argc + 1;
    param_stack->argv = malloc((argc + 1) * sizeof(char *));
    param_stack->argv[0] = old_zero;
    for (int i = 0; i < argc; i++) {
        param_stack->argv[i + 1] = strdup(argv[i]);
    }
}

char* expand_variables(const char *input) {
    if (!input) return NULL;
    char buffer[8192];
    int j = 0;
    bool in_sq = false;
    bool in_dq = false;

    for (int i = 0; input[i]; i++) {
        if (input[i] == '\x04') { in_sq = true; buffer[j++] = input[i]; continue; }
        if (input[i] == '\x05') { in_sq = false; buffer[j++] = input[i]; continue; }
        if (input[i] == '\x06') { in_dq = true; buffer[j++] = input[i]; continue; }
        if (input[i] == '\x07') { in_dq = false; buffer[j++] = input[i]; continue; }
        if (input[i] == '\x10') { buffer[j++] = input[i++]; buffer[j++] = input[i]; continue; }

        if (input[i] == '$' && !in_sq) {
            i++;
            char *val = NULL;
            bool should_split = !in_dq;

            if (input[i] == '\0' || isspace(input[i]) || input[i] == '"' || input[i] == '\'') {
                if (j < 8191) buffer[j++] = '$';
                i--;
                continue;
            }

            if (input[i] == '?') {
                char status_str[16];
                snprintf(status_str, sizeof(status_str), "%d", last_exit_status);
                val = strdup(status_str);
            } else if (input[i] == '#') {
                char count_str[16];
                int count = param_stack ? param_stack->argc - 1 : 0;
                if (count < 0) count = 0;
                snprintf(count_str, sizeof(count_str), "%d", count);
                val = strdup(count_str);
            } else if (input[i] == '*' || input[i] == '@') {
                if (param_stack && param_stack->argc > 1) {
                    int len = 0;
                    for (int k = 1; k < param_stack->argc; k++) len += strlen(param_stack->argv[k]) + 1;
                    val = malloc(len + 1);
                    val[0] = '\0';
                    for (int k = 1; k < param_stack->argc; k++) {
                        strcat(val, param_stack->argv[k]);
                        if (k < param_stack->argc - 1) strcat(val, " ");
                    }
                }
            } else if (isdigit(input[i])) {
                int idx = input[i] - '0';
                if (param_stack && idx < param_stack->argc) {
                    val = strdup(param_stack->argv[idx]);
                }
            } else if (input[i] == '{') {
                i++;
                char varname[128];
                char op = 0;
                char word[256];
                word[0] = '\0';
                int k = 0;
                while (input[i] && input[i] != '}' && input[i] != ':' && input[i] != '-' && input[i] != '=' && input[i] != '?' && input[i] != '+' && k < 127) {
                    varname[k++] = input[i++];
                }
                varname[k] = '\0';
                
                bool check_null = false;
                if (input[i] == ':') {
                    check_null = true;
                    i++;
                }

                if (input[i] == '-' || input[i] == '=' || input[i] == '?' || input[i] == '+') {
                    op = input[i++];
                    int w = 0;
                    while (input[i] && input[i] != '}' && w < 255) {
                        word[w++] = input[i++];
                    }
                    word[w] = '\0';
                } else {
                    while (input[i] && input[i] != '}') i++;
                }
                if (input[i] == '}') i++;
                
                char *env_val = NULL;
                if (isdigit(varname[0])) {
                    int idx = atoi(varname);
                    if (param_stack && idx < param_stack->argc) env_val = param_stack->argv[idx];
                } else {
                    env_val = getenv(varname);
                }
                
                bool is_set = (env_val != NULL);
                bool is_null = (env_val != NULL && env_val[0] == '\0');
                bool use_default = check_null ? (!is_set || is_null) : !is_set;
                
                if (op == 0) {
                    if (env_val) val = strdup(env_val);
                } else if (op == '-') {
                    if (!use_default) val = strdup(env_val ? env_val : "");
                    else val = expand_variables(word);
                } else if (op == '=') {
                    if (!use_default) val = strdup(env_val ? env_val : "");
                    else {
                        val = expand_variables(word);
                        setenv(varname, val ? val : "", 1);
                    }
                } else if (op == '?') {
                    if (!use_default) val = strdup(env_val ? env_val : "");
                    else {
                        char *exp_word = expand_variables(word);
                        fprintf(stderr, "%s: %s\n", varname, (exp_word && exp_word[0]) ? exp_word : "parameter null or not set");
                        free(exp_word);
                        exit(1);
                    }
                } else if (op == '+') {
                    if (!use_default) val = expand_variables(word);
                    else val = NULL;
                }
                i--;
            } else if (isalnum(input[i]) || input[i] == '_') {
                char varname[128];
                int k = 0;
                while ((isalnum(input[i]) || input[i]=='_') && k < 127) varname[k++] = input[i++];
                varname[k] = '\0';
                i--;
                char *env = getenv(varname);
                if (env) val = strdup(env);
            } else {
                if (j < 8191) buffer[j++] = '$';
                i--;
                continue;
            }

            if (val) {
                for (int l = 0; val[l]; l++) {
                    if (j < 8191) {
                        if (should_split && isspace(val[l])) buffer[j++] = '\x11';
                        else if (should_split && (val[l] == '*' || val[l] == '?' || val[l] == '[')) {
                            if (val[l] == '*') buffer[j++] = '\x01';
                            else if (val[l] == '?') buffer[j++] = '\x02';
                            else if (val[l] == '[') buffer[j++] = '\x03';
                        }
                        else buffer[j++] = val[l];
                    }
                }
                free(val);
            }
        } else {
            if (j < 8191) buffer[j++] = input[i];
        }
    }

    buffer[j] = '\0';
    return strdup(buffer);
}

void quote_removal(char *args[], int argc) {
    for (int i = 0; i < argc; i++) {
        if (!args[i]) continue;
        char *src = args[i];
        char *dst = args[i];
        while (*src) {
            if (*src == '\x04' || *src == '\x05' || *src == '\x06' || *src == '\x07') {
                src++;
            } else if (*src == '\x10') {
                src++;
                if (*src) *dst++ = *src++;
            } else if (*src == '\x01') {
                *dst++ = '*'; src++;
            } else if (*src == '\x02') {
                *dst++ = '?'; src++;
            } else if (*src == '\x03') {
                *dst++ = '['; src++;
            } else {
                *dst++ = *src++;
            }
        }
        *dst = '\0';
    }
}

void expand_glob(char *args[], int *argc, int max_args) {
    char *new_args[max_args];
    int new_argc = 0;

    for (int i = 0; i < *argc; i++) {
        bool has_marker = false;
        if (args[i]) {
            for (int k = 0; args[i][k]; k++) {
                if (args[i][k] == '\x01' || args[i][k] == '\x02' || args[i][k] == '\x03') {
                    has_marker = true;
                    break;
                }
            }
        }

        if (has_marker) {
            char *pattern = strdup(args[i]);
            for (int k = 0; pattern[k]; k++) {
                if (pattern[k] == '\x01') pattern[k] = '*';
                else if (pattern[k] == '\x02') pattern[k] = '?';
                else if (pattern[k] == '\x03') pattern[k] = '[';
            }

            glob_t results;
            int ret = glob(pattern, GLOB_NOCHECK | GLOB_TILDE, NULL, &results);
            if (ret == 0) {
                for (size_t j = 0; j < results.gl_pathc && new_argc < max_args - 1; j++) {
                    new_args[new_argc++] = strdup(results.gl_pathv[j]);
                }
                globfree(&results);
                free(args[i]);
            } else {
                for (int k = 0; args[i][k]; k++) {
                    if (args[i][k] == '\x01') args[i][k] = '*';
                    else if (args[i][k] == '\x02') args[i][k] = '?';
                    else if (args[i][k] == '\x03') args[i][k] = '[';
                }
                if (new_argc < max_args - 1) {
                    new_args[new_argc++] = args[i];
                } else {
                    free(args[i]);
                }
            }
            free(pattern);
        } else {
            if (new_argc < max_args - 1) {
                new_args[new_argc++] = args[i];
            } else {
                free(args[i]);
            }
        }
    }
    new_args[new_argc] = NULL;
    for (int i = 0; i < new_argc; i++) {
        args[i] = new_args[i];
    }
    args[new_argc] = NULL;
    *argc = new_argc;
}

void unescape_args(char *args[], int argc) {
    for (int i = 0; i < argc; i++) {
        char *tmp = unescape_string(args[i]);
        free(args[i]);
        args[i] = tmp;
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