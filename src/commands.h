// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]


#ifndef COMMANDS_H
#define COMMANDS_H

int cmd_cd(int argc, char **argv);
int cmd_ls(int argc, char **argv);
int cmd_pwd(int argc, char **argv);
int cmd_echo(int argc, char **argv);
int cmd_export(int argc, char **argv);
int cmd_help(int argc, char **argv);
int cmd_history(int argc, char **argv);
int cmd_jobs(int argc, char **argv);
int cmd_fg(int argc, char **argv);
int cmd_bg(int argc, char **argv);
int cmd_alias(int argc, char **argv);
int cmd_unalias(int argc, char **argv);

#endif
