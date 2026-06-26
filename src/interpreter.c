#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "ast.h"
#include "environment.h"
#include "interpreter.h"

/* ── Control-flow flags ─────────────────────────────────── */
bool g_phase_ended  = false;   /* end_phase() was called  */
bool g_break_loop   = false;   /* break was executed       */

/* ── Turn scope ─────────────────────────────────────────── */
/*
 * The turn scope sits between the global scope and each
 * phase / event scope. Variables assigned in any phase
 * automatically land here (via assign_var walking up the
 * chain) and are therefore visible in later phases AND in
 * event handlers triggered during the same turn.
 *
 * Layout (innermost → outermost):
 *   [event / phase scope]  ← pushed/popped per block
 *   [turn scope]           ← pushed once per turn, lives
 *                             until interpreter_end_turn()
 *   [global scope]         ← pushed once at program start
 */
void interpreter_begin_turn(void) {
    push_scope();
}

void interpreter_end_turn(void) {
    pop_scope();
}

/* ── Helper: seed built-in variables into the turn scope ── */
/*
 * These are always available inside any phase or event:
 *
 *   player_id     integer  1-based index of the current player
 *   player_hp     integer  current player's HP
 *   player_money  integer  current player's gold
 *   player_pos    integer  current player's board position
 *   result        integer  last roll() or random() result
 */
void interpreter_seed_player(int id, int hp, int money, int pos) {
    Value v; v.type = VAL_INT;
    v.data.i_val = id;     declare_var("player_id",    VAL_INT, v);
    v.data.i_val = hp;     declare_var("player_hp",    VAL_INT, v);
    v.data.i_val = money;  declare_var("player_money", VAL_INT, v);
    v.data.i_val = pos;    declare_var("player_pos",   VAL_INT, v);
    v.data.i_val = 0;      declare_var("result",       VAL_INT, v);
}

/* ── Truthiness ─────────────────────────────────────────── */
static bool is_truthy(Value v) {
    if (v.type == VAL_BOOL)  return v.data.b_val;
    if (v.type == VAL_INT)   return v.data.i_val != 0;
    if (v.type == VAL_FLOAT) return v.data.f_val != 0.0f;
    return false;
}

/* ── Expression evaluator ───────────────────────────────── */
Value eval_expr(ASTNode *expr) {
    Value result; result.type = VAL_ERROR;
    if (!expr) return result;

    switch (expr->type) {
        case NODE_INT_LIT:
            result.type = VAL_INT; result.data.i_val = expr->data.int_val; break;
        case NODE_FLOAT_LIT:
            result.type = VAL_FLOAT; result.data.f_val = expr->data.float_val; break;
        case NODE_STRING_LIT:
            result.type = VAL_STRING; result.data.s_val = strdup(expr->data.string_val); break;
        case NODE_BOOL_LIT:
            result.type = VAL_BOOL; result.data.b_val = expr->data.bool_val; break;

        case NODE_IDENT: {
            Value *v = get_var(expr->data.ident_val);
            if (!v) {
                fprintf(stderr,"Runtime Error: Undefined variable '%s' at line %d\n",
                        expr->data.ident_val, expr->line);
                exit(EXIT_FAILURE);
            }
            result = *v;
            if (result.type == VAL_STRING)
                result.data.s_val = strdup(result.data.s_val);
            break;
        }

        case NODE_UNARY_OP: {
            Value op = eval_expr(expr->data.unary.operand);
            if (strcmp(expr->data.unary.op,"not") == 0) {
                result.type = VAL_BOOL;
                if (op.type==VAL_BOOL)  result.data.b_val = !op.data.b_val;
                else if (op.type==VAL_INT) result.data.b_val = (op.data.i_val == 0);
                else result.data.b_val = false;
            }
            break;
        }

        case NODE_BINARY_OP: {
            const char *op = expr->data.binary.op;

            /* Short-circuit logic */
            if (strcmp(op,"and") == 0) {
                Value l = eval_expr(expr->data.binary.left);
                if (!is_truthy(l)) { result.type=VAL_BOOL; result.data.b_val=false; break; }
                Value r = eval_expr(expr->data.binary.right);
                result.type=VAL_BOOL; result.data.b_val=is_truthy(r); break;
            }
            if (strcmp(op,"or") == 0) {
                Value l = eval_expr(expr->data.binary.left);
                if (is_truthy(l))  { result.type=VAL_BOOL; result.data.b_val=true; break; }
                Value r = eval_expr(expr->data.binary.right);
                result.type=VAL_BOOL; result.data.b_val=is_truthy(r); break;
            }

            Value left  = eval_expr(expr->data.binary.left);
            Value right = eval_expr(expr->data.binary.right);
            if (left.type==VAL_ERROR || right.type==VAL_ERROR) return result;

            /* String ops */
            if (left.type==VAL_STRING || right.type==VAL_STRING) {
                if (strcmp(op,"+")==0) {
                    char *ls=(left.type==VAL_STRING)?left.data.s_val:"";
                    char *rs=(right.type==VAL_STRING)?right.data.s_val:"";
                    size_t len=strlen(ls)+strlen(rs)+1;
                    char *out=(char*)malloc(len);
                    snprintf(out,len,"%s%s",ls,rs);
                    result.type=VAL_STRING; result.data.s_val=out;
                } else if (strcmp(op,"==")==0) {
                    result.type=VAL_BOOL;
                    result.data.b_val=(left.type==VAL_STRING&&right.type==VAL_STRING&&
                                       strcmp(left.data.s_val,right.data.s_val)==0);
                } else if (strcmp(op,"!=")==0) {
                    result.type=VAL_BOOL;
                    result.data.b_val=!(left.type==VAL_STRING&&right.type==VAL_STRING&&
                                        strcmp(left.data.s_val,right.data.s_val)==0);
                } else {
                    fprintf(stderr,"Runtime Error: invalid string op '%s' at line %d\n",op,expr->line);
                    exit(EXIT_FAILURE);
                }
                if (left.type==VAL_STRING)  free(left.data.s_val);
                if (right.type==VAL_STRING) free(right.data.s_val);
                break;
            }

            /* Numeric */
            bool use_float = (left.type==VAL_FLOAT || right.type==VAL_FLOAT);
            float l=(left.type==VAL_FLOAT)?left.data.f_val:(float)left.data.i_val;
            float r=(right.type==VAL_FLOAT)?right.data.f_val:(float)right.data.i_val;

            if      (strcmp(op,"+")==0) {
                if(use_float){result.type=VAL_FLOAT;result.data.f_val=l+r;}
                else{result.type=VAL_INT;result.data.i_val=left.data.i_val+right.data.i_val;}
            } else if (strcmp(op,"-")==0) {
                if(use_float){result.type=VAL_FLOAT;result.data.f_val=l-r;}
                else{result.type=VAL_INT;result.data.i_val=left.data.i_val-right.data.i_val;}
            } else if (strcmp(op,"*")==0) {
                if(use_float){result.type=VAL_FLOAT;result.data.f_val=l*r;}
                else{result.type=VAL_INT;result.data.i_val=left.data.i_val*right.data.i_val;}
            } else if (strcmp(op,"/")==0) {
                if(r==0){fprintf(stderr,"Runtime Error: division by zero at line %d\n",expr->line);exit(1);}
                if(use_float){result.type=VAL_FLOAT;result.data.f_val=l/r;}
                else{result.type=VAL_INT;result.data.i_val=left.data.i_val/right.data.i_val;}
            } else {
                result.type=VAL_BOOL;
                if      (strcmp(op,"==")==0) result.data.b_val=(l==r);
                else if (strcmp(op,"!=")==0) result.data.b_val=(l!=r);
                else if (strcmp(op,"<")==0)  result.data.b_val=(l<r);
                else if (strcmp(op,">")==0)  result.data.b_val=(l>r);
                else if (strcmp(op,"<=")==0) result.data.b_val=(l<=r);
                else if (strcmp(op,">=")==0) result.data.b_val=(l>=r);
            }
            break;
        }

        default:
            fprintf(stderr,"Runtime Error: unhandled expr type %d at line %d\n",expr->type,expr->line);
            break;
    }
    return result;
}

/* ── Statement executor ─────────────────────────────────── */
void exec_stmt(ASTNode *stmt) {
    if (!stmt) return;

    switch (stmt->type) {

        case NODE_STMT_ASSIGN: {
            Value val = eval_expr(stmt->data.assignment.expr);
            if (val.type == VAL_ERROR) {
                fprintf(stderr,"Runtime Error: bad assignment at line %d\n",stmt->line);
                exit(1);
            }
            if (!assign_var(stmt->data.assignment.var_name, val))
                declare_var(stmt->data.assignment.var_name, val.type, val);
            if (val.type == VAL_STRING) free(val.data.s_val);
            break;
        }

        case NODE_STMT_BREAK:
            g_break_loop = true;
            break;

        case NODE_STMT_ACTION: {
            char *name = stmt->data.action.name;

            if (strcmp(name,"heal")==0 && stmt->data.action.arg_count>=1) {
                Value a=eval_expr(stmt->data.action.args[0]);
                if(a.type==VAL_INT&&a.data.i_val<=0){
                    fprintf(stderr,"Domain Error [Line %d]: 'heal' amount must be positive.\n",stmt->line);
                    exit(1);
                }
                printf("[GAME ENGINE] heal: +%d HP\n",a.data.i_val);
                /* update player_hp built-in */
                Value *phv=get_var("player_hp");
                if(phv&&phv->type==VAL_INT){
                    Value nv; nv.type=VAL_INT; nv.data.i_val=phv->data.i_val+a.data.i_val;
                    assign_var("player_hp",nv);
                }

            } else if (strcmp(name,"damage")==0 && stmt->data.action.arg_count>=1) {
                Value a=eval_expr(stmt->data.action.args[0]);
                printf("[GAME ENGINE] damage: -%d HP\n",a.data.i_val);
                Value *phv=get_var("player_hp");
                if(phv&&phv->type==VAL_INT){
                    Value nv; nv.type=VAL_INT; nv.data.i_val=phv->data.i_val-a.data.i_val;
                    if(nv.data.i_val<0) nv.data.i_val=0;
                    assign_var("player_hp",nv);
                }

            } else if (strcmp(name,"pay")==0 && stmt->data.action.arg_count>=1) {
                Value a=eval_expr(stmt->data.action.args[0]);
                printf("[GAME ENGINE] pay: -%d gold\n",a.data.i_val);
                Value *pmv=get_var("player_money");
                if(pmv&&pmv->type==VAL_INT){
                    Value nv; nv.type=VAL_INT; nv.data.i_val=pmv->data.i_val-a.data.i_val;
                    if(nv.data.i_val<0) nv.data.i_val=0;
                    assign_var("player_money",nv);
                }

            } else if (strcmp(name,"transfer")==0 && stmt->data.action.arg_count>=1) {
                Value a=eval_expr(stmt->data.action.args[0]);
                printf("[GAME ENGINE] transfer: +%d gold\n",a.data.i_val);
                Value *pmv=get_var("player_money");
                if(pmv&&pmv->type==VAL_INT){
                    Value nv; nv.type=VAL_INT; nv.data.i_val=pmv->data.i_val+a.data.i_val;
                    assign_var("player_money",nv);
                }

            } else if (strcmp(name,"roll")==0 && stmt->data.action.arg_count>=1) {
                Value dice=eval_expr(stmt->data.action.args[0]);
                int sides=(stmt->data.action.arg_count>=2)?eval_expr(stmt->data.action.args[1]).data.i_val:6;
                int total=0;
                for(int d=0;d<dice.data.i_val;d++) total+=(rand()%sides)+1;
                printf("[GAME ENGINE] roll(%d d%d) = %d\n",dice.data.i_val,sides,total);
                Value rv; rv.type=VAL_INT; rv.data.i_val=total;
                if(!assign_var("result",rv)) declare_var("result",VAL_INT,rv);

            } else if (strcmp(name,"random")==0 && stmt->data.action.arg_count>=2) {
                Value lo=eval_expr(stmt->data.action.args[0]);
                Value hi=eval_expr(stmt->data.action.args[1]);
                int range=hi.data.i_val-lo.data.i_val+1;
                int val=lo.data.i_val+(range>0?rand()%range:0);
                printf("[GAME ENGINE] random(%d..%d) = %d\n",lo.data.i_val,hi.data.i_val,val);
                Value rv; rv.type=VAL_INT; rv.data.i_val=val;
                if(!assign_var("result",rv)) declare_var("result",VAL_INT,rv);

            } else if (strcmp(name,"advance")==0 && stmt->data.action.arg_count>=1) {
                Value a=eval_expr(stmt->data.action.args[0]);
                printf("[GAME ENGINE] advance: +%d spaces\n",a.data.i_val);
                Value *ppv=get_var("player_pos");
                if(ppv&&ppv->type==VAL_INT){
                    Value nv; nv.type=VAL_INT; nv.data.i_val=ppv->data.i_val+a.data.i_val;
                    assign_var("player_pos",nv);
                }

            } else if (strcmp(name,"move_to")==0 && stmt->data.action.arg_count>=1) {
                Value a=eval_expr(stmt->data.action.args[0]);
                if(a.type==VAL_STRING) printf("[GAME ENGINE] move_to: %s\n",a.data.s_val);
                else printf("[GAME ENGINE] move_to: position %d\n",a.data.i_val);

            } else if (strcmp(name,"draw")==0) {
                if(stmt->data.action.arg_count>=1){
                    Value a=eval_expr(stmt->data.action.args[0]);
                    if(a.type==VAL_STRING) printf("[GAME ENGINE] draw from deck: %s\n",a.data.s_val);
                } else { printf("[GAME ENGINE] draw card\n"); }

            } else if (strcmp(name,"eliminate")==0) {
                printf("[GAME ENGINE] PLAYER ELIMINATED!\n");

            } else if (strcmp(name,"print")==0) {
                for(int i=0;i<stmt->data.action.arg_count;i++){
                    Value v=eval_expr(stmt->data.action.args[i]);
                    if(v.type==VAL_STRING){printf("%s",v.data.s_val);free(v.data.s_val);}
                    else if(v.type==VAL_INT)   printf("%d",v.data.i_val);
                    else if(v.type==VAL_FLOAT)  printf("%f",v.data.f_val);
                    else if(v.type==VAL_BOOL)   printf("%s",v.data.b_val?"true":"false");
                }
                printf("\n");

            } else if (strcmp(name,"end_phase")==0) {
                g_phase_ended=true;
                printf("[GAME ENGINE] end_phase triggered.\n");

            } else {
                printf("[ACTION] %s (%d args)\n",name,stmt->data.action.arg_count);
            }
            break;
        }

        case NODE_STMT_IF: {
            Value cond=eval_expr(stmt->data.if_stmt.cond);
            if(cond.type==VAL_ERROR){
                fprintf(stderr,"Runtime Error: bad if condition at line %d\n",stmt->line); exit(1);
            }
            if(is_truthy(cond))
                exec_stmt(stmt->data.if_stmt.then_block);
            else if(stmt->data.if_stmt.else_block)
                exec_stmt(stmt->data.if_stmt.else_block);
            break;
        }

        case NODE_STMT_WHILE: {
            int iters=0;
            while(1) {
                Value cond=eval_expr(stmt->data.while_stmt.cond);
                if(!is_truthy(cond)) break;
                if(++iters>10000){
                    fprintf(stderr,"Runtime Error: while loop exceeded 10000 iterations\n"); exit(1);
                }
                exec_stmt(stmt->data.while_stmt.body);
                if(g_break_loop){ g_break_loop=false; break; }
                if(g_phase_ended) break;
            }
            break;
        }

        case NODE_BLOCK: {
            push_scope();
            for(int i=0;i<stmt->data.block.count;i++){
                if(g_phase_ended||g_break_loop) break;
                exec_stmt(stmt->data.block.stmts[i]);
            }
            /* reset phase_ended inside block, but NOT break (let it propagate up) */
            g_phase_ended=false;
            pop_scope();
            break;
        }

        default:
            fprintf(stderr,"Runtime Error: unhandled stmt type %d at line %d\n",stmt->type,stmt->line);
            break;
    }
}

/* Execute a block's statements WITHOUT pushing a new scope.
 * Used for phase bodies so variables persist into the turn scope. */
void exec_phase_body(ASTNode *block) {
    if (!block || block->type != NODE_BLOCK) {
        exec_stmt(block);
        return;
    }
    for (int i = 0; i < block->data.block.count; i++) {
        if (g_phase_ended || g_break_loop) break;
        exec_stmt(block->data.block.stmts[i]);
    }
    g_phase_ended = false;
}   