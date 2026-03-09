// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>
#include "prompt.h"

static void expand_escapes(const char *src, char *dest, size_t dest_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dest_size - 1; i++) {
        if (src[i] == '\\' && src[i+1]) {
            i++;
            switch (src[i]) {
                case 'n': dest[j++] = '\n'; break;
                case 't': dest[j++] = '\t'; break;
                case 'r': dest[j++] = '\r'; break;
                case 'a': dest[j++] = '\a'; break;
                case 'v': dest[j++] = '\v'; break;
                case '\\': dest[j++] = '\\'; break;
                case '"': dest[j++] = '"'; break;
                case '$': dest[j++] = '$'; break;
                case 'x': {
                    i++;
                    if (isxdigit(src[i])) {
                        int val = 0;
                        for (int k = 0; k < 2 && isxdigit(src[i]); k++, i++) {
                            char c = src[i];
                            val *= 16;
                            if (c >= '0' && c <= '9') val += c - '0';
                            else if (c >= 'a' && c <= 'f') val += c - 'a' + 10;
                            else if (c >= 'A' && c <= 'F') val += c - 'A' + 10;
                        }
                        i--;
                        dest[j++] = (char)val;
                    } else {
                        dest[j++] = 'x';
                        i--;
                    }
                    break;
                }
                default: dest[j++] = src[i]; break;
            }
        } else {
            dest[j++] = src[i];
        }
    }
    dest[j] = '\0';
}

const char* get_prompt(void) {
    static char prompt[2048];

    if (strcmp(cvx_prompt, "DEFAULT_PROMPT") == 0 || cvx_prompt[0] == '\0') {
        char home[1024], hostname[256];
        struct passwd *pw = getpwuid(getuid());
        const char *username = pw ? pw->pw_name : "unknown";
        gethostname(hostname, sizeof(hostname));
        strncpy(home, getenv("HOME") ? getenv("HOME") : "", sizeof(home));
        home[sizeof(home)-1] = '\0';

        if (strncmp(current_dir, home, strlen(home)) == 0) {
            snprintf(prompt, sizeof(prompt), "%s@%s:~%s|> ", username, hostname, current_dir + strlen(home));
        } else {
            snprintf(prompt, sizeof(prompt), "%s@%s:%s|> ", username, hostname, current_dir);
        }
        return prompt;     
    }   

    char tmp[2048];
    char buf[2048] = "";
    char expanded[2048];

    strncpy(tmp, cvx_prompt, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';

    char *p = tmp;
    while (*p) {
        if (*p == '$') {
            if (strncmp(p, "$USER", 5) == 0) {
                strncat(buf, getenv("USER") ? getenv("USER") : "", sizeof(buf)-strlen(buf)-1);
                p += 5;
                continue;
            } else if (strncmp(p, "$HOST", 5) == 0) {
                char hostname[256];
                gethostname(hostname, sizeof(hostname));
                strncat(buf, hostname, sizeof(buf)-strlen(buf)-1);
                p += 5;
                continue;
            } else if (strncmp(p, "$PWD", 4) == 0) {
                char pwd[1024];
                getcwd(pwd, sizeof(pwd));
                const char *home = getenv("HOME");
                if (home && strncmp(pwd, home, strlen(home)) == 0) {
                    char tmp2[1024];
                    snprintf(tmp2, sizeof(tmp2), "~%s", pwd + strlen(home));
                    strncat(buf, tmp2, sizeof(buf)-strlen(buf)-1);
                } else {
                    strncat(buf, pwd, sizeof(buf)-strlen(buf)-1);
                }
                p += 4;
                continue;
            } else {
                char varname[128] = "";
                int i = 0;
                p++;
                while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || 
                       (*p >= '0' && *p <= '9') || *p == '_') {
                    if (i < 127) varname[i++] = *p;
                    p++;
                }
                varname[i] = '\0';
                const char *val = getenv(varname);
                if (val) strncat(buf, val, sizeof(buf)-strlen(buf)-1);
                continue;
            }
        }
        
        int len = strlen(buf);
        if (len < (int)sizeof(buf) - 1) {
            buf[len] = *p;
            buf[len + 1] = '\0';
        }
        p++;
    }

    expand_escapes(buf, expanded, sizeof(expanded));
    snprintf(prompt, sizeof(prompt), "%s", expanded);
    return prompt;
}
