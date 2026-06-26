#ifndef AST_H
#define AST_H

#include <stdbool.h>

typedef enum {
    NODE_PROGRAM,
    NODE_RESOURCE,
    NODE_ARENA,
    NODE_TILE,
    NODE_DECK,
    NODE_CARD,
    NODE_PLAYERS,
    NODE_TURN_STRUCT,
    NODE_PHASE,
    NODE_EVENT,
    NODE_STMT_ACTION,
    NODE_STMT_ASSIGN,
    NODE_STMT_IF,       /* if / else if / else chain */
    NODE_STMT_WHILE,
    NODE_STMT_BREAK,    /* NEW: break */
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STRING_LIT,
    NODE_BOOL_LIT,
    NODE_IDENT,
    NODE_BLOCK,
    NODE_PROP
} NodeType;

typedef struct ASTNode ASTNode;

struct ASTNode {
    NodeType type;
    int      line;

    union {
        struct { ASTNode **decls; int count; int capacity; } program;
        struct { char *name; ASTNode **props; int prop_count; int prop_capacity; } resource;
        struct { ASTNode **phases; int count; int capacity; } turn_struct;
        struct { char *name; ASTNode *body; } phase;
        struct {
            char    *event_type;
            char   **params;
            int      param_count;
            int      param_capacity;
            ASTNode *body;
        } event;
        struct { char *name; ASTNode **args; int arg_count; int arg_capacity; } action;
        struct { char *var_name; ASTNode *expr; } assignment;

        /* NODE_STMT_IF: supports else-if chains via else_block being another NODE_STMT_IF */
        struct { ASTNode *cond; ASTNode *then_block; ASTNode *else_block; } if_stmt;

        struct { ASTNode *cond; ASTNode *body; } while_stmt;
        struct { char *op; ASTNode *left; ASTNode *right; } binary;
        struct { char *op; ASTNode *operand; } unary;
        struct { ASTNode **stmts; int count; int capacity; } block;
        struct {
            char *key;
            char *val_str;
            int   val_int;
            bool  is_int;
            bool  is_bool;
            bool  bool_val;
        } prop;
        int   int_val;
        float float_val;
        char *string_val;
        char *ident_val;
        bool  bool_val;
    } data;
};

ASTNode *ast_create_node(NodeType type, int line);
void     ast_free(ASTNode *node);
void     ast_dump(const ASTNode *node, int depth);

#endif