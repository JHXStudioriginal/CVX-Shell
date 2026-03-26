// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>
#include <termios.h>
#include <ctype.h>
#include "parser.h"
#include "ast.h"
#include "commands.h"
#include "jobs.h"
#include "config.h"
#include "utils.h"
#include "linenoise.h"
#include "functions.h"

static pid_t shell_pgid = -1;
static pid_t fg_pgid = -1;
int last_exit_status = 0;
int loop_control = 0;
volatile sig_atomic_t sigint_received = 0;

int exec_command(char *cmdline, bool background) {
    if (!cmdline || !*cmdline)
        return 0;

    if (shell_pgid == -1)
        shell_pgid = getpgrp();

    char *args[256];
    int argc = split_args(cmdline, args, 256);

    replace_alias(args, &argc);

    for (int i = 0; i < argc; i++) {
        char *expanded = expand_variables(args[i]);
        free(args[i]);
        char *t_expanded = expand_tilde(expanded);
        free(expanded);
        args[i] = t_expanded;
    }

    char *new_args[256];
    int new_argc = 0;
    for (int i = 0; i < argc; i++) {
        if (strchr(args[i], '\x11')) {
            char *p = args[i];
            char *start = p;
            while (*p) {
                if (*p == '\x11') {
                    *p = '\0';
                    if (start != p) {
                        new_args[new_argc++] = strdup(start);
                    }
                    start = p + 1;
                }
                p++;
            }
            if (*start) {
                new_args[new_argc++] = strdup(start);
            }
            free(args[i]);
        } else {
            new_args[new_argc++] = args[i];
        }
    }
    argc = new_argc;
    for (int i = 0; i < argc; i++) args[i] = new_args[i];
    args[argc] = NULL;

    expand_glob(args, &argc, 256);
    quote_removal(args, argc);

    if (argc == 0) return 0;

    bool has_redirect = false;
    for (int i = 0; i < argc; i++) {
        const char *op = args[i];
        if (!op) continue;
        if (isdigit((unsigned char)op[0]) && (op[1] == '<' || op[1] == '>')) op++;
        if (op[0] == '<' || op[0] == '>') {
            has_redirect = true;
            break;
        }
    }
    
    if (argc > 0 && strchr(args[0], '=') != NULL && !has_redirect) {
        char *eq = strchr(args[0], '=');
        if (eq > args[0]) {
            *eq = '\0';
            char *varname = args[0];
            char *value = eq + 1;
            setenv(varname, value, 1);
            if (argc == 1) {
                free_args(args, argc);
                return 0;
            }
        }
    }

    const char *func_body = get_function(args[0]);
    if (func_body) {
        push_param_frame(argc, args);
        char *body_copy = strdup(func_body);
        last_exit_status = process_command_line(body_copy);
        free(body_copy);
        pop_param_frame();
        free_args(args, argc);
        return last_exit_status;
    }

    if (!has_redirect) {
        int builtin_status = -1;
        if (!strcmp(args[0], "cd")) builtin_status = cmd_cd(argc, args);
        else if (!strcmp(args[0], "pwd")) builtin_status = cmd_pwd(argc, args);
        else if (!strcmp(args[0], "export")) builtin_status = cmd_export(argc, args);
        else if (!strcmp(args[0], "help")) builtin_status = cmd_help(argc, args);
        else if (!strcmp(args[0], "ls")) builtin_status = cmd_ls(argc, args);
        else if (!strcmp(args[0], "history")) builtin_status = cmd_history(argc, args);
        else if (!strcmp(args[0], "echo")) builtin_status = cmd_echo(argc, args);
        else if (!strcmp(args[0], "jobs")) builtin_status = cmd_jobs(argc, args);
        else if (!strcmp(args[0], "fg")) builtin_status = cmd_fg(argc, args);
        else if (!strcmp(args[0], "bg")) builtin_status = cmd_bg(argc, args);
        else if (!strcmp(args[0], "alias")) builtin_status = cmd_alias(argc, args);
        else if (!strcmp(args[0], "unalias")) builtin_status = cmd_unalias(argc, args);
        else if (!strcmp(args[0], "test")) builtin_status = cmd_test(argc, args);
        else if (!strcmp(args[0], "[")) builtin_status = cmd_bracket(argc, args);
        else if (!strcmp(args[0], "functions")) builtin_status = cmd_functions(argc, args);
        else if (!strcmp(args[0], "delfunc")) builtin_status = cmd_delfunc(argc, args);
        else if (!strcmp(args[0], "set")) builtin_status = cmd_set(argc, args);
        else if (!strcmp(args[0], "break")) { loop_control = 1; builtin_status = 0; }
        else if (!strcmp(args[0], "continue")) { loop_control = 2; builtin_status = 0; }
        else if (!strcmp(args[0], ":")) builtin_status = 0;
        else if (!strcmp(args[0], "exit")) exit(0);

        if (builtin_status != -1) {
            last_exit_status = builtin_status;
            free_args(args, argc);
            return last_exit_status;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        free_args(args, argc);
        return 1;
    }

    if (pid == 0) {
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        handle_redirection(args, &argc);
        execvp(args[0], args);
        perror("exec");
        exit(1);
    }

    setpgid(pid, pid);

    int status = 0;

    if (background) {
        jobs_add(pid, cmdline, JOB_RUNNING);
        printf("[%d] %d\n", jobs_last_id(), pid);
        if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, shell_pgid);
    } else {
        fg_pgid = pid;
        if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, fg_pgid);

        waitpid(-fg_pgid, &status, WUNTRACED);

        if (WIFSTOPPED(status) || WIFSIGNALED(status))
            write(STDOUT_FILENO, "\n", 1);

        if (WIFSTOPPED(status))
        jobs_add(fg_pgid, cmdline, JOB_STOPPED);

        if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, shell_pgid);
        fg_pgid = -1;
    }

    free_args(args, argc);
    last_exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 0);
    return last_exit_status;
}

int execute_pipeline(char **cmds, int n, bool background) {
    int in_fd = 0;
    int pipefd[2];
    pid_t pgid = -1;

    if (shell_pgid == -1)
        shell_pgid = getpgrp();

    for (int i = 0; i < n; i++) {
        if (i != n - 1 && pipe(pipefd) < 0) {
            perror("pipe");
            return 1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            if (pgid == -1)
                pgid = getpid();

            setpgid(0, pgid);

            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            if (in_fd != 0) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            if (i != n - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            char *args[256];
            int argc = split_args(cmds[i], args, 256);

            replace_alias(args, &argc);

            for (int j = 0; j < argc; j++) {
                char *expanded = expand_variables(args[j]);
                free(args[j]);
                char *t_expanded = expand_tilde(expanded);
                free(expanded);
                args[j] = t_expanded;
            }

            char *new_args[256];
            int new_argc = 0;
            for (int k = 0; k < argc; k++) {
                if (strchr(args[k], '\x11')) {
                    char *p = args[k];
                    char *start = p;
                    while (*p) {
                        if (*p == '\x11') {
                            *p = '\0';
                            if (start != p) {
                                new_args[new_argc++] = strdup(start);
                            }
                            start = p + 1;
                        }
                        p++;
                    }
                    if (*start) {
                        new_args[new_argc++] = strdup(start);
                    }
                    free(args[k]);
                } else {
                    new_args[new_argc++] = args[k];
                }
            }
            argc = new_argc;
            for (int k = 0; k < argc; k++) args[k] = new_args[k];
            args[argc] = NULL;

            expand_glob(args, &argc, 256);
            quote_removal(args, argc);

            handle_redirection(args, &argc);

            if (argc == 0) exit(0);

            const char *func_body = get_function(args[0]);
            if (func_body) {
                push_param_frame(argc, args);
                char *body_copy = strdup(func_body);
                int status = process_command_line(body_copy);
                free(body_copy);
                pop_param_frame();
                exit(status);
            }

            int builtin_status = -1;
            if (!strcmp(args[0], "cd")) builtin_status = cmd_cd(argc, args);
            else if (!strcmp(args[0], "pwd")) builtin_status = cmd_pwd(argc, args);
            else if (!strcmp(args[0], "export")) builtin_status = cmd_export(argc, args);
            else if (!strcmp(args[0], "help")) builtin_status = cmd_help(argc, args);
            else if (!strcmp(args[0], "ls")) builtin_status = cmd_ls(argc, args);
            else if (!strcmp(args[0], "history")) builtin_status = cmd_history(argc, args);
            else if (!strcmp(args[0], "echo")) builtin_status = cmd_echo(argc, args);
            else if (!strcmp(args[0], "jobs")) builtin_status = cmd_jobs(argc, args);
            else if (!strcmp(args[0], "fg")) builtin_status = cmd_fg(argc, args);
            else if (!strcmp(args[0], "bg")) builtin_status = cmd_bg(argc, args);
            else if (!strcmp(args[0], "alias")) builtin_status = cmd_alias(argc, args);
            else if (!strcmp(args[0], "unalias")) builtin_status = cmd_unalias(argc, args);
            else if (!strcmp(args[0], "test")) builtin_status = cmd_test(argc, args);
            else if (!strcmp(args[0], "[")) builtin_status = cmd_bracket(argc, args);
            else if (!strcmp(args[0], "functions")) builtin_status = cmd_functions(argc, args);
            else if (!strcmp(args[0], "delfunc")) builtin_status = cmd_delfunc(argc, args);
            else if (!strcmp(args[0], "set")) builtin_status = cmd_set(argc, args);
            else if (!strcmp(args[0], "break")) { loop_control = 1; builtin_status = 0; }
            else if (!strcmp(args[0], "continue")) { loop_control = 2; builtin_status = 0; }
            else if (!strcmp(args[0], ":")) builtin_status = 0;
            else if (!strcmp(args[0], "exit")) exit(0);

            if (builtin_status != -1) exit(builtin_status);

            execvp(args[0], args);
            perror("exec");
            exit(1);
        }

        if (pgid == -1)
            pgid = pid;

        setpgid(pid, pgid);

        if (in_fd != 0)
            close(in_fd);

        if (i != n - 1) {
            close(pipefd[1]);
            in_fd = pipefd[0];
        }
    }

    if (background) {
        jobs_add(pgid, cmds[0], JOB_RUNNING);
        printf("[%d] %d\n", jobs_last_id(), pgid);
    } else {
        fg_pgid = pgid;
        tcsetpgrp(STDIN_FILENO, fg_pgid);
        int status;
        pid_t wpid;
        while ((wpid = waitpid(-fg_pgid, &status, WUNTRACED)) > 0) {
            if (wpid == pgid) {
                last_exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 0);
            }
            if (WIFSTOPPED(status) || WIFSIGNALED(status)) {
                write(STDOUT_FILENO, "\n", 1);
            }
            if (WIFSTOPPED(status)) {
                jobs_add(fg_pgid, cmds[0], JOB_STOPPED);
                break;
            }
        }
        if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, shell_pgid);
        fg_pgid = -1;
    }    

    return last_exit_status;
}
