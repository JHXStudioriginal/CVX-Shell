// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]

#ifndef AST_H
#define AST_H

#include <stdbool.h>

typedef enum {
    AST_COMMAND,
    AST_PIPELINE,
    AST_AND,
    AST_OR,
    AST_SEQUENCE,
    AST_BACKGROUND,
    AST_FUNCDEF,
    AST_IF,
    AST_IF_BODY,
    AST_CASE,
    AST_CASE_ITEM,
    AST_NEGATION
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    char *cmd;
    char *name;
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *cond;
} ASTNode;

void free_ast(ASTNode *node);
int execute_ast(ASTNode *node, bool background);

#endif
