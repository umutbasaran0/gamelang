#include "typecheck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ValueType check_expr_type(ASTNode *expr) {
    if (!expr) return VAL_ERROR;
    switch (expr->type) {
        case NODE_INT_LIT:    return VAL_INT;
        case NODE_FLOAT_LIT:  return VAL_FLOAT;
        case NODE_STRING_LIT: return VAL_STRING;
        case NODE_BOOL_LIT:   return VAL_BOOL;

        case NODE_IDENT: {
            Value *v = get_var(expr->data.ident_val);
            if (!v) {
                /* Warn but don't abort — variable may be assigned later */
                return VAL_INT; /* assume int for unknown vars */
            }
            return v->type;
        }

        case NODE_UNARY_OP:
            check_expr_type(expr->data.unary.operand);
            return VAL_BOOL;

        case NODE_BINARY_OP: {
            if (strcmp(expr->data.binary.op,"and")==0||strcmp(expr->data.binary.op,"or")==0) {
                check_expr_type(expr->data.binary.left);
                check_expr_type(expr->data.binary.right);
                return VAL_BOOL;
            }
            ValueType lt = check_expr_type(expr->data.binary.left);
            ValueType rt = check_expr_type(expr->data.binary.right);
            if (lt==VAL_ERROR||rt==VAL_ERROR) return VAL_ERROR;

            /* String + String = String; == and != also ok */
            if (lt==VAL_STRING||rt==VAL_STRING) {
                if (lt==VAL_STRING&&rt==VAL_STRING) {
                    if (strcmp(expr->data.binary.op,"+")==0) return VAL_STRING;
                    if (strcmp(expr->data.binary.op,"==")==0||strcmp(expr->data.binary.op,"!=")==0)
                        return VAL_BOOL;
                }
                fprintf(stderr,"TypeError [Line %d]: String mixed with non-string in '%s'\n",
                        expr->line,expr->data.binary.op);
                exit(EXIT_FAILURE);
            }
            /* Relational → BOOL */
            if (strcmp(expr->data.binary.op,"==")==0||strcmp(expr->data.binary.op,"!=")==0||
                strcmp(expr->data.binary.op,"<")==0 ||strcmp(expr->data.binary.op,">")==0 ||
                strcmp(expr->data.binary.op,"<=")==0||strcmp(expr->data.binary.op,">=")==0)
                return VAL_BOOL;
            if (lt==VAL_FLOAT||rt==VAL_FLOAT) return VAL_FLOAT;
            return VAL_INT;
        }
        default: return VAL_ERROR;
    }
}

static void typecheck_stmt(ASTNode *stmt) {
    if (!stmt) return;
    switch (stmt->type) {
        case NODE_STMT_ASSIGN: {
            ValueType et = check_expr_type(stmt->data.assignment.expr);
            Value *v = get_var(stmt->data.assignment.var_name);
            if (!v) {
                Value dummy; dummy.type=et;
                if (et==VAL_STRING) dummy.data.s_val="";
                else dummy.data.i_val=0;
                declare_var(stmt->data.assignment.var_name, et, dummy);
            }
            break;
        }
        case NODE_STMT_ACTION:
            for (int i=0;i<stmt->data.action.arg_count;i++)
                check_expr_type(stmt->data.action.args[i]);
            break;
        case NODE_STMT_IF:
            check_expr_type(stmt->data.if_stmt.cond);
            typecheck_stmt(stmt->data.if_stmt.then_block);
            if (stmt->data.if_stmt.else_block) typecheck_stmt(stmt->data.if_stmt.else_block);
            break;
        case NODE_STMT_WHILE:
            check_expr_type(stmt->data.while_stmt.cond);
            typecheck_stmt(stmt->data.while_stmt.body);
            break;
        case NODE_BLOCK:
            for (int i=0;i<stmt->data.block.count;i++) typecheck_stmt(stmt->data.block.stmts[i]);
            break;
        case NODE_EVENT:
            push_scope();
            for (int p=0;p<stmt->data.event.param_count;p++) {
                Value dummy; dummy.type=VAL_INT; dummy.data.i_val=0;
                declare_var(stmt->data.event.params[p],VAL_INT,dummy);
            }
            typecheck_stmt(stmt->data.event.body);
            pop_scope();
            break;
        default: break;
    }
}

void typecheck_program(ASTNode *root) {
    if (!root||root->type!=NODE_PROGRAM) return;
    push_scope();
    for (int i=0;i<root->data.program.count;i++) {
        ASTNode *d=root->data.program.decls[i];
        if (d->type==NODE_EVENT) typecheck_stmt(d);
    }
    pop_scope();
}