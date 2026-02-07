// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]

#include "signals.h"
#include "config.h"
#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "jobs.h"
#include <signal.h>
#include <termios.h>
#include <sys/wait.h>
#include <ctype.h>

char previous_dir[1024] = "";

static int parse_job_id(const char *arg) {
    if (!arg)
        return jobs_last_id();

    if (arg[0] == '%')
        return atoi(arg + 1);

    return atoi(arg);
}

static void print_with_escapes(const char *s) {
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\\' && s[i+1]) {
            i++;
            switch (s[i]) {
                case 'n': putchar('\n'); break;
                case 't': putchar('\t'); break;
                case 'r': putchar('\r'); break;
                case 'a': putchar('\a'); break;
                case 'v': putchar('\v'); break;
                case '\\': putchar('\\'); break;
                case '"': putchar('"'); break;
                case '$': putchar('$'); break;
                case 'x': {
                    int val = 0;
                    for (int k = 0; k < 2; k++) {
                        char c = s[i+1];
                        if (!c || !isxdigit(c)) break;
                        i++;
                        if (c >= '0' && c <= '9') val = val*16 + (c-'0');
                        else if (c >= 'a' && c <= 'f') val = val*16 + (c-'a'+10);
                        else if (c >= 'A' && c <= 'F') val = val*16 + (c-'A'+10);
                    }
                    putchar(val);
                    break;
                }
                default: putchar(s[i]); break;
            }
        } else {
            putchar(s[i]);
        }
    }
}

    

int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        print_with_escapes(argv[i]);
        if (i < argc - 1) putchar(' ');
    }
    putchar('\n');
    return 0;
}

int cmd_cd(int argc, char **argv) {
    char *target;
    char cwd[1024];
    char path[1024];

    if (getcwd(cwd, sizeof(cwd)) == NULL) cwd[0] = '\0';

    if (argc < 2) {
        target = getenv("HOME");
        if (!target) target = "/";
    } else if (strcmp(argv[1], "-") == 0) {
        target = previous_dir[0] ? previous_dir : getenv("HOME");
        if (!target) target = "/";
        printf("%s\n", target);
    } else if (argv[1][0] == '~') {
        char *home = getenv("HOME");
        if (!home) home = "/";
        if (argv[1][1] == '/' || argv[1][1] == '\0') {
            snprintf(path, sizeof(path), "%s%s", home, argv[1] + 1);
            target = path;
        } else {
            fprintf(stderr, "cd: unsupported format: %s\n", argv[1]);
            return 1;
        }
    } else {
        target = argv[1];
    }

    if (chdir(target) != 0) {
        perror("cd");
        return 1;
    }

    strncpy(previous_dir, cwd, sizeof(previous_dir)-1);
    previous_dir[sizeof(previous_dir)-1] = '\0';

    if (getcwd(current_dir, sizeof(current_dir)) == NULL) current_dir[0] = '\0';
    return 0;
}

int cmd_pwd(int argc, char **argv) {
    int physical = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-P") == 0) physical = 1;
        else if (strcmp(argv[i], "-L") == 0) physical = 0;
        else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: pwd [OPTION]...\n");
            printf("Print the name of the current working directory.\n\n");
            printf("Options:\n");
            printf("  -L      print the logical current working directory (default)\n");
            printf("  -P      print the physical current working directory (resolving symlinks)\n");
            printf("      --help     display this help and exit\n");
            return 0;
        }
    }

    if (physical) {
        char real[1024];
        if (realpath(current_dir, real)) printf("%s\n", real);
        else perror("pwd -P error");
    } else {
        printf("%s\n", current_dir);
    }

    return 0;
}

int cmd_export(int argc, char **argv) {
    if (argc < 2) {
        printf("export: usage: export VAR=value\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (eq) {
            *eq = '\0';
            if (setenv(argv[i], eq + 1, 1) != 0) perror("export");
        } else {
            const char *val = getenv(argv[i]);
            if (val) printf("%s=%s\n", argv[i], val);
            else fprintf(stderr, "export: %s not set\n", argv[i]);
        }
    }

    return 0;
}

int cmd_history(int argc, char **argv) {
    const char *home = getenv("HOME");
    char path[1024];
    snprintf(path, sizeof(path), "%s/.cvx_history", home ? home : ".");

    FILE *f = fopen(path, "r");
    if (!f) { perror("No history"); return 1; }

    char lines[1024][1024];
    int count = 0;
    while (fgets(lines[count], sizeof(lines[count]), f) && count < 1024) {
        lines[count][strcspn(lines[count], "\n")] = 0;
        count++;
    }
    fclose(f);

    if (argc == 2 && argv[1][0] == '!') {
        int index = -1;
        if (argv[1][1] == '-') index = count - atoi(argv[1]+2);
        else index = atoi(argv[1]+1) - 1;
        if (index >= 0 && index < count) {
            printf("%s\n", lines[index]);
        } else {
            fprintf(stderr, "history: invalid index\n");
        }
        return 0;
    }

    for (int i = 0; i < count; i++) printf("%d  %s\n", i+1, lines[i]);
    return 0;
}

int cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("\nCVX Shell Help:\n");
    printf("Built-in commands:\n");
    printf("  cd [dir]       - change current directory\n");
    printf("  pwd [-L|-P|-help] - print current directory\n");
    printf("  ls             - list files\n");
    printf("  history [!N]   - show or repeat command history\n");
    printf("  echo [args]    - print arguments (supports escapes)\n");
    printf("  export [VAR=value] - set or show environment variable\n");
    printf("  jobs           - list background and stopped jobs\n");
    printf("  && and |       - command chaining and pipelines\n");
    printf("  # [comment]    - inline comments\n");
    printf("  command &      - run command in background\n\n");
    printf("  cvx --version, cvx -version, cvx -v  - show shell version\n");
    printf("  help, cvx --help, cvx -help, cvx -h - show this help message\n\n");
    printf("External commands can be executed as usual.\n");
    return 0;
}

int cmd_ls(int argc, char **argv) {
    char *args[128];
    int new_argc = argc;
    bool has_color = false;

    for (int i = 0; i < argc; i++) {
        args[i] = argv[i];
        if (strcmp(argv[i], "--color=auto") == 0) has_color = true;
    }

    if (!has_color && new_argc < 127) {
        args[new_argc++] = "--color=auto";
    }

    args[new_argc] = NULL;

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) { execvp("ls", args); perror("execvp"); exit(EXIT_FAILURE); }
    else { int status; waitpid(pid, &status, 0); return WIFEXITED(status) ? WEXITSTATUS(status) : 1; }
}

int cmd_jobs(int argc, char **argv) {
    (void)argc;
    (void)argv;
    jobs_cleanup();
    jobs_list();
    return 0;
}


int cmd_fg(int argc, char **argv) {
    int id = parse_job_id(argc >= 2 ? argv[1] : NULL);

    pid_t pgid = jobs_get_pgid(id);
    if (pgid <= 0) {
        fprintf(stderr, "fg: no such job: %d\n", id);
        return 1;
    }

    tcsetpgrp(STDIN_FILENO, pgid);
    kill(-pgid, SIGCONT);
    jobs_set_state(pgid, JOB_RUNNING);

    int status;
    waitpid(-pgid, &status, WUNTRACED);

    tcsetpgrp(STDIN_FILENO, getpgrp());

    if (WIFSTOPPED(status))
        jobs_set_state(pgid, JOB_STOPPED);
    else
        jobs_cleanup();

    return 0;
}


int cmd_bg(int argc, char **argv) {
    int id = parse_job_id(argc >= 2 ? argv[1] : NULL);

    pid_t pgid = jobs_get_pgid(id);
    if (pgid <= 0) {
        fprintf(stderr, "bg: no such job: %d\n", id);
        return 1;
    }

    kill(-pgid, SIGCONT);
    jobs_set_state(pgid, JOB_RUNNING);

    return 0;
}

static void alias_usage() {
    printf("Usage: alias <name>-<command>\n");
    printf("Example: alias ll-ls -l\n");
    printf("Options:\n");
    printf("  -h, --help, -help    Show this help message\n");
}

int cmd_alias(int argc, char **argv) {
    if (argc < 2) {
        for (int i = 0; i < alias_count; i++) {
            printf("alias %s='%s'\n", aliases[i].name, aliases[i].command);
        }
        return 0;
    }

    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-help")) {
        alias_usage();
        return 0;
    }

    
    char full_arg[1024] = "";
    for (int i = 1; i < argc; i++) {
        strncat(full_arg, argv[i], sizeof(full_arg) - strlen(full_arg) - 1);
        if (i < argc - 1) strncat(full_arg, " ", sizeof(full_arg) - strlen(full_arg) - 1);
    }

    char *sep = strchr(full_arg, '-');
    if (!sep) {
        printf("Error: Invalid alias format.\n");
        alias_usage();
        return 1;
    }

    char *home = getenv("HOME");
    if (!home) return 1;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.cvx.conf", home);

    FILE *f = fopen(path, "a");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fprintf(f, "ALIAS=\"%s\"\n", full_arg);
    fclose(f);

    printf("Alias added persistently to %s\n", path);
    return 0;
}

int cmd_unalias(int argc, char **argv) {
    if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-help")) {
        printf("Usage: unalias <name>\n");
        return argc < 2 ? 1 : 0;
    }

    char *home = getenv("HOME");
    if (!home) return 1;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.cvx.conf", home);

    FILE *f = fopen(path, "r");
    if (!f) return 1;

    char temp_path[1100];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
    FILE *tmp = fopen(temp_path, "w");
    if (!tmp) {
        fclose(f);
        return 1;
    }

    char line[1024];
    char search[128];
    snprintf(search, sizeof(search), "ALIAS=\"%s-", argv[1]);
    bool found = false;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, search, strlen(search)) == 0) {
            found = true;
            continue;
        }
        fputs(line, tmp);
    }

    fclose(f);
    fclose(tmp);

    if (found) {
        if (rename(temp_path, path) != 0) {
            perror("rename");
            return 1;
        }
        printf("Alias '%s' removed from %s\n", argv[1], path);
    } else {
        remove(temp_path);
        printf("unalias: %s: not found\n", argv[1]);
    }

    return 0;
}
