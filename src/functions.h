// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]


#ifndef FUNCTIONS_H
#define FUNCTIONS_H

void add_function(const char *name, const char *body);
const char* get_function(const char *name);
void remove_function(const char *name);
int cmd_functions(int argc, char **argv);
int cmd_delfunc(int argc, char **argv);

#endif
