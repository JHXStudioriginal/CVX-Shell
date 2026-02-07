// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <stdbool.h>
#include <ctype.h>
#include "config.h"

#include <sys/stat.h>
#include <time.h>

char current_dir[512] = "";
char cvx_prompt[2048] = "";
Alias aliases[MAX_ALIASES];
int alias_count = 0;
bool history_enabled = true;
char start_dir[1024] = "";

static time_t global_mtime = 0;
static time_t local_mtime = 0;

static void parse_config_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    struct stat st;
    if (stat(path, &st) == 0) {
        if (strcmp(path, "/etc/cvx.conf") == 0) global_mtime = st.st_mtime;
        else if (strstr(path, "/.cvx.conf")) local_mtime = st.st_mtime;
    }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {

        line[strcspn(line, "\r\n")] = 0;
        char *ptr = line;
        while (*ptr && isspace(*ptr)) ptr++;
        if (*ptr == '#' || *ptr == '\0') continue;

        if (strncmp(ptr, "PROMPT=", 7) == 0) {
            char *value = ptr + 7;
            if (strncmp(value, "default", 7) == 0 && (value[7] == '\0' || isspace(value[7]))) {
                strncpy(cvx_prompt, "DEFAULT_PROMPT", sizeof(cvx_prompt)-1);
                cvx_prompt[sizeof(cvx_prompt)-1] = '\0';
            } else if (*value == '"') {
                value++;
                char *end = strchr(value, '"');
                if (end) *end = '\0';
                strncpy(cvx_prompt, value, sizeof(cvx_prompt)-1);
                cvx_prompt[sizeof(cvx_prompt)-1] = '\0';
            }
        } 
        else if (strncmp(ptr, "ALIAS=", 6) == 0) {
            char *value = ptr + 6;
            if (*value != '"') continue;
            value++;
            char *end = strchr(value, '"');
            if (!end) continue;
            *end = '\0';

            char *sep = strchr(value, '-');
            if (!sep) continue;
            *sep = '\0';
            char *name = value;
            char *command = sep + 1;

            if (alias_count < MAX_ALIASES) {
                aliases[alias_count].name = strdup(name);
                aliases[alias_count].command = strdup(command);
                alias_count++;
            }
        } 
        else if (strncmp(ptr, "STARTDIR=", 9) == 0) {
            char *value = ptr + 9;
            if (*value == '"') value++;
            char *end = strchr(value, '"');
            if (end) *end = '\0';
            strncpy(start_dir, value, sizeof(start_dir)-1);
            start_dir[sizeof(start_dir)-1] = '\0';
            chdir(start_dir);
            if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
                perror("getcwd error");
                current_dir[0] = '\0';
            }
        } 
        else if (strncmp(ptr, "HISTORY=", 8) == 0) {
            char *value = ptr + 8;
            if (strcmp(value, "true") == 0) history_enabled = true;
            else if (strcmp(value, "false") == 0) history_enabled = false;
        }
    }

    fclose(f);
}

void config() {
    
    for (int i = 0; i < alias_count; i++) {
        free(aliases[i].name);
        free(aliases[i].command);
        aliases[i].name = NULL;
        aliases[i].command = NULL;
    }
    alias_count = 0;

    strncpy(cvx_prompt, "DEFAULT_PROMPT", sizeof(cvx_prompt)-1);
    cvx_prompt[sizeof(cvx_prompt)-1] = '\0';

    if (current_dir[0] == '\0') {
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            perror("getcwd error");
            current_dir[0] = '\0';
        }
        strncpy(start_dir, current_dir, sizeof(start_dir)-1);
        start_dir[sizeof(start_dir)-1] = '\0';
    }

    history_enabled = true;

    
    parse_config_file("/etc/cvx.conf");

    
    char *home = getenv("HOME");
    if (home) {
        char user_config[1024];
        snprintf(user_config, sizeof(user_config), "%s/.cvx.conf", home);

        
        if (getuid() != 0 && access(user_config, F_OK) == -1) {
            FILE *def = fopen(user_config, "w");
            if (def) {
                fprintf(def, "PROMPT=default\nHISTORY=true\n");
                fclose(def);
            }
        }

        parse_config_file(user_config);
    }
}

void check_and_reload_config() {
    bool reload = false;
    struct stat st;

    if (stat("/etc/cvx.conf", &st) == 0) {
        if (st.st_mtime > global_mtime) reload = true;
    }

    char *home = getenv("HOME");
    if (home) {
        char user_config[1024];
        snprintf(user_config, sizeof(user_config), "%s/.cvx.conf", home);
        if (stat(user_config, &st) == 0) {
            if (st.st_mtime > local_mtime) reload = true;
        }
    }

    if (reload) {
        config();
    }
}
