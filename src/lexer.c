// Copyright (c) 2025-2026 JHXStudioriginal
// This file is part of the GNU General Public License v3
// All original author information and file headers must be preserved.
// For full license text, see: [https://github.com/JHXStudioriginal/CVX-Shell/blob/main/LICENSE]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "lexer.h"

typedef struct {
    Token *head;
    Token *tail;
} LexerCtx;

static void add_tok(LexerCtx *ctx, TokenType t, const char *val, int len) {
    Token *tok = calloc(1, sizeof(Token));
    if (!tok) return; // Zawsze warto sprawdzić przy calloc
    tok->type = t;
    if (val) tok->val = strndup(val, len);
    
    if (!ctx->head) {
        ctx->head = ctx->tail = tok;
    } else {
        ctx->tail->next = tok;
        ctx->tail = tok;
    }
}

Token *tokenize(const char *line) {
    LexerCtx ctx = {NULL, NULL};
    const char *p = line;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        if (strncmp(p, "&&", 2) == 0) { add_tok(&ctx, TOK_AND, NULL, 0); p += 2; continue; }
        if (strncmp(p, "||", 2) == 0) { add_tok(&ctx, TOK_OR, NULL, 0); p += 2; continue; }
        if (strncmp(p, ";;", 2) == 0) { add_tok(&ctx, TOK_DSEMI, NULL, 0); p += 2; continue; }
        if (*p == '|') { add_tok(&ctx, TOK_PIPE, NULL, 0); p++; continue; }
        if (*p == '&') { add_tok(&ctx, TOK_AMP, NULL, 0); p++; continue; }
        if (*p == '(') { add_tok(&ctx, TOK_LPAREN, NULL, 0); p++; continue; }
        if (*p == ')') { add_tok(&ctx, TOK_RPAREN, NULL, 0); p++; continue; }
        if (*p == '!') { add_tok(&ctx, TOK_BANG, "!", 1); p++; continue; }

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
                add_tok(&ctx, TOK_BLOCK, start, p - start);
            }
            if (*p == '}') p++;
            continue;
        }

        if (*p == '\n' || *p == ';') {
            if (*p == '\n') {
                if (!ctx.tail || (ctx.tail->type != TOK_NEWLINE && ctx.tail->type != TOK_SEMI)) {
                    add_tok(&ctx, TOK_NEWLINE, NULL, 0);
                }
                p++;
            } else {
                if (ctx.tail && ctx.tail->type == TOK_NEWLINE) {
                    ctx.tail->type = TOK_SEMI;
                } else if (!ctx.tail || ctx.tail->type != TOK_SEMI) {
                    add_tok(&ctx, TOK_SEMI, NULL, 0);
                }
                p++;
            }
            continue;
        }

        const char *start = p;
        bool in_quotes = false;
        char quote_char = 0;
        int p_depth = 0;

        while (*p) {
            if (!in_quotes) {
                if (*p == '"' || *p == '\'') {
                    in_quotes = true;
                    quote_char = *p;
                } else if (*p == '$' && p[1] == '(') {
                    p_depth++;
                    p++;
                } else if (*p == '(' && p_depth > 0) {
                    p_depth++;
                } else if (*p == ')' && p_depth > 0) {
                    p_depth--;
                } else if (p_depth == 0) {
                    if (*p == ' ' || *p == '\t' || *p == '\n' ||
                        *p == ';' || *p == '|' || *p == '&' ||
                        *p == '(' || *p == ')' || *p == '{' || *p == '}') break;
                }
            } else {
                if (quote_char == '"' && *p == '$' && p[1] == '(') {
                    p_depth++;
                    p++;
                } else if (quote_char == '"' && *p == '(' && p_depth > 0) {
                    p_depth++;
                } else if (quote_char == '"' && *p == ')' && p_depth > 0) {
                    p_depth--;
                } else if (*p == quote_char) {
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
            else if (strcmp(s, "for") == 0) t = TOK_FOR;
            else if (strcmp(s, "while") == 0) t = TOK_WHILE;
            else if (strcmp(s, "until") == 0) t = TOK_UNTIL;
            else if (strcmp(s, "do") == 0) t = TOK_DO;
            else if (strcmp(s, "done") == 0) t = TOK_DONE;
            add_tok(&ctx, t, s, p - start);
            free(s);
        } else {
            p++;
        }
    }
    add_tok(&ctx, TOK_EOF, NULL, 0);
    return ctx.head;
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
            else if (t->type == TOK_NEWLINE) strcat(res, "\n");
            else if (t->type == TOK_AMP) strcat(res, "&");
            else if (t->type == TOK_DSEMI) strcat(res, ";;");
        }
        if (t->next != end) strcat(res, " ");
    }
    return res;
}

bool is_block_complete(const char *line) {
    if (!line) return true;

    int brace_depth = 0;
    int paren_depth = 0;
    bool in_sq = false;
    bool in_dq = false;
    bool in_bt = false;
    const char *p = line;

    while (*p) {
        if (!in_sq && *p == '\\') {
            p++;
            if (!*p) return false;
            p++;
            continue;
        }

        if (in_sq) {
            if (*p == '\'') in_sq = false;
        } else if (in_dq) {
            if (*p == '"') in_dq = false;
            else if (*p == '`') in_bt = !in_bt;
        } else if (in_bt) {
            if (*p == '`') in_bt = false;
        } else {
            if (*p == '\'') in_sq = true;
            else if (*p == '"') in_dq = true;
            else if (*p == '`') in_bt = true;
            else if (*p == '{') brace_depth++;
            else if (*p == '}') { if (brace_depth > 0) brace_depth--; }
            else if (*p == '(') paren_depth++;
            else if (*p == ')') { if (paren_depth > 0) paren_depth--; }
        }
        p++;
    }

    if (in_sq || in_dq || in_bt || brace_depth > 0 || paren_depth > 0) return false;

    const char *end = line + strlen(line) - 1;
    while (end >= line && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    if (end >= line) {
        if (*end == '|' || *end == '&' || *end == '\\') return false;
    }

    Token *tokens = tokenize(line);
    if (!tokens) return true;

    int if_depth = 0, case_depth = 0, loop_depth = 0;
    TokenType last_type = TOK_EOF;

    for (Token *t = tokens; t && t->type != TOK_EOF; t = t->next) {
        if (t->type == TOK_IF) if_depth++;
        else if (t->type == TOK_FI) { if (if_depth > 0) if_depth--; }
        else if (t->type == TOK_CASE) case_depth++;
        else if (t->type == TOK_ESAC) { if (case_depth > 0) case_depth--; }
        else if (t->type == TOK_FOR || t->type == TOK_WHILE || t->type == TOK_UNTIL) loop_depth++;
        else if (t->type == TOK_DONE) { if (loop_depth > 0) loop_depth--; }
        if (t->type != TOK_NEWLINE) last_type = t->type;
    }
    free_tokens(tokens);

    if (if_depth != 0 || case_depth != 0 || loop_depth != 0) return false;

    if (last_type == TOK_THEN || last_type == TOK_DO || last_type == TOK_ELSE ||
        last_type == TOK_ELIF || last_type == TOK_IN || last_type == TOK_AND ||
        last_type == TOK_OR || last_type == TOK_PIPE) {
        return false;
    }

    return true;
}
