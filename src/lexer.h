// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]

#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>

typedef enum {
    TOK_STR,
    TOK_AND,
    TOK_OR,
    TOK_PIPE,
    TOK_SEMI,
    TOK_AMP,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_BLOCK,
    TOK_IF,
    TOK_THEN,
    TOK_ELSE,
    TOK_ELIF,
    TOK_FI,
    TOK_CASE,
    TOK_IN,
    TOK_ESAC,
    TOK_DSEMI,
    TOK_BANG,
    TOK_FOR,
    TOK_WHILE,
    TOK_UNTIL,
    TOK_DO,
    TOK_DONE,
    TOK_EOF
} TokenType;

typedef struct Token {
    TokenType type;
    char *val;
    struct Token *next;
} Token;

Token *tokenize(const char *line);
void free_tokens(Token *head);
bool match(Token **token, TokenType type);
void consume(Token **token);
char *concat_tokens(Token *start, Token *end);
bool is_block_complete(const char *line);

#endif
