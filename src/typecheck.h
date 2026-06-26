#ifndef TYPECHECK_H
#define TYPECHECK_H

#include "ast.h"
#include "environment.h"

ValueType check_expr_type(ASTNode *expr);
void typecheck_program(ASTNode *root);

#endif