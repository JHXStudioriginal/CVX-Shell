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
#include "parser.h"
#include "commands.h"
#include "jobs.h"
#include "config.h"
#include "linenoise.h"

static pid_t shell_pgid = -1;
static pid_t fg_pgid = -1;

int exec_command(char *cmdline, bool background) {
    if (!cmdline || !*cmdline)
        return 0;

    if (shell_pgid == -1)
        shell_pgid = getpgrp();

    char *args[64];
    int argc = split_args(cmdline, args, 64);

    replace_alias(args, &argc);
    unescape_args(args, argc);

    for (int i = 0; i < argc; i++) {
        char *tmp = expand_variables(args[i]);
        free(args[i]);
        args[i] = tmp;
    }

    bool has_redirect = false;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(args[i], ">") || !strcmp(args[i], ">>") ||
            !strcmp(args[i], "<") || !strcmp(args[i], "<<")) {
            has_redirect = true;
            break;
        }
    }

    if (!has_redirect) {
        if (!strcmp(args[0], "cd")) return cmd_cd(argc, args);
        if (!strcmp(args[0], "pwd")) return cmd_pwd(argc, args);
        if (!strcmp(args[0], "export")) return cmd_export(argc, args);
        if (!strcmp(args[0], "help")) return cmd_help(argc, args);
        if (!strcmp(args[0], "ls")) return cmd_ls(argc, args);
        if (!strcmp(args[0], "history")) return cmd_history(argc, args);
        if (!strcmp(args[0], "echo")) return cmd_echo(argc, args);
        if (!strcmp(args[0], "jobs")) return cmd_jobs(argc, args);
        if (!strcmp(args[0], "fg")) return cmd_fg(argc, args);
        if (!strcmp(args[0], "bg")) return cmd_bg(argc, args);
        if (!strcmp(args[0], "alias")) return cmd_alias(argc, args);
        if (!strcmp(args[0], "unalias")) return cmd_unalias(argc, args);
        if (!strcmp(args[0], "exit")) exit(0);
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

        for (int i = 0; i < argc; i++) {
            char *tmp = expand_tilde(args[i]);
            free(args[i]);
            args[i] = tmp;
        }

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
    
        tcsetpgrp(STDIN_FILENO, shell_pgid);
    } else {
        fg_pgid = pid;
        tcsetpgrp(STDIN_FILENO, fg_pgid);

        waitpid(-fg_pgid, &status, WUNTRACED);

        if (WIFSTOPPED(status) || WIFSIGNALED(status))
            write(STDOUT_FILENO, "\n", 1);

        if (WIFSTOPPED(status))
        jobs_add(fg_pgid, cmdline, JOB_STOPPED);

        tcsetpgrp(STDIN_FILENO, shell_pgid);
        fg_pgid = -1;
    }

    free_args(args, argc);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 0;
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

            char *args[64];
            int argc = split_args(cmds[i], args, 64);

            replace_alias(args, &argc);
            unescape_args(args, argc);

            for (int j = 0; j < argc; j++) {
                char *tmp = expand_variables(args[j]);
                free(args[j]);
                args[j] = tmp;
            }

            for (int j = 0; j < argc; j++) {
                char *tmp = expand_tilde(args[j]);
                free(args[j]);
                args[j] = tmp;
            }

            handle_redirection(args, &argc);

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
            if (WIFSTOPPED(status) || WIFSIGNALED(status)) {
                write(STDOUT_FILENO, "\n", 1);
            }
    
            if (WIFSTOPPED(status)) {
                jobs_add(fg_pgid, cmds[0], JOB_STOPPED);
                break;
            }
        }
    
        tcsetpgrp(STDIN_FILENO, shell_pgid);
        fg_pgid = -1;
    }    

    return 0;
}
