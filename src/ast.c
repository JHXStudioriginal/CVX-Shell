// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "ast.h"
#include "exec.h"
#include "functions.h"
#include "utils.h"

extern int last_exit_status;

void free_ast(ASTNode *node) {
    if (!node) return;
    free_ast(node->left);
    free_ast(node->right);
    free_ast(node->cond);
    free(node->cmd);
    free(node->name);
    free(node);
}

static int get_pipeline_cmds(ASTNode *node, char *cmds[], int max_cmds) {
    if (!node) return 0;
    if (node->type == AST_COMMAND) {
        cmds[0] = node->cmd;
        return 1;
    }
    if (node->type == AST_PIPELINE) {
        int n = get_pipeline_cmds(node->left, cmds, max_cmds);
        if (n < max_cmds && node->right && node->right->type == AST_COMMAND) {
            cmds[n++] = node->right->cmd;
        }
        return n;
    }
    return 0;
}

int execute_ast(ASTNode *node, bool background) {
    if (!node) return 0;

    switch (node->type) {
        case AST_SEQUENCE: {
            execute_ast(node->left, false);
            return execute_ast(node->right, background);
        }
        case AST_BACKGROUND: {
            return execute_ast(node->left, true);
        }
        case AST_AND: {
            int status = execute_ast(node->left, false);
            if (status == 0) {
                return execute_ast(node->right, background);
            }
            return status;
        }
        case AST_OR: {
            int status = execute_ast(node->left, false);
            if (status != 0) {
                return execute_ast(node->right, background);
            }
            return status;
        }
        case AST_PIPELINE: {
            char *cmds[64];
            int n = get_pipeline_cmds(node, cmds, 64);
            return execute_pipeline(cmds, n, background);
        }
        case AST_COMMAND: {
            return exec_command(node->cmd, background);
        }
        case AST_FUNCDEF: {
            add_function(node->name, node->cmd);
            return 0;
        }
        case AST_IF_BODY:
        case AST_CASE_ITEM:
            return 0;
        case AST_IF: {
            int status = execute_ast(node->cond, false);
            if (status == 0) {
                return execute_ast(node->left, background);
            } else if (node->right) {
                return execute_ast(node->right, background);
            }
            return 0;
        }
        case AST_CASE: {
            char *expanded_word = expand_variables(node->cmd);
            ASTNode *item = node->left;
            while (item) {
                bool match = false;
                char *expanded_pattern = expand_variables(item->cmd);
                if (strcmp(expanded_pattern, "*") == 0) match = true;
                else {
                    char *p_copy = strdup(expanded_pattern);
                    char *tok = strtok(p_copy, "|");
                    while (tok) {
                        while (*tok == ' ') tok++;
                        char *end = tok + strlen(tok) - 1;
                        while (end > tok && *end == ' ') { *end = '\0'; end--; }
                        if (strcmp(tok, expanded_word) == 0) { match = true; break; }
                        tok = strtok(NULL, "|");
                    }
                    free(p_copy);
                }
                free(expanded_pattern);
                
                if (match) {
                    free(expanded_word);
                    return execute_ast(item->left, background);
                }
                item = item->right;
            }
            free(expanded_word);
            return 0;
        }
        case AST_NEGATION: {
            last_exit_status = (execute_ast(node->left, background) == 0 ? 1 : 0);
            return last_exit_status;
        }
        default:
        
            return 1;
    }
}
