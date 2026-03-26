// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]

#ifndef EXEC_H
#define EXEC_H

#include "parser.h"
#include "commands.h"
#include "config.h"
#include "signals.h"
#include "linenoise.h"

#include <signal.h>

extern int last_exit_status;
extern int loop_control;
extern volatile sig_atomic_t sigint_received;

int exec_command(char *cmdline, bool background);
int execute_pipeline(char **cmds, int n, bool background);

#endif
