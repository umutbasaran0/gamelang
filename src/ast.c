#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ASTNode *ast_create_node(NodeType type, int line) {
    ASTNode *n = (ASTNode *)calloc(1, sizeof(ASTNode));
    if (!n) { fprintf(stderr, "Fatal: out of memory\n"); exit(1); }
    n->type = type; n->line = line;
    return n;
}

void ast_free(ASTNode *n) {
    if (!n) return;
    switch (n->type) {
        case NODE_PROGRAM:
            for (int i=0;i<n->data.program.count;i++) ast_free(n->data.program.decls[i]);
            free(n->data.program.decls); break;
        case NODE_RESOURCE: case NODE_ARENA: case NODE_TILE:
        case NODE_DECK: case NODE_CARD: case NODE_PLAYERS:
            free(n->data.resource.name);
            for (int i=0;i<n->data.resource.prop_count;i++) ast_free(n->data.resource.props[i]);
            free(n->data.resource.props); break;
        case NODE_PROP:
            free(n->data.prop.key);
            if (!n->data.prop.is_int && !n->data.prop.is_bool) free(n->data.prop.val_str);
            break;
        case NODE_TURN_STRUCT:
            for (int i=0;i<n->data.turn_struct.count;i++) ast_free(n->data.turn_struct.phases[i]);
            free(n->data.turn_struct.phases); break;
        case NODE_PHASE: free(n->data.phase.name); ast_free(n->data.phase.body); break;
        case NODE_EVENT:
            free(n->data.event.event_type);
            for (int i=0;i<n->data.event.param_count;i++) free(n->data.event.params[i]);
            free(n->data.event.params); ast_free(n->data.event.body); break;
        case NODE_BLOCK:
            for (int i=0;i<n->data.block.count;i++) ast_free(n->data.block.stmts[i]);
            free(n->data.block.stmts); break;
        case NODE_STMT_ACTION:
            free(n->data.action.name);
            for (int i=0;i<n->data.action.arg_count;i++) ast_free(n->data.action.args[i]);
            free(n->data.action.args); break;
        case NODE_STMT_ASSIGN: free(n->data.assignment.var_name); ast_free(n->data.assignment.expr); break;
        case NODE_STMT_IF:
            ast_free(n->data.if_stmt.cond);
            ast_free(n->data.if_stmt.then_block);
            ast_free(n->data.if_stmt.else_block); break;
        case NODE_STMT_WHILE: ast_free(n->data.while_stmt.cond); ast_free(n->data.while_stmt.body); break;
        case NODE_STMT_BREAK: break;
        case NODE_BINARY_OP:
            free(n->data.binary.op); ast_free(n->data.binary.left); ast_free(n->data.binary.right); break;
        case NODE_UNARY_OP: free(n->data.unary.op); ast_free(n->data.unary.operand); break;
        case NODE_STRING_LIT: free(n->data.string_val); break;
        case NODE_IDENT:      free(n->data.ident_val); break;
        case NODE_INT_LIT: case NODE_FLOAT_LIT: case NODE_BOOL_LIT: break;
        default: break;
    }
    free(n);
}

static void indent(int d) { for (int i=0;i<d;i++) printf("  "); }

void ast_dump(const ASTNode *n, int depth) {
    if (!n) return;
    switch (n->type) {
        case NODE_PROGRAM:   indent(depth); printf("PROGRAM (%d)\n",n->data.program.count);
            for(int i=0;i<n->data.program.count;i++) ast_dump(n->data.program.decls[i],depth+1); break;
        case NODE_RESOURCE:  indent(depth); printf("RESOURCE %s\n",n->data.resource.name);
            for(int i=0;i<n->data.resource.prop_count;i++) ast_dump(n->data.resource.props[i],depth+1); break;
        case NODE_ARENA:     indent(depth); printf("ARENA %s\n",n->data.resource.name);
            for(int i=0;i<n->data.resource.prop_count;i++) ast_dump(n->data.resource.props[i],depth+1); break;
        case NODE_TILE:      indent(depth); printf("TILE %s\n",n->data.resource.name);
            for(int i=0;i<n->data.resource.prop_count;i++) ast_dump(n->data.resource.props[i],depth+1); break;
        case NODE_DECK:      indent(depth); printf("DECK %s\n",n->data.resource.name);
            for(int i=0;i<n->data.resource.prop_count;i++) ast_dump(n->data.resource.props[i],depth+1); break;
        case NODE_CARD:      indent(depth); printf("CARD %s\n",n->data.resource.name);
            for(int i=0;i<n->data.resource.prop_count;i++) ast_dump(n->data.resource.props[i],depth+1); break;
        case NODE_PLAYERS:   indent(depth); printf("PLAYERS\n");
            for(int i=0;i<n->data.resource.prop_count;i++) ast_dump(n->data.resource.props[i],depth+1); break;
        case NODE_PROP:
            indent(depth);
            if(n->data.prop.is_int)       printf("PROP %s : %d\n",n->data.prop.key,n->data.prop.val_int);
            else if(n->data.prop.is_bool) printf("PROP %s : %s\n",n->data.prop.key,n->data.prop.bool_val?"true":"false");
            else                          printf("PROP %s : %s\n",n->data.prop.key,n->data.prop.val_str);
            break;
        case NODE_TURN_STRUCT: indent(depth); printf("TURN_STRUCTURE (%d phases)\n",n->data.turn_struct.count);
            for(int i=0;i<n->data.turn_struct.count;i++) ast_dump(n->data.turn_struct.phases[i],depth+1); break;
        case NODE_PHASE:     indent(depth); printf("PHASE %s\n",n->data.phase.name);
            ast_dump(n->data.phase.body,depth+1); break;
        case NODE_EVENT:     indent(depth); printf("EVENT on %s\n",n->data.event.event_type);
            ast_dump(n->data.event.body,depth+1); break;
        case NODE_BLOCK:     indent(depth); printf("BLOCK (%d)\n",n->data.block.count);
            for(int i=0;i<n->data.block.count;i++) ast_dump(n->data.block.stmts[i],depth+1); break;
        case NODE_STMT_ACTION: indent(depth); printf("ACTION %s (%d args)\n",n->data.action.name,n->data.action.arg_count);
            for(int i=0;i<n->data.action.arg_count;i++) ast_dump(n->data.action.args[i],depth+1); break;
        case NODE_STMT_ASSIGN: indent(depth); printf("ASSIGN %s\n",n->data.assignment.var_name);
            ast_dump(n->data.assignment.expr,depth+1); break;
        case NODE_STMT_IF:
            indent(depth); printf("IF\n");
            indent(depth+1); printf("COND\n"); ast_dump(n->data.if_stmt.cond,depth+2);
            indent(depth+1); printf("THEN\n"); ast_dump(n->data.if_stmt.then_block,depth+2);
            if(n->data.if_stmt.else_block) {
                /* else if prints as ELSE → IF (chain) */
                indent(depth+1); printf("ELSE\n"); ast_dump(n->data.if_stmt.else_block,depth+2);
            }
            break;
        case NODE_STMT_WHILE:
            indent(depth); printf("WHILE\n");
            indent(depth+1); printf("COND\n"); ast_dump(n->data.while_stmt.cond,depth+2);
            ast_dump(n->data.while_stmt.body,depth+1); break;
        case NODE_STMT_BREAK:  indent(depth); printf("BREAK\n"); break;
        case NODE_BINARY_OP:   indent(depth); printf("OP(%s)\n",n->data.binary.op);
            ast_dump(n->data.binary.left,depth+1); ast_dump(n->data.binary.right,depth+1); break;
        case NODE_UNARY_OP:    indent(depth); printf("UNARY(%s)\n",n->data.unary.op);
            ast_dump(n->data.unary.operand,depth+1); break;
        case NODE_INT_LIT:     indent(depth); printf("INT(%d)\n",n->data.int_val); break;
        case NODE_FLOAT_LIT:   indent(depth); printf("FLOAT(%f)\n",n->data.float_val); break;
        case NODE_STRING_LIT:  indent(depth); printf("STRING(%s)\n",n->data.string_val); break;
        case NODE_BOOL_LIT:    indent(depth); printf("BOOL(%s)\n",n->data.bool_val?"true":"false"); break;
        case NODE_IDENT:       indent(depth); printf("ID(%s)\n",n->data.ident_val); break;
        default:               indent(depth); printf("UNKNOWN(%d)\n",(int)n->type); break;
    }
}
