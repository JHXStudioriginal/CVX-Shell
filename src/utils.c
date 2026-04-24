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
#include "parser.h"
#include <sys/wait.h>

static long get_val(const char **p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
    if (isdigit((unsigned char)**p)) {
        char *endptr;
        long val = strtol(*p, &endptr, 10);
        *p = endptr;
        return val;
    } else if (isalpha((unsigned char)**p) || **p == '_') {
        char var[128];
        int k = 0;
        while ((isalnum((unsigned char)**p) || **p == '_') && k < 127) var[k++] = *(*p)++;
        var[k] = '\0';
        char *ev = getenv(var);
        return ev ? atol(ev) : 0;
    }
    return 0;
}

static long evaluate_arithmetic(const char *expr) {
    if (!expr) return 0;
    const char *p = expr;
    long res = get_val(&p);
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char op = *p++;
        long next = get_val(&p);
        if (op == '+') res += next;
        else if (op == '-') res -= next;
        else if (op == '*') res *= next;
        else if (op == '/' && next != 0) res /= next;
    }
    return res;
}

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
    bool sub_in_sq = false;
    bool sub_in_dq = false;
    int paren_depth = 0;

    while (*p) {
        if (!in_sq && !in_dq && paren_depth == 0) {
            if (isspace((unsigned char)*p) || *p == '\x11') {
                if (buf_i > 0) {
                    buffer[buf_i] = '\0';
                    args[argc++] = strdup(buffer);
                    buf_i = 0;
                }
                while (*p && (isspace((unsigned char)*p) || *p == '\x11')) p++;
                continue;
            }
            if (*p == '#' && (p == line || isspace((unsigned char)*(p-1)))) break;
            if (*p == ';' || *p == '|' || *p == '&' || *p == '(' || *p == ')') {
                if (buf_i > 0) {
                    buffer[buf_i] = '\0';
                    args[argc++] = strdup(buffer);
                    buf_i = 0;
                }
                
                
                
                
                char op[3] = {*p, '\0', '\0'};
                if ((*p == '&' && p[1] == '&') || (*p == '|' && p[1] == '|') || (*p == ';' && p[1] == ';')) {
                    op[1] = p[1];
                    p++;
                }
                args[argc++] = strdup(op);
                p++;
                continue;
            }
        }

        if (argc >= max_args - 1) break;

        
        if (!in_sq) {
            if (*p == '$' && p[1] == '(') {
                buffer[buf_i++] = *p++;
                buffer[buf_i++] = *p++;
                paren_depth++;
                continue;
            }
        }

        if (paren_depth > 0) {
            if (*p == '\'') {
                if (!sub_in_dq && (p == line || *(p-1) != '\\')) sub_in_sq = !sub_in_sq;
            } else if (*p == '"') {
                if (!sub_in_sq && (p == line || *(p-1) != '\\')) sub_in_dq = !sub_in_dq;
            } else if (!sub_in_sq && !sub_in_dq) {
                if (*p == '(') paren_depth++;
                else if (*p == ')') {
                    paren_depth--;
                    if (paren_depth == 0) {
                        sub_in_sq = false;
                        sub_in_dq = false;
                    }
                }
            }
            buffer[buf_i++] = *p++;
            continue;
        }

        
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
        }

        
        if (!in_sq && !in_dq) {
            if (*p == '\\') {
                if (p[1]) {
                    buffer[buf_i++] = '\x10';
                    buffer[buf_i++] = p[1];
                    p += 2;
                } else p++;
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
        } else if (in_dq) {
            if (*p == '\\') {
                if (p[1] == '$' || p[1] == '"' || p[1] == '\\' || p[1] == '`' || p[1] == '\n') {
                    buffer[buf_i++] = '\x10';
                    buffer[buf_i++] = p[1];
                    p += 2;
                } else buffer[buf_i++] = *p++;
                continue;
            }
            if (*p == '"') {
                in_dq = false;
                buffer[buf_i++] = '\x07';
                p++;
                continue;
            }
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
                    buf[strcspn(buf, "\r\n")] = 0;
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

typedef struct {
    char **res;
    size_t *j;
    size_t *res_size;
} ExpandCtx;

static void add_c(ExpandCtx *ctx, char c) {
    if (*(ctx->j) + 1 >= *(ctx->res_size)) {
        *(ctx->res_size) *= 2;
        char *nr = realloc(*(ctx->res), *(ctx->res_size));
        if (!nr) return;
        *(ctx->res) = nr;
    }
    (*(ctx->res))[(*(ctx->j))++] = c;
}

char* expand_variables(const char *input) {
    if (!input) return NULL;
    size_t res_size = 4096;
    char *res = malloc(res_size);
    if (!res) return NULL;
    size_t j = 0;
    ExpandCtx ectx = { &res, &j, &res_size };
    bool in_sq = false, in_dq = false;
    
    bool is_assignment = false;
    const char *as_start = input;
    while (*as_start == '\x04' || *as_start == '\x05' || *as_start == '\x06' || *as_start == '\x07') as_start++;
    if (isalpha((unsigned char)as_start[0]) || as_start[0] == '_') {
        int k = 1;
        while (isalnum((unsigned char)as_start[k]) || as_start[k] == '_') k++;
        if (as_start[k] == '=') is_assignment = true;
    }

    for (int i = 0; input[i]; i++) {
        if (input[i] == '\x04') { in_sq = true;  add_c(&ectx, input[i]); continue; }
        if (input[i] == '\x05') { in_sq = false; add_c(&ectx, input[i]); continue; }
        if (input[i] == '\x06') { in_dq = true;  add_c(&ectx, input[i]); continue; }
        if (input[i] == '\x07') { in_dq = false; add_c(&ectx, input[i]); continue; }
        if (input[i] == '\x10') { add_c(&ectx, input[i++]); if(input[i]) add_c(&ectx, input[i]); continue; }

        if (input[i] == '$' && !in_sq) {
            if (input[i+1] == '(' && input[i+2] == '(') {
                i += 3;
                int start_i = i, depth = 1;
                while (input[i] && depth > 0) {
                    if (input[i] == '(') depth++;
                    else if (input[i] == ')') {
                        depth--;
                        if (depth == 0 && input[i+1] == ')') { i++; break; }
                    }
                    i++;
                }
                char *expr_raw = strndup(input + start_i, i - start_i);
                char *expr_expanded = expand_variables(expr_raw);
                long res_val = evaluate_arithmetic(expr_expanded);
                char sbuf[32];
                snprintf(sbuf, sizeof(sbuf), "%ld", res_val);
                for (int l = 0; sbuf[l]; l++) add_c(&ectx, sbuf[l]);
                free(expr_raw);
                free(expr_expanded);
                continue;
            }
            if (input[i+1] == '(') {
                i += 2;
                int start_i = i, depth = 1;
                bool inner_sq = false, inner_dq = false;
                while (input[i] && depth > 0) {
                    if (input[i] == '\'' && (i == 0 || input[i-1] != '\\') && !inner_dq) inner_sq = !inner_sq;
                    else if (input[i] == '"' && (i == 0 || input[i-1] != '\\') && !inner_sq) inner_dq = !inner_dq;
                    else if (!inner_sq && !inner_dq) {
                        if (input[i] == '(') depth++;
                        else if (input[i] == ')') depth--;
                    }
                    if (depth > 0) i++;
                }
                char *cmd = strndup(input + start_i, i - start_i);
                int pipefd[2];
                if (pipe(pipefd) == 0) {
                    fflush(NULL);
                    pid_t pid = fork();
                    if (pid == 0) {
                        close(pipefd[0]);
                        dup2(pipefd[1], STDOUT_FILENO);
                        close(pipefd[1]);
                        exit(process_command_line(cmd));
                    } else if (pid > 0) {
                        close(pipefd[1]);
                        size_t cap_size = 4096, cap_len = 0;
                        char *cap = malloc(cap_size), r_buf[4096];
                        ssize_t n;
                        while ((n = read(pipefd[0], r_buf, sizeof(r_buf))) > 0) {
                            if (cap_len + n >= cap_size) {
                                cap_size *= 2;
                                cap = realloc(cap, cap_size);
                            }
                            if (cap) { memcpy(cap + cap_len, r_buf, n); cap_len += n; }
                        }
                        if (cap) {
                            cap[cap_len] = '\0';
                            while (cap_len > 0 && (cap[cap_len-1] == '\n' || cap[cap_len-1] == '\r')) cap[--cap_len] = '\0';
                            bool should_split = !in_dq && !is_assignment;
                            for (size_t l = 0; cap[l]; l++) {
                                char c = cap[l];
                                if (should_split && isspace((unsigned char)c)) c = '\x11';
                                add_c(&ectx, c);
                            }
                            free(cap);
                        }
                        close(pipefd[0]);
                        waitpid(pid, NULL, 0);
                    }
                }
                free(cmd);
                continue;
            }

            i++;
            char *val = NULL;
            bool should_split = !in_dq && !is_assignment;
            
            if (input[i] == '\0' || isspace((unsigned char)input[i]) || input[i] == '"' || input[i] == '\'') {
                add_c(&ectx, '$'); i--; continue;
            }

            if (input[i] == '?') {
                char sbuf[16]; snprintf(sbuf, 16, "%d", last_exit_status); val = strdup(sbuf);
            } else if (input[i] == '#') {
                int count = param_stack ? param_stack->argc - 1 : 0;
                char sbuf[16]; snprintf(sbuf, 16, "%d", count < 0 ? 0 : count); val = strdup(sbuf);
            } else if (isdigit((unsigned char)input[i])) {
                int idx = input[i] - '0';
                if (param_stack && idx < param_stack->argc) val = strdup(param_stack->argv[idx]);
            } else if (input[i] == '{') {
                i++;
                char var[128], op = 0, word[256]; var[0] = word[0] = '\0';
                int k = 0;
                while (input[i] && input[i] != '}' && !strchr(":-=?+", input[i]) && k < 127) var[k++] = input[i++];
                var[k] = '\0';
                bool check_null = (input[i] == ':');
                if (check_null) i++;
                if (strchr("-=?+", input[i])) {
                    op = input[i++];
                    int w = 0;
                    while (input[i] && input[i] != '}' && w < 255) word[w++] = input[i++];
                    word[w] = '\0';
                }
                while (input[i] && input[i] != '}') i++;
                char *ev = NULL;
                if (isdigit((unsigned char)var[0])) {
                    int idx = atoi(var);
                    if (param_stack && idx < param_stack->argc) ev = param_stack->argv[idx];
                } else ev = getenv(var);
                
                bool is_set = (ev != NULL), is_null = (ev && !*ev);
                bool use_def = check_null ? (!is_set || is_null) : !is_set;
                
                if (op == 0) { if (ev) val = strdup(ev); } 
                else if (op == '-') val = use_def ? expand_variables(word) : strdup(ev?ev:"");
                else if (op == '=') {
                    if (use_def) { val = expand_variables(word); setenv(var, val?val:"", 1); }
                    else val = strdup(ev?ev:"");
                } else if (op == '?') {
                    if (use_def) {
                        char *ew = expand_variables(word);
                        fprintf(stderr, "%s: %s\n", var, (ew && *ew)?ew:"parameter null or not set");
                        free(ew); exit(1);
                    } else val = strdup(ev?ev:"");
                } else if (op == '+') val = use_def ? NULL : expand_variables(word);
            } else {
                int start_v = i;
                while (isalnum((unsigned char)input[i]) || input[i] == '_') i++;
                char *var = strndup(input + start_v, i - start_v);
                char *ev = getenv(var); if (ev) val = strdup(ev);
                free(var); i--;
            }

            if (val) {
                for (int l = 0; val[l]; l++) {
                    char c = val[l];
                    if (should_split && isspace((unsigned char)c)) c = '\x11';
                    else if (should_split && strchr("*?[", c)) {
                        if (c == '*') add_c(&ectx, '\x01');
                        else if (c == '?') add_c(&ectx, '\x02');
                        else if (c == '[') add_c(&ectx, '\x03');
                        continue;
                    }
                    add_c(&ectx, c);
                }
                free(val);
            }
        } else {
            add_c(&ectx, input[i]);
        }
    }
    add_c(&ectx, '\0');
    return res;
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
