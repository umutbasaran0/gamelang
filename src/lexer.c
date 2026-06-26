#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *KEYWORDS[] = {
    "resource", "arena", "turn_structure", "phase", "on", "land",
    "pass", "deplete", "phase_enter", "roll", "advance", "heal",
    "damage", "pay", "transfer", "eliminate", "end_phase", "if", "else",
    "tile", "players", "deck", "card", "draw", "while",
    "each_player", "win_if", "lose_if", "print", "random", "move_to",
    "and", "or", "not", "true", "false",
    "buy", "sell", "give",
    "break"          /* NEW */
};
static const int NUM_KEYWORDS = (int)(sizeof(KEYWORDS) / sizeof(KEYWORDS[0]));

static const char *ACTION_KEYWORDS[] = {
    "heal", "damage", "pay", "transfer", "eliminate",
    "end_phase", "roll", "advance", "deplete", "land", "pass",
    "draw", "print", "random", "move_to", "buy", "sell", "give"
};
static const int NUM_ACTION_KEYWORDS =
    (int)(sizeof(ACTION_KEYWORDS) / sizeof(ACTION_KEYWORDS[0]));

bool is_keyword(const char *str) {
    for (int i = 0; i < NUM_KEYWORDS; i++)
        if (strcmp(str, KEYWORDS[i]) == 0) return true;
    return false;
}

bool is_action_keyword(const char *str) {
    for (int i = 0; i < NUM_ACTION_KEYWORDS; i++)
        if (strcmp(str, ACTION_KEYWORDS[i]) == 0) return true;
    return false;
}

static void tl_init(TokenList *list) {
    list->capacity = 64;
    list->count    = 0;
    list->tokens   = (Token *)malloc(list->capacity * sizeof(Token));
    if (!list->tokens) { fprintf(stderr, "Fatal: out of memory\n"); exit(1); }
}

static void tl_push(TokenList *list, TokenType type, const char *value, int line) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->tokens = (Token *)realloc(list->tokens, list->capacity * sizeof(Token));
        if (!list->tokens) { fprintf(stderr, "Fatal: out of memory\n"); exit(1); }
    }
    list->tokens[list->count].type        = type;
    list->tokens[list->count].value       = strdup(value);
    list->tokens[list->count].line_number = line;
    list->count++;
}

TokenList tokenize(const char *source) {
    TokenList list;
    tl_init(&list);
    int         line = 1;
    const char *cur  = source;

    while (*cur != '\0') {
        if (isspace((unsigned char)*cur)) {
            if (*cur == '\n') line++;
            cur++; continue;
        }
        if (cur[0] == '/' && cur[1] == '/') {
            const char *start = cur; cur += 2;
            while (*cur && *cur != '\n') cur++;
            int len = (int)(cur - start);
            char *buf = (char *)malloc(len + 1);
            strncpy(buf, start, len); buf[len] = '\0';
            tl_push(&list, TOKEN_COMMENT, buf, line);
            free(buf); continue;
        }
        if (isalpha((unsigned char)*cur) || *cur == '_') {
            const char *start = cur;
            while (isalnum((unsigned char)*cur) || *cur == '_') cur++;
            int len = (int)(cur - start);
            char *val = (char *)malloc(len + 1);
            strncpy(val, start, len); val[len] = '\0';
            tl_push(&list, is_keyword(val) ? TOKEN_KEYWORD : TOKEN_IDENT, val, line);
            free(val); continue;
        }
        if (*cur == '"') {
            cur++;
            const char *start = cur;
            while (*cur != '\0' && *cur != '"') cur++;
            int len = (int)(cur - start);
            char *val = (char *)malloc(len + 1);
            strncpy(val, start, len); val[len] = '\0';
            tl_push(&list, TOKEN_STRING_LIT, val, line);
            if (*cur == '"') cur++;
            free(val); continue;
        }
        if (isdigit((unsigned char)*cur)) {
            const char *start = cur; bool is_float = false;
            while (isdigit((unsigned char)*cur) || *cur == '.') {
                if (*cur == '.') is_float = true; cur++;
            }
            int len = (int)(cur - start);
            char *val = (char *)malloc(len + 1);
            strncpy(val, start, len); val[len] = '\0';
            tl_push(&list, is_float ? TOKEN_FLOAT_LIT : TOKEN_INT_LIT, val, line);
            free(val); continue;
        }
        if (cur[0] == '!' && cur[1] == '=') {
            tl_push(&list, TOKEN_OPERATOR, "!=", line); cur += 2; continue;
        }
        if (strchr("=<>-+*/!", *cur)) {
            char op[3] = {0}; op[0] = *cur;
            if ((cur[0]=='='&&cur[1]=='=')||(cur[0]=='<'&&cur[1]=='=')||
                (cur[0]=='>'&&cur[1]=='=')||(cur[0]=='-'&&cur[1]=='>')) {
                op[1] = cur[1]; cur += 2;
            } else { cur++; }
            tl_push(&list, TOKEN_OPERATOR, op, line); continue;
        }
        if (strchr("{}():,", *cur)) {
            char sep[2] = {*cur, '\0'};
            tl_push(&list, TOKEN_SEPARATOR, sep, line); cur++; continue;
        }
        char unk[2] = {*cur, '\0'};
        tl_push(&list, TOKEN_UNKNOWN, unk, line); cur++;
    }
    tl_push(&list, TOKEN_EOF, "EOF", line);
    return list;
}

void free_token_list(TokenList *list) {
    for (size_t i = 0; i < list->count; i++) free(list->tokens[i].value);
    free(list->tokens);
    list->tokens = NULL; list->count = 0; list->capacity = 0;
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_IDENT:      return "IDENT";
        case TOKEN_INT_LIT:    return "INT_LIT";
        case TOKEN_FLOAT_LIT:  return "FLOAT_LIT";
        case TOKEN_STRING_LIT: return "STRING_LIT";
        case TOKEN_KEYWORD:    return "KEYWORD";
        case TOKEN_OPERATOR:   return "OPERATOR";
        case TOKEN_SEPARATOR:  return "SEPARATOR";
        case TOKEN_COMMENT:    return "COMMENT";
        case TOKEN_EOF:        return "EOF";
        default:               return "UNKNOWN";
    }
}