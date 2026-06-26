#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TokenList *g_tl  = NULL;
static int        g_pos = 0;

static Token *peek(void)  { return &g_tl->tokens[g_pos]; }
static Token *peek2(void) {
    size_t next = (size_t)(g_pos + 1);
    if (next < g_tl->count) return &g_tl->tokens[next];
    return &g_tl->tokens[g_tl->count - 1];
}
static Token *advance(void) { return &g_tl->tokens[g_pos++]; }

static void error_exit(const char *expected) {
    Token *t = peek();
    fprintf(stderr,"SyntaxError on line %d: unexpected token '%s' (expected %s)\n",
            t->line_number, t->value, expected);
    exit(1);
}
static void expect_sep(const char *sep) {
    if (strcmp(peek()->value, sep) != 0) error_exit(sep);
    advance();
}
static ASTNode *new_node(NodeType type) { return ast_create_node(type, peek()->line_number); }
static ASTNode **dyn_push(ASTNode **arr, int *cnt, int *cap, ASTNode *item) {
    if (*cnt >= *cap) {
        *cap *= 2;
        arr = (ASTNode **)realloc(arr, (size_t)(*cap) * sizeof(ASTNode *));
        if (!arr) { fprintf(stderr,"Fatal: out of memory\n"); exit(1); }
    }
    arr[(*cnt)++] = item;
    return arr;
}

static ASTNode *parse_expr(void);

/* ── Expression parsing ─────────────────────────────────── */
static ASTNode *parse_factor(void) {
    Token *t = peek();
    if (t->type == TOKEN_KEYWORD && strcmp(t->value,"not") == 0) {
        ASTNode *n = new_node(NODE_UNARY_OP);
        n->data.unary.op = strdup(advance()->value);
        n->data.unary.operand = parse_factor();
        return n;
    }
    if (t->type == TOKEN_INT_LIT)    { ASTNode *n=new_node(NODE_INT_LIT);    n->data.int_val=(int)atoi(advance()->value);        return n; }
    if (t->type == TOKEN_FLOAT_LIT)  { ASTNode *n=new_node(NODE_FLOAT_LIT);  n->data.float_val=(float)atof(advance()->value);    return n; }
    if (t->type == TOKEN_STRING_LIT) { ASTNode *n=new_node(NODE_STRING_LIT); n->data.string_val=strdup(advance()->value);        return n; }
    if (t->type == TOKEN_KEYWORD && (strcmp(t->value,"true")==0||strcmp(t->value,"false")==0)) {
        ASTNode *n=new_node(NODE_BOOL_LIT); n->data.bool_val=(strcmp(advance()->value,"true")==0); return n;
    }
    if (t->type == TOKEN_IDENT || t->type == TOKEN_KEYWORD) {
        ASTNode *n=new_node(NODE_IDENT); n->data.ident_val=strdup(advance()->value); return n;
    }
    if (t->type == TOKEN_SEPARATOR && strcmp(t->value,"(") == 0) {
        advance(); ASTNode *inner=parse_expr(); expect_sep(")"); return inner;
    }
    error_exit("expression"); return NULL;
}

static ASTNode *parse_term(void) {
    ASTNode *l = parse_factor();
    while (peek()->type==TOKEN_OPERATOR && (strcmp(peek()->value,"*")==0||strcmp(peek()->value,"/")==0)) {
        char *op=strdup(advance()->value); ASTNode *n=new_node(NODE_BINARY_OP);
        n->data.binary.op=op; n->data.binary.left=l; n->data.binary.right=parse_factor(); l=n;
    }
    return l;
}
static ASTNode *parse_add(void) {
    ASTNode *l = parse_term();
    while (peek()->type==TOKEN_OPERATOR && (strcmp(peek()->value,"+")==0||strcmp(peek()->value,"-")==0)) {
        char *op=strdup(advance()->value); ASTNode *n=new_node(NODE_BINARY_OP);
        n->data.binary.op=op; n->data.binary.left=l; n->data.binary.right=parse_term(); l=n;
    }
    return l;
}
static ASTNode *parse_rel(void) {
    ASTNode *l = parse_add();
    if (peek()->type==TOKEN_OPERATOR) {
        const char *v=peek()->value;
        if (strcmp(v,"==")==0||strcmp(v,"!=")==0||strcmp(v,"<")==0||
            strcmp(v,">")==0||strcmp(v,"<=")==0||strcmp(v,">=")==0) {
            char *op=strdup(advance()->value); ASTNode *n=new_node(NODE_BINARY_OP);
            n->data.binary.op=op; n->data.binary.left=l; n->data.binary.right=parse_add(); l=n;
        }
    }
    return l;
}
static ASTNode *parse_expr(void) {
    ASTNode *l = parse_rel();
    while (peek()->type==TOKEN_KEYWORD && (strcmp(peek()->value,"and")==0||strcmp(peek()->value,"or")==0)) {
        char *op=strdup(advance()->value); ASTNode *n=new_node(NODE_BINARY_OP);
        n->data.binary.op=op; n->data.binary.left=l; n->data.binary.right=parse_rel(); l=n;
    }
    return l;
}

/* ── Statement & block parsing ──────────────────────────── */
static ASTNode *parse_block(void);
static ASTNode *parse_statement(void);

/*
 * parse_if: handles full else-if chains.
 *
 *   if (cond) { ... }
 *   else if (cond) { ... }   ← becomes else_block = another NODE_STMT_IF
 *   else { ... }
 */
static ASTNode *parse_if(void) {
    advance();  /* consume 'if' */
    expect_sep("(");
    ASTNode *n = new_node(NODE_STMT_IF);
    n->data.if_stmt.cond       = parse_expr();
    expect_sep(")");
    n->data.if_stmt.then_block = parse_block();
    n->data.if_stmt.else_block = NULL;

    if (peek()->type == TOKEN_KEYWORD && strcmp(peek()->value,"else") == 0) {
        advance();  /* consume 'else' */

        /* else if → recurse into another if node (no wrapping block needed) */
        if (peek()->type == TOKEN_KEYWORD && strcmp(peek()->value,"if") == 0) {
            n->data.if_stmt.else_block = parse_if();
        } else {
            /* plain else { ... } */
            n->data.if_stmt.else_block = parse_block();
        }
    }
    return n;
}

static ASTNode *parse_action_call(void) {
    Token *t = peek();
    if (t->type != TOKEN_IDENT && t->type != TOKEN_KEYWORD) error_exit("action name");
    ASTNode *n = new_node(NODE_STMT_ACTION);
    n->data.action.name         = strdup(advance()->value);
    n->data.action.arg_capacity = 8;
    n->data.action.arg_count    = 0;
    n->data.action.args         = (ASTNode **)malloc(8 * sizeof(ASTNode *));
    expect_sep("(");
    while (strcmp(peek()->value,")") != 0) {
        if (peek()->type == TOKEN_EOF) error_exit("')'");
        n->data.action.args = dyn_push(n->data.action.args,
            &n->data.action.arg_count, &n->data.action.arg_capacity, parse_expr());
        if (strcmp(peek()->value,",") == 0) advance();
    }
    expect_sep(")");
    bool zero_ok = (strcmp(n->data.action.name,"end_phase")==0 ||
                    strcmp(n->data.action.name,"eliminate")==0  ||
                    strcmp(n->data.action.name,"draw")==0       ||
                    strcmp(n->data.action.name,"print")==0      ||
                    strcmp(n->data.action.name,"break")==0);
    if (!zero_ok && n->data.action.arg_count == 0) {
        fprintf(stderr,"SyntaxError on line %d: action '%s' requires at least one argument\n",
                n->line, n->data.action.name);
        exit(1);
    }
    return n;
}

static ASTNode *parse_statement(void) {
    Token *t = peek();

    /* if */
    if (t->type == TOKEN_KEYWORD && strcmp(t->value,"if") == 0)
        return parse_if();

    /* while */
    if (t->type == TOKEN_KEYWORD && strcmp(t->value,"while") == 0) {
        advance(); expect_sep("(");
        ASTNode *n = new_node(NODE_STMT_WHILE);
        n->data.while_stmt.cond = parse_expr(); expect_sep(")");
        n->data.while_stmt.body = parse_block();
        return n;
    }

    /* break — can be a keyword-statement (no parentheses) */
    if (t->type == TOKEN_KEYWORD && strcmp(t->value,"break") == 0) {
        advance();
        return ast_create_node(NODE_STMT_BREAK, t->line_number);
    }

    /* ident -> expr */
    if ((t->type == TOKEN_IDENT) &&
        peek2()->type == TOKEN_OPERATOR && strcmp(peek2()->value,"->") == 0) {
        ASTNode *n = new_node(NODE_STMT_ASSIGN);
        n->data.assignment.var_name = strdup(advance()->value);
        advance();  /* consume '->' */
        n->data.assignment.expr = parse_expr();
        return n;
    }

    return parse_action_call();
}

static ASTNode *parse_block(void) {
    expect_sep("{");
    ASTNode *n = ast_create_node(NODE_BLOCK, peek()->line_number);
    n->data.block.capacity = 16; n->data.block.count = 0;
    n->data.block.stmts = (ASTNode **)malloc(16 * sizeof(ASTNode *));
    while (peek()->type != TOKEN_EOF && strcmp(peek()->value,"}") != 0) {
        if (peek()->type == TOKEN_COMMENT) { advance(); continue; }
        n->data.block.stmts = dyn_push(n->data.block.stmts,
            &n->data.block.count, &n->data.block.capacity, parse_statement());
    }
    expect_sep("}");
    return n;
}

/* ── Declaration parsing ────────────────────────────────── */
static ASTNode *parse_prop_block(NodeType ntype, const char *keyword) {
    advance();
    ASTNode *n = new_node(ntype);
    n->data.resource.name = (ntype == NODE_PLAYERS) ? strdup("players") : strdup(advance()->value);
    n->data.resource.prop_capacity = 16; n->data.resource.prop_count = 0;
    n->data.resource.props = (ASTNode **)malloc(16 * sizeof(ASTNode *));
    expect_sep("{");
    while (peek()->type != TOKEN_EOF && strcmp(peek()->value,"}") != 0) {
        if (peek()->type == TOKEN_COMMENT) { advance(); continue; }
        if (ntype == NODE_DECK && peek()->type == TOKEN_KEYWORD && strcmp(peek()->value,"card") == 0) {
            ASTNode *card = parse_prop_block(NODE_CARD,"card");
            n->data.resource.props = dyn_push(n->data.resource.props,
                &n->data.resource.prop_count, &n->data.resource.prop_capacity, card);
            continue;
        }
        ASTNode *p = ast_create_node(NODE_PROP, peek()->line_number);
        if (peek()->type != TOKEN_IDENT && peek()->type != TOKEN_KEYWORD) error_exit("property name");
        p->data.prop.key = strdup(advance()->value);
        if (strcmp(peek()->value,":") != 0) error_exit("':'"); advance();
        Token *val = peek();
        if (val->type == TOKEN_INT_LIT) { p->data.prop.is_int=true; p->data.prop.val_int=atoi(advance()->value); }
        else if (val->type==TOKEN_KEYWORD && (strcmp(val->value,"true")==0||strcmp(val->value,"false")==0))
            { p->data.prop.is_bool=true; p->data.prop.bool_val=(strcmp(advance()->value,"true")==0); }
        else { p->data.prop.is_int=false; p->data.prop.val_str=strdup(advance()->value); }
        n->data.resource.props = dyn_push(n->data.resource.props,
            &n->data.resource.prop_count, &n->data.resource.prop_capacity, p);
        if (strcmp(peek()->value,",") == 0) advance();
    }
    expect_sep("}");
    return n;
    (void)keyword;
}

static ASTNode *parse_turn_structure(void) {
    advance();
    ASTNode *n = new_node(NODE_TURN_STRUCT);
    n->data.turn_struct.capacity=8; n->data.turn_struct.count=0;
    n->data.turn_struct.phases=(ASTNode **)malloc(8*sizeof(ASTNode *));
    expect_sep("{");
    while (peek()->type != TOKEN_EOF && strcmp(peek()->value,"}") != 0) {
        if (peek()->type==TOKEN_COMMENT) { advance(); continue; }
        if (peek()->type!=TOKEN_KEYWORD||strcmp(peek()->value,"phase")!=0) error_exit("'phase'");
        advance();
        ASTNode *ph = ast_create_node(NODE_PHASE, peek()->line_number);
        ph->data.phase.name = strdup(advance()->value);
        ph->data.phase.body = parse_block();
        n->data.turn_struct.phases = dyn_push(n->data.turn_struct.phases,
            &n->data.turn_struct.count, &n->data.turn_struct.capacity, ph);
    }
    expect_sep("}");
    return n;
}

static ASTNode *parse_event(void) {
    advance();
    ASTNode *n = new_node(NODE_EVENT);
    n->data.event.event_type = strdup(advance()->value);
    n->data.event.param_capacity=4; n->data.event.param_count=0;
    n->data.event.params=(char **)malloc(4*sizeof(char *));
    expect_sep("(");
    if (strcmp(peek()->value,")") != 0) {
        do {
            if (n->data.event.param_count >= n->data.event.param_capacity) {
                n->data.event.param_capacity *= 2;
                n->data.event.params=(char **)realloc(n->data.event.params,
                    n->data.event.param_capacity*sizeof(char *));
            }
            if (peek()->type != TOKEN_IDENT) error_exit("parameter name");
            n->data.event.params[n->data.event.param_count++] = strdup(advance()->value);
            if (strcmp(peek()->value,",") == 0) advance(); else break;
        } while (1);
    }
    expect_sep(")");
    n->data.event.body = parse_block();
    return n;
}

static ASTNode *parse_declaration(void) {
    Token *t = peek();
    if (t->type == TOKEN_KEYWORD) {
        if (strcmp(t->value,"resource")==0)      return parse_prop_block(NODE_RESOURCE,"resource");
        if (strcmp(t->value,"arena")==0)          return parse_prop_block(NODE_ARENA,"arena");
        if (strcmp(t->value,"tile")==0)           return parse_prop_block(NODE_TILE,"tile");
        if (strcmp(t->value,"deck")==0)           return parse_prop_block(NODE_DECK,"deck");
        if (strcmp(t->value,"players")==0)        return parse_prop_block(NODE_PLAYERS,"players");
        if (strcmp(t->value,"turn_structure")==0) return parse_turn_structure();
        if (strcmp(t->value,"on")==0)             return parse_event();
    }
    error_exit("top-level declaration (resource/arena/tile/deck/players/turn_structure/on)");
    return NULL;
}

ASTNode *parse_program(TokenList *tokens) {
    g_tl = tokens; g_pos = 0;
    while (peek()->type == TOKEN_COMMENT) advance();
    ASTNode *root = ast_create_node(NODE_PROGRAM, 1);
    root->data.program.capacity=32; root->data.program.count=0;
    root->data.program.decls=(ASTNode **)malloc(32*sizeof(ASTNode *));
    while (peek()->type != TOKEN_EOF) {
        root->data.program.decls = dyn_push(root->data.program.decls,
            &root->data.program.count, &root->data.program.capacity, parse_declaration());
        while (peek()->type == TOKEN_COMMENT) advance();
    }
    return root;
}