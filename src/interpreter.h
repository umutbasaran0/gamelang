#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ast.h"
#include "environment.h"

/* Turn-level scope management — called from main.c */
void interpreter_begin_turn(void);   /* push a persistent turn scope */
void interpreter_end_turn(void);     /* pop the turn scope */

Value eval_expr(ASTNode *expr);
void  exec_stmt(ASTNode *stmt);

#endif

/* Seed built-in player variables into the current turn scope */
void interpreter_seed_player(int id, int hp, int money, int pos);
void exec_phase_body(ASTNode *block);