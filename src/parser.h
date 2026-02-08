// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]


#ifndef PARSER_H
#define PARSER_H

int split_args(const char *line, char *args[], int max_args);
void free_args(char *args[], int argc);
char* unescape_string(const char *s);
void unescape_args(char *args[], int argc);
char* expand_tilde(const char *path);
char* expand_variables(const char *input);
void replace_alias(char *args[], int *argc);
void handle_redirection(char *args[], int *argc);
void process_single_command(char *line);
void process_command_line(char *line);
char* expand_history(const char *line, const char *last_command);

#endif
