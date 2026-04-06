// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]

#include "signals.h"
#include "config.h"
#include "commands.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "parser.h"
#include "jobs.h"
#include <signal.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ctype.h>

extern int last_exit_status;

char previous_dir[1024] = "";

static int parse_job_id(const char *arg) {
    if (!arg)
        return jobs_last_id();

    if (arg[0] == '%')
        return atoi(arg + 1);

    return atoi(arg);
}

static void print_with_escapes(const char *s, bool interpret) {
    if (!s) return;
    for (int i = 0; s[i]; i++) {
        if (interpret && s[i] == '\\' && s[i+1]) {
            i++;
            switch (s[i]) {
                case 'n': putchar('\n'); break;
                case 'r': putchar('\r'); break;
                case 't': putchar('\t'); break;
                case '\\': putchar('\\'); break;
                case 'a': putchar('\a'); break;
                case 'b': putchar('\b'); break;
                case 'f': putchar('\f'); break;
                case 'v': putchar('\v'); break;
                default: putchar('\\'); putchar(s[i]); break;
            }
        } else {
            putchar(s[i]);
        }
    }
}

int cmd_echo(int argc, char **argv) {
    bool interpret = false;
    bool newline = true;
    int start = 1;

    while (start < argc && argv[start][0] == '-') {
        bool possible_flag = true;
        for (int j = 1; argv[start][j]; j++) {
            if (argv[start][j] == 'e') interpret = true;
            else if (argv[start][j] == 'n') newline = false;
            else if (argv[start][j] == 'E') interpret = false;
            else { possible_flag = false; break; }
        }
        if (!possible_flag) break;
        start++;
    }

    for (int i = start; i < argc; i++) {
        print_with_escapes(argv[i], interpret);
        if (i < argc - 1) putchar(' ');
    }
    if (newline) putchar('\n');
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
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: pwd [OPTION]...\n");
            printf("Print the name of the current working directory.\n\n");
            printf("Options:\n");
            printf("  -L      print the logical current working directory (default)\n");
            printf("  -P      print the physical current working directory (resolving symlinks)\n");
            printf("      --help, -help, -h     display this help message\n");
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
    printf("\nCVX Shell Help - Built-in commands:\n");
    printf("  cd [dir]                - Change directory (supports ~)\n");
    printf("  pwd [-L|-P|--help]      - Print working directory (logical/physical)\n");
    printf("  help                    - Show this help message\n");
    printf("  ls                      - List directory contents (auto --color=auto)\n");
    printf("  history                 - Show command history\n");
    printf("  alias [<name>=<cmd>]    - Create a command alias\n");
    printf("  unalias [name]          - Remove the specified alias\n");
    printf("  echo [args]             - Display text (supports environment variables)\n");
    printf("  export [VAR=value]      - Set environment variables\n");
    printf("  jobs                    - List background jobs\n");
    printf("  fg                      - Resume job in foreground\n");
    printf("  bg                      - Resume job in background\n");
    printf("  functions               - List all defined functions\n");
    printf("  delfunc [name]          - Delete the specified function\n");
    printf("  break [n]               - Exit from within a for, while, or until loop\n");
    printf("  continue [n]            - Resume the next iteration of an enclosing loop\n");
    printf("  :                       - Null command (returns 0 exit status)\n");
    printf("  eval [arg ...]          - Combine arguments into a single command and execute it\n");
    printf("  exec [command] [args]   - Replace the shell with the specified command\n");
    printf("  exit                    - Exit the shell\n\n");
    printf("External commands can be executed as usual via PATH.\n");
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

int cmd_exit(int argc, char **argv) {
    int status = last_exit_status;
    if (argc > 1) {
        char *endptr;
        long val = strtol(argv[1], &endptr, 10);
        if (*endptr == '\0') {
            status = (int)val;
        } else {
            fprintf(stderr, "cvx: exit: %s: numeric argument required\n", argv[1]);
            status = 2; 
        }
    }
    exit(status);
    return 0;
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

int cmd_test(int argc, char **argv) {
    if (argc < 2) return 1;

    
    if (argc > 2 && strcmp(argv[1], "!") == 0) {
        return cmd_test(argc - 1, argv + 1) == 0 ? 1 : 0;
    }
    
    if (argc == 2) {
        return (strlen(argv[1]) > 0) ? 0 : 1;
    }
    
    if (argc == 3) {
        if (strcmp(argv[1], "-f") == 0) {
            struct stat st;
            if (stat(argv[2], &st) == 0 && S_ISREG(st.st_mode)) return 0;
            return 1;
        }
        if (strcmp(argv[1], "-d") == 0) {
            struct stat st;
            if (stat(argv[2], &st) == 0 && S_ISDIR(st.st_mode)) return 0;
            return 1;
        }
        if (strcmp(argv[1], "-z") == 0) return (strlen(argv[2]) == 0) ? 0 : 1;
        if (strcmp(argv[1], "-n") == 0) return (strlen(argv[2]) > 0) ? 0 : 1;
        if (strcmp(argv[1], "!") == 0) return (strlen(argv[2]) == 0) ? 0 : 1;
    }
    
    if (argc == 4) {
        if (strcmp(argv[2], "=") == 0 || strcmp(argv[2], "==") == 0) return (strcmp(argv[1], argv[3]) == 0) ? 0 : 1;
        if (strcmp(argv[2], "!=") == 0) return (strcmp(argv[1], argv[3]) != 0) ? 0 : 1;
        
        char *end1, *end2;
        long n1 = strtol(argv[1], &end1, 10);
        long n2 = strtol(argv[3], &end2, 10);
        
        if (strcmp(argv[2], "-eq") == 0) return (n1 == n2) ? 0 : 1;
        if (strcmp(argv[2], "-ne") == 0) return (n1 != n2) ? 0 : 1;
        if (strcmp(argv[2], "-gt") == 0) return (n1 > n2) ? 0 : 1;
        if (strcmp(argv[2], "-lt") == 0) return (n1 < n2) ? 0 : 1;
        if (strcmp(argv[2], "-ge") == 0) return (n1 >= n2) ? 0 : 1;
        if (strcmp(argv[2], "-le") == 0) return (n1 <= n2) ? 0 : 1;
    }
    
    return 1;
}

int cmd_bracket(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[argc-1], "]") != 0) {
        fprintf(stderr, "[: expected ']' as last argument\n");
        return 1;
    }
    argv[argc-1] = NULL;
    return cmd_test(argc - 1, argv);
}

int cmd_set(int argc, char **argv) {
    if (argc == 1) {
        return 0;
    }

    int start_idx = 1;
    if (strcmp(argv[1], "--") == 0) {
        start_idx = 2;
    }

    set_current_param_frame(argc - start_idx, argv + start_idx);
    return 0;
}

int cmd_exec(int argc, char **argv) {
    if (argc < 2) return 0;
    execvp(argv[1], &argv[1]);
    perror("exec");
    return 1;
}

int cmd_eval(int argc, char **argv) {
    if (argc < 2) return 0;
    size_t total_len = 0;
    for (int i = 1; i < argc; i++) total_len += strlen(argv[i]) + 1;
    char *cmd = malloc(total_len + 1);
    if (!cmd) return 1;
    cmd[0] = '\0';
    for (int i = 1; i < argc; i++) {
        strcat(cmd, argv[i]);
        if (i < argc - 1) strcat(cmd, " ");
    }
    int status = process_command_line(cmd);
    free(cmd);
    return status;
}
