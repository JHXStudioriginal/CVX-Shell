// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include "config.h"
#include "prompt.h"
#include "exec.h"
#include "signals.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "utils.h"
#include "linenoise.h"

static char *last_command = NULL;

static void load_profile(const char *path) {
    if (access(path, R_OK) == 0) {
        FILE *f = fopen(path, "r");
        if (!f) return;

        char line_buf[4096];
        while (fgets(line_buf, sizeof(line_buf), f)) {
            line_buf[strcspn(line_buf, "\r\n")] = 0;
            char *p = line_buf + strlen(line_buf);
            while (p > line_buf && (*(p-1) == ' ' || *(p-1) == '\t')) p--;
            *p = '\0';
            p = line_buf;
            while (*p == ' ' || *p == '\t') p++;
            if (*p != '\0' && *p != '#') {
                if (process_command_line(p) != 0) {
                    fprintf(stderr, "Failed to load profile\n");
                    break;
                }
            }
        }
        fclose(f);
    }
}



int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT,  SIG_IGN);
    if (argc > 1 &&
        (strcmp(argv[1], "--version") == 0 ||
         strcmp(argv[1], "-v") == 0 ||
         strcmp(argv[1], "-version") == 0)) {

        printf("CVX Shell beta 0.8.5\n");
        printf("Copyright (C) 2025-2026 JHX Studio's\n");
        printf("License: Elasna Open Source License v2\n");
        return 0;
    }

    if (argc > 1 &&
        (strcmp(argv[1], "--help") == 0 ||
         strcmp(argv[1], "-help") == 0 ||
         strcmp(argv[1], "-h") == 0)) {
        process_command_line("help");
        return 0;
    }

    if (argc > 2 && strcmp(argv[1], "-c") == 0) {
        char cmd[4096] = {0};
        for (int i = 2; i < argc; i++) {
            strncat(cmd, argv[i], sizeof(cmd) - strlen(cmd) - 1);
            if (i < argc - 1)
                strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
        }
        process_command_line(cmd);
        return 0;
    }    

    if (argc > 1 && strcmp(argv[1], "-l") == 0) {
        load_profile("/etc/profile");
        const char *home = getenv("HOME");
        if (home) {
            char user_profile[1024];
            snprintf(user_profile, sizeof(user_profile), "%s/.profile", home);
            load_profile(user_profile);
        }
    }

    if (argc > 1 && argv[1][0] != '-') {
        FILE *f = fopen(argv[1], "r");
        if (!f) {
            perror("Cannot open file.");
            return 1;
        }

        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        char *buffer = malloc(fsize + 1);
        if (!buffer) {
            perror("malloc");
            fclose(f);
            return 1;
        }

        size_t bytes_read = fread(buffer, 1, fsize, f);
        fclose(f);
        buffer[bytes_read] = '\0';

        push_param_frame(argc - 1, argv + 1);
        process_command_line(buffer);
        pop_param_frame();
        free(buffer);
        return 0;
    }

    char *line;
    setenv("TERM", "xterm", 1);

    setup_signals();

    pid_t shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, shell_pgid);

    const char *history_file = ".cvx_history";
    char history_path[1024];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(history_path, sizeof(history_path), "%s/%s", home, history_file);
    linenoiseHistoryLoad(history_path);
    getcwd(current_dir, sizeof(current_dir));

    config();

    linenoiseSetMultiLine(1);

    while (1) {
        check_and_reload_config();
        const char *prompt = get_prompt();
        char *full_line = NULL;
        size_t full_len = 0;

        while (1) {
            line = linenoise(full_line == NULL ? prompt : "> ");
            if (line == NULL) {
                if (full_line) free(full_line);
                goto end_shell;
            }

            char *p = line + strlen(line);
            while (p > line && (*(p-1) == ' ' || *(p-1) == '\t')) p--;
            *p = '\0';

            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\\') {
                line[len - 1] = '\0';
                len--;

                size_t new_len = full_len + len + 1;
                char *new_full = realloc(full_line, new_len);
                if (!new_full) {
                    perror("realloc");
                    free(line);
                    break;
                }
                full_line = new_full;
                memcpy(full_line + full_len, line, len);
                full_len += len;
                full_line[full_len] = '\0';
                free(line);
                continue;
            } else {
                size_t new_len = full_len + len + 2;
                char *new_full = realloc(full_line, new_len);
                if (!new_full) {
                    perror("realloc");
                    free(line);
                    break;
                }
                full_line = new_full;
                if (full_len > 0 && full_line[full_len-1] != '\n') {
                    full_line[full_len++] = '\n';
                }
                memcpy(full_line + full_len, line, len);
                full_len += len;
                full_line[full_len] = '\0';
                free(line);

                if (!is_block_complete(full_line)) {
                    continue;
                }
                break;
            }
        }

        line = full_line;
        char *expanded_line = expand_history(line, last_command);
        if (strcmp(line, expanded_line) != 0) {
            printf("%s\n", expanded_line);
        }
        free(line);
        line = expanded_line;
        if (strcmp(line, "exit") == 0) {
            free(line);
            break;
        }
        if (line[0] != '\0' && history_enabled) {
            char *ptr = line;
            while (*ptr == ' ' || *ptr == '\t') ptr++;
            if (*ptr != '\0' && *ptr != '#') {
                linenoiseHistoryAdd(line);
                linenoiseHistorySave(history_path);
            }
        }
        char *original_line = NULL;
        if (line[0] != '\0') {
            char *ptr = line;
            while (*ptr == ' ' || *ptr == '\t') ptr++;
            if (*ptr != '\0' && *ptr != '#') {
                original_line = strdup(line);
            }
        }
        if (original_line) {
            free(last_command);
            last_command = strdup(original_line);
        }

        process_command_line(line);
        if (original_line) {
            free(original_line);
        }
        free(line);
    }
    end_shell:
    return 0;
}
