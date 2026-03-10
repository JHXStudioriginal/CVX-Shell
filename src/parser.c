// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lexer.h"
#include "ast.h"
#include "parser.h"

static ASTNode *parse_command(Token **token);
static ASTNode *parse_pipeline(Token **token);
static ASTNode *parse_and_or(Token **token);
static ASTNode *parse_sequence(Token **token);
static ASTNode *parse_if(Token **token);
static ASTNode *parse_case(Token **token);

static ASTNode *parse_if_inner(Token **token, bool expect_fi) {
    consume(token);
    ASTNode *cond = parse_sequence(token);
    if (!match(token, TOK_THEN)) {
        fprintf(stderr, "syntax error: expected 'then'\n");
        return NULL;
    }
    ASTNode *then_branch = parse_sequence(token);
    ASTNode *else_branch = NULL;
    if ((*token)->type == TOK_ELIF) {
        else_branch = parse_if_inner(token, false);
    } else if (match(token, TOK_ELSE)) {
        else_branch = parse_sequence(token);
    }
    
    if (expect_fi && !match(token, TOK_FI)) {
        fprintf(stderr, "syntax error: expected 'fi'\n");
    }
    
    ASTNode *node = calloc(1, sizeof(*node));
    node->type = AST_IF;
    node->cond = cond;
    node->left = then_branch;
    node->right = else_branch;
    return node;
}

static ASTNode *parse_if(Token **token) {
    return parse_if_inner(token, true);
}

static ASTNode *parse_case(Token **token) {
    consume(token);
    if ((*token)->type != TOK_STR) return NULL;
    char *word = strdup((*token)->val);
    consume(token);
    if (!match(token, TOK_IN)) {
        free(word);
        return NULL;
    }
    ASTNode *root = calloc(1, sizeof(*root));
    root->type = AST_CASE;
    root->cmd = word;
    ASTNode **next_item = &root->left;

    while ((*token)->type != TOK_ESAC && (*token)->type != TOK_EOF) {
        Token *p_start = *token;
        while ((*token)->type != TOK_RPAREN && (*token)->type != TOK_EOF) {
            consume(token);
        }
        char *pattern = concat_tokens(p_start, *token);
        consume(token);
        
        ASTNode *body = parse_sequence(token);
        
        ASTNode *item = calloc(1, sizeof(*item));
        item->type = AST_CASE_ITEM;
        item->cmd = pattern;
        item->left = body;
        *next_item = item;
        next_item = &item->right;
        
        if ((*token)->type == TOK_DSEMI) consume(token);
        else if ((*token)->type == TOK_ESAC) break;
    }
    match(token, TOK_ESAC);
    return root;
}

static ASTNode *parse_command(Token **token) {
    if ((*token)->type == TOK_IF) return parse_if(token);
    if ((*token)->type == TOK_CASE) return parse_case(token);
    
    if ((*token)->type == TOK_STR && 
        (*token)->next && (*token)->next->type == TOK_LPAREN &&
        (*token)->next->next && (*token)->next->next->type == TOK_RPAREN &&
        (*token)->next->next->next && (*token)->next->next->next->type == TOK_BLOCK) {
        char *name = strdup((*token)->val);
        char *body = strdup((*token)->next->next->next->val);
        for (int i = 0; i < 4; i++) consume(token);
        ASTNode *node = calloc(1, sizeof(*node));
        node->type = AST_FUNCDEF;
        node->name = name;
        node->cmd = body; 
        return node;
    }
    if ((*token)->type == TOK_STR) {
        Token *start = *token;
        Token *end = start;
        while (end && end->type == TOK_STR) {
            end = end->next;
        }
        char *cmd_str = concat_tokens(start, end);
        *token = end;
        ASTNode *node = calloc(1, sizeof(*node));
        node->type = AST_COMMAND;
        node->cmd = cmd_str;
        return node;
    }
    return NULL;
}

static ASTNode *parse_pipeline(Token **token) {
    bool negate = false;
    if ((*token)->type == TOK_BANG) {
        negate = true;
        consume(token);
    }
    ASTNode *left = parse_command(token);
    if (!left) return NULL;
    while ((*token)->type == TOK_PIPE) {
        consume(token);
        while ((*token)->type == TOK_SEMI) consume(token);
        ASTNode *right = parse_command(token);
        if (!right) break;
        ASTNode *node = calloc(1, sizeof(*node));
        node->type = AST_PIPELINE;
        node->left = left;
        node->right = right;
        left = node;
    }
    if (negate) {
        ASTNode *neg = calloc(1, sizeof(*neg));
        neg->type = AST_NEGATION;
        neg->left = left;
        left = neg;
    }
    return left;
}

static ASTNode *parse_and_or(Token **token) {
    ASTNode *left = parse_pipeline(token);
    if (!left) return NULL;
    while ((*token)->type == TOK_AND || (*token)->type == TOK_OR) {
        TokenType op = (*token)->type;
        consume(token);
        while ((*token)->type == TOK_SEMI) consume(token);
        ASTNode *right = parse_pipeline(token);
        if (!right) break;
        ASTNode *node = calloc(1, sizeof(*node));
        node->type = (op == TOK_AND) ? AST_AND : AST_OR;
        node->left = left;
        node->right = right;
        left = node; 
    }
    return left;
}

static ASTNode *parse_sequence(Token **token) {
    while ((*token)->type == TOK_SEMI) {
        consume(token);
    }
    
    ASTNode *left = parse_and_or(token);
    if (!left) {
        if ((*token)->type != TOK_EOF && (*token)->type != TOK_RPAREN &&
            (*token)->type != TOK_THEN && (*token)->type != TOK_ELIF &&
            (*token)->type != TOK_ELSE && (*token)->type != TOK_FI &&
            (*token)->type != TOK_ESAC && (*token)->type != TOK_DSEMI) {
             consume(token);
             return parse_sequence(token);
        }
        return NULL;
    }
    
    bool is_sequence = false;
    
    while ((*token)->type == TOK_AMP || (*token)->type == TOK_SEMI) {
        bool is_bg = ((*token)->type == TOK_AMP);
        consume(token);
        
        if (is_bg) {
            ASTNode *bg = calloc(1, sizeof(*bg));
            bg->type = AST_BACKGROUND;
            bg->left = left;
            left = bg;
        }
        
        is_sequence = true;
    }
    
    if (!is_sequence && (*token)->type != TOK_EOF && (*token)->type != TOK_PIPE && 
        (*token)->type != TOK_AND && (*token)->type != TOK_OR && (*token)->type != TOK_RPAREN) {
        is_sequence = true;
    }

    if (is_sequence) {
        if ((*token)->type != TOK_EOF) {
            ASTNode *right = parse_sequence(token);
            if (right) {
                ASTNode *seq = calloc(1, sizeof(*seq));
                seq->type = AST_SEQUENCE;
                seq->left = left;
                seq->right = right;
                left = seq;
            }
        }
    }
    
    return left;
}

ASTNode* parse_ast(const char *line) {
    Token *tokens = tokenize(line);
    if (!tokens) return NULL;
    
    Token *ptr = tokens;
    ASTNode *ast = parse_sequence(&ptr);
    
    if (ptr && ptr->type != TOK_EOF) {
        fprintf(stderr, "cvx_shell: syntax error near '%s'\n", ptr->val ? ptr->val : "EOF");
        free_ast(ast);
        free_tokens(tokens);
        return NULL;
    }
    
    free_tokens(tokens);
    return ast;
}

int process_command_line(char *line) {
    if (!line) return 0;
    ASTNode *ast = parse_ast(line);
    if (!ast) return 0;
    int status = execute_ast(ast, false);
    free_ast(ast);
    return status;
}
