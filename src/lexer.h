#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TOKEN_IDENT,
    TOKEN_INT_LIT,
    TOKEN_FLOAT_LIT,
    TOKEN_STRING_LIT,
    TOKEN_KEYWORD,
    TOKEN_OPERATOR,
    TOKEN_SEPARATOR,
    TOKEN_COMMENT,
    TOKEN_UNKNOWN,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char     *value;
    int       line_number;
} Token;

typedef struct {
    Token  *tokens;
    size_t  count;
    size_t  capacity;
} TokenList;

TokenList   tokenize(const char *source);
void        free_token_list(TokenList *list);
const char *token_type_name(TokenType type);
bool        is_keyword(const char *str);
bool        is_action_keyword(const char *str);

#endif
