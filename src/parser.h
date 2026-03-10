// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

ASTNode* parse_ast(const char *line);
int process_command_line(char *line);

#endif
