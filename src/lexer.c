// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the Elasna Open Source License v3.
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "lexer.h"

Token *tokenize(const char *line) {
    Token *head = NULL, *tail = NULL;
    void add_tok(TokenType t, const char *val, int len) {
        Token *tok = calloc(1, sizeof(Token));
        tok->type = t;
        if (val) tok->val = strndup(val, len);
        if (!head) head = tail = tok;
        else { tail->next = tok; tail = tok; }
    }

    const char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        if (strncmp(p, "&&", 2) == 0) { add_tok(TOK_AND, NULL, 0); p += 2; continue; }
        if (strncmp(p, "||", 2) == 0) { add_tok(TOK_OR, NULL, 0); p += 2; continue; }
        if (strncmp(p, ";;", 2) == 0) { add_tok(TOK_DSEMI, NULL, 0); p += 2; continue; }
        if (*p == '|') { add_tok(TOK_PIPE, NULL, 0); p++; continue; }
        if (*p == '&') { add_tok(TOK_AMP, NULL, 0); p++; continue; }
        if (*p == '(') { add_tok(TOK_LPAREN, NULL, 0); p++; continue; }
        if (*p == ')') { add_tok(TOK_RPAREN, NULL, 0); p++; continue; }
        if (*p == '!') { add_tok(TOK_BANG, NULL, 0); p++; continue; }

        if (*p == '{') {
            const char *start = p + 1;
            int depth = 1;
            bool b_in_quotes = false;
            char b_quote_char = 0;
            p++;
            while (*p) {
                if (!b_in_quotes) {
                    if (*p == '"' || *p == '\'') {
                        b_in_quotes = true;
                        b_quote_char = *p;
                    } else if (*p == '#') {
                        while (*p && *p != '\n') p++;
                        if (!*p) break;
                    } else if (*p == '{') {
                        depth++;
                    } else if (*p == '}') {
                        depth--;
                        if (depth == 0) break;
                    }
                } else {
                    if (*p == b_quote_char && (p == line || *(p-1) != '\\')) {
                        b_in_quotes = false;
                    }
                }
                p++;
            }
            if (p > start) {
                add_tok(TOK_BLOCK, start, p - start);
            }
            if (*p == '}') p++;
            continue;
        }

        if (*p == '\n' || *p == ';') {
            if (!tail || tail->type != TOK_SEMI) {
                add_tok(TOK_SEMI, NULL, 0);
            }
            while (*p == '\n' || *p == ';' || *p == ' ' || *p == '\t') p++;
            continue;
        }

        const char *start = p;
        bool in_quotes = false;
        char quote_char = 0;

        while (*p) {
            if (!in_quotes) {
                if (*p == '"' || *p == '\'') {
                    in_quotes = true;
                    quote_char = *p;
                } else if (*p == ' ' || *p == '\t' || *p == '\n' ||
                           *p == ';' || *p == '|' ||
                           *p == '{' || *p == '}' || *p == '(' || *p == ')' ||
                           *p == '#') {
                    break;
                } else if (*p == '&') {
                    if (p > start && (*(p-1) == '>' || *(p-1) == '<')) {
                    } else if (p[1] == '&') {
                        break;
                    } else {
                        break;
                    }
                }
            } else {
                if (*p == quote_char) {
                    bool escaped = false;
                    const char *rev = p - 1;
                    while (rev >= start && *rev == '\\') {
                        escaped = !escaped;
                        rev--;
                    }
                    if (!escaped) in_quotes = false;
                }
            }
            p++;
        }

        if (p > start) {
            char *s = strndup(start, p - start);
            TokenType t = TOK_STR;
            if (strcmp(s, "if") == 0) t = TOK_IF;
            else if (strcmp(s, "then") == 0) t = TOK_THEN;
            else if (strcmp(s, "else") == 0) t = TOK_ELSE;
            else if (strcmp(s, "elif") == 0) t = TOK_ELIF;
            else if (strcmp(s, "fi") == 0) t = TOK_FI;
            else if (strcmp(s, "case") == 0) t = TOK_CASE;
            else if (strcmp(s, "in") == 0) t = TOK_IN;
            else if (strcmp(s, "esac") == 0) t = TOK_ESAC;
            add_tok(t, s, p - start);
            free(s);
        } else {
            p++;
        }
    }
    add_tok(TOK_EOF, NULL, 0);

    return head;
}

void free_tokens(Token *head) {
    while(head) {
        Token *t = head;
        head = head->next;
        free(t->val);
        free(t);
    }
}

bool match(Token **token, TokenType type) {
    if ((*token)->type == type) {
        *token = (*token)->next;
        return true;
    }
    return false;
}

void consume(Token **token) {
    if ((*token)->type != TOK_EOF) *token = (*token)->next;
}

char *concat_tokens(Token *start, Token *end) {
    int len = 0;
    for (Token *t = start; t != end; t = t->next) {
        if (t->val) len += strlen(t->val) + 1;
        else len += 4;
    }
    if (len == 0) return strdup("");
    char *res = malloc(len + 1);
    res[0] = '\0';
    for (Token *t = start; t != end; t = t->next) {
        if (t->val) {
            strcat(res, t->val);
        } else {
            if (t->type == TOK_AND) strcat(res, "&&");
            else if (t->type == TOK_OR) strcat(res, "||");
            else if (t->type == TOK_PIPE) strcat(res, "|");
            else if (t->type == TOK_SEMI) strcat(res, ";");
            else if (t->type == TOK_AMP) strcat(res, "&");
            else if (t->type == TOK_DSEMI) strcat(res, ";;");
        }
        if (t->next != end) strcat(res, " ");
    }
    return res;
}

bool is_block_complete(const char *line) {
    if (!line) return true;
    int depth = 0;
    bool in_quotes = false;
    char quote_char = 0;
    const char *p = line;
    const char *last_op_pos = NULL;

    while (*p) {
        if (!in_quotes) {
            if (*p == '"' || *p == '\'') {
                in_quotes = true;
                quote_char = *p;
                last_op_pos = NULL;
            } else if (*p == '#') {
                while (*p && *p != '\n') p++;
                if (!*p) break;
                continue;
            } else if (*p == '{') {
                depth++;
                last_op_pos = NULL;
            } else if (*p == '}') {
                depth--;
                last_op_pos = NULL;
            } else if (strncmp(p, "&&", 2) == 0) {
                last_op_pos = p;
                p++;
            } else if (strncmp(p, "||", 2) == 0) {
                last_op_pos = p;
                p++;
            } else if (*p == '|') {
                last_op_pos = p;
            } else if (!isspace(*p)) {
                last_op_pos = NULL;
            }
        } else {
            if (*p == quote_char) {
                bool escaped = false;
                const char *rev = p - 1;
                while (rev >= line && *rev == '\\') {
                    escaped = !escaped;
                    rev--;
                }
                if (!escaped) in_quotes = false;
            }
        }
        p++;
    }

    if (in_quotes || depth > 0) return false;

    if (last_op_pos != NULL) {
        return false;
    }

    return true;
}
