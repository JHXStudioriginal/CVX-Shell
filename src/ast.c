// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the GNU General Public License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/CVX-Shell/blob/main/LICENSE]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fnmatch.h>

#include "ast.h"
#include "exec.h"
#include "functions.h"
#include "utils.h"
#include <unistd.h>
#include <sys/wait.h>

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

static char *expand_and_unquote(const char *input) {
    if (!input) return NULL;
    char *args[16];
    int count = split_args(input, args, 16);
    if (count == 0) return strdup("");
    for (int i = 0; i < count; i++) {
        char *expanded = expand_variables(args[i]);
        free(args[i]);
        args[i] = expanded;
    }
    quote_removal(args, count);
    int len = 0;
    for (int i = 0; i < count; i++) len += strlen(args[i]) + 1;
    char *res = malloc(len + 1);
    if (!res) { free_args(args, count); return NULL; }
    res[0] = '\0';
    for (int i = 0; i < count; i++) {
        strcat(res, args[i]);
        if (i < count - 1) strcat(res, " ");
    }
    free_args(args, count);
    return res;
}

int execute_ast(ASTNode *node, bool background) {
    if (!node) return 0;

    switch (node->type) {
        case AST_SEQUENCE: {
            execute_ast(node->left, false);
            if (loop_control != 0 || sigint_received) return 0;
            return execute_ast(node->right, background);
        }
        case AST_BACKGROUND: {
            return execute_ast(node->left, true);
        }
        case AST_AND: {
            int status = execute_ast(node->left, false);
            if (loop_control != 0 || sigint_received) return 0;
            if (status == 0) {
                return execute_ast(node->right, background);
            }
            return status;
        }
        case AST_OR: {
            int status = execute_ast(node->left, false);
            if (loop_control != 0 || sigint_received) return 0;
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
            if (loop_control != 0 || sigint_received) return 0;
            if (status == 0) {
                return execute_ast(node->left, background);
            } else if (node->right) {
                return execute_ast(node->right, background);
            }
            return 0;
        }
        case AST_CASE: {
            char *expanded_word = expand_and_unquote(node->cmd);
            ASTNode *item = node->left;
            while (item) {
                bool match = false;
                char *expanded_pattern = expand_and_unquote(item->cmd);
                if (expanded_pattern && strcmp(expanded_pattern, "*") == 0) match = true;
                else if (expanded_pattern && expanded_word) {
                    char *p_copy = strdup(expanded_pattern);
                    char *tok = strtok(p_copy, "|");
                    while (tok) {
                        while (*tok == ' ' || *tok == '\t' || *tok == '\n') tok++;
                        char *end = tok + strlen(tok) - 1;
                        while (end > tok && (*end == ' ' || *end == '\t' || *end == '\n')) { *end = '\0'; end--; }
                        if (strcmp(tok, expanded_word) == 0 || fnmatch(tok, expanded_word, 0) == 0) { match = true; break; }
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
        case AST_WHILE: {
            int status = 0;
            while (execute_ast(node->cond, false) == 0) {
                if (sigint_received) break;
                status = execute_ast(node->left, background);
                if (loop_control == 1) { loop_control = 0; break; }
                if (loop_control == 2) { loop_control = 0; continue; }
                if (sigint_received) break;
            }
            return status;
        }
        case AST_UNTIL: {
            int status = 0;
            while (execute_ast(node->cond, false) != 0) {
                if (sigint_received) break;
                status = execute_ast(node->left, background);
                if (loop_control == 1) { loop_control = 0; break; }
                if (loop_control == 2) { loop_control = 0; continue; }
                if (sigint_received) break;
            }
            return status;
        }
        case AST_FOR: {
            int status = 0;
            char *list_expanded = node->cmd ? expand_variables(node->cmd) : strdup("");
            char *args[256];
            int count = split_args(list_expanded, args, 256);
            free(list_expanded);
            
            expand_glob(args, &count, 256);
            quote_removal(args, count);
            
            for (int i = 0; i < count; i++) {
                if (sigint_received) break;
                setenv(node->name, args[i], 1);
                status = execute_ast(node->left, background);
                if (loop_control == 1) { loop_control = 0; break; }
                if (loop_control == 2) { loop_control = 0; continue; }
                if (sigint_received) break;
            }
            
            free_args(args, count);
            return status;
        }
        case AST_NEGATION: {
            last_exit_status = (execute_ast(node->left, background) == 0 ? 1 : 0);
            return last_exit_status;
        }
        case AST_SUBSHELL: {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                return 1;
            }
            if (pid == 0) {
                
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                int status = execute_ast(node->left, false);
                exit(status);
            }
            
            int status = 0;
            if (background) {
                
                
                return 0; 
            } else {
                waitpid(pid, &status, 0);
                last_exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 0);
                return last_exit_status;
            }
        }
        default:
        
            return 1;
    }
}
