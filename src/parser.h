#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

// Public API
ASTNode *parse_program(TokenList *tokens);

#endif 