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
            if (!ctx.tail || ctx.tail->type != TOK_SEMI) {
                add_tok(&ctx, TOK_SEMI, NULL, 0);
            }
            while (*p == '\n' || *p == ';' || *p == ' ' || *p == '\t') p++;
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
    bool in_sq = false;
    bool in_dq = false;
    const char *p = line;
    const char *last_op_pos = NULL;

    while (*p) {
        if (!in_sq && *p == '\\') {
            p++;
            if (*p) p++;
            last_op_pos = NULL;
            continue;
        }

        if (in_sq) {
            if (*p == '\'') in_sq = false;
        } else if (in_dq) {
            if (*p == '"') in_dq = false;
        } else {
            if (*p == '\'') { in_sq = true; last_op_pos = NULL; }
            else if (*p == '"') { in_dq = true; last_op_pos = NULL; }
            else if (*p == '{') brace_depth++;
            else if (*p == '}') brace_depth--;
            else if (strncmp(p, "<<", 2) == 0 && *(p+2) != '<' && *(p+2) != '>') {
                const char *q = p + 2;
                while (*q == ' ' || *q == '\t') q++;
                if (*q == '-') { q++; while (*q == ' ' || *q == '\t') q++; }
                bool qdel = (*q == '\'' || *q == '"');
                char qch = qdel ? *q++ : 0;
                char delim[64] = {0};
                int di = 0;
                while (*q && di < 63) {
                    if (qdel && *q == qch) { q++; break; }
                    if (!qdel && (*q == ' ' || *q == '\t' || *q == '\n' ||
                        *q == ';' || *q == '|' || *q == '&' || *q == ')')) break;
                    delim[di++] = *q++;
                }
                p = q;
                while (*p && *p != '\n') p++;
                if (*p == '\n') {
                    p++;
                    while (*p) {
                        const char *sol = p;
                        while (*p && *p != '\n') p++;
                        size_t ll = (size_t)(p - sol);
                        if (ll == strlen(delim) && strncmp(sol, delim, ll) == 0) {
                            if (*p == '\n') p++;
                            break;
                        }
                        if (*p == '\n') p++;
                    }
                }
                last_op_pos = NULL;
                continue;
            } else if (strncmp(p, "&&", 2) == 0 || strncmp(p, "||", 2) == 0) {
                last_op_pos = p;
                p++;
            } else if (*p == '|' || *p == '&' || *p == '(') {
                last_op_pos = p;
            } else if (*p == ';') {
                last_op_pos = NULL;
            } else if (!isspace((unsigned char)*p)) {
                last_op_pos = NULL;
            }
        }
        if (in_sq || in_dq) last_op_pos = NULL;
        if (*p) p++;
    }

    if (in_sq || in_dq || brace_depth > 0 || last_op_pos != NULL) return false;

    Token *tokens = tokenize(line);
    if (!tokens) return true;

    int if_depth = 0, case_depth = 0, loop_depth = 0;
    for (Token *t = tokens; t && t->type != TOK_EOF; t = t->next) {
        if (t->type == TOK_IF) if_depth++;
        else if (t->type == TOK_FI) if_depth--;
        else if (t->type == TOK_CASE) case_depth++;
        else if (t->type == TOK_ESAC) case_depth--;
        else if (t->type == TOK_FOR || t->type == TOK_WHILE || t->type == TOK_UNTIL) loop_depth++;
        else if (t->type == TOK_DONE) loop_depth--;
    }
    free_tokens(tokens);

    if (if_depth != 0 || case_depth != 0 || loop_depth != 0) return false;
    return true;
}
