#include "environment.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Environment *current_scope = NULL;

void push_scope(void) {
    Environment *e = (Environment *)malloc(sizeof(Environment));
    if (!e) { fprintf(stderr, "Fatal: out of memory\n"); exit(1); }
    e->variables = NULL;
    e->parent    = current_scope;
    current_scope = e;
}

void pop_scope(void) {
    if (!current_scope) { fprintf(stderr, "Error: no scope to pop\n"); return; }
    Environment *old = current_scope;
    VarEntry    *v   = old->variables;
    while (v) {
        VarEntry *nxt = v->next;
        free(v->name);
        if (v->value.type == VAL_STRING) free(v->value.data.s_val);
        free(v);
        v = nxt;
    }
    current_scope = old->parent;
    free(old);
}

void declare_var(const char *name, ValueType type, Value val) {
    if (!current_scope) push_scope();
    VarEntry *e = (VarEntry *)malloc(sizeof(VarEntry));
    e->name       = strdup(name);
    e->value.type = type;
    if (type == VAL_STRING)
        e->value.data.s_val = strdup(val.data.s_val ? val.data.s_val : "");
    else
        e->value = val;
    e->next = current_scope->variables;
    current_scope->variables = e;
}

bool assign_var(const char *name, Value val) {
    for (Environment *env = current_scope; env; env = env->parent) {
        for (VarEntry *v = env->variables; v; v = v->next) {
            if (strcmp(v->name, name) == 0) {
                if (v->value.type == VAL_STRING) free(v->value.data.s_val);
                v->value.type = val.type;
                if (val.type == VAL_STRING)
                    v->value.data.s_val = strdup(val.data.s_val ? val.data.s_val : "");
                else
                    v->value = val;
                return true;
            }
        }
    }
    return false;
}

Value *get_var(const char *name) {
    for (Environment *env = current_scope; env; env = env->parent) {
        for (VarEntry *v = env->variables; v; v = v->next) {
            if (strcmp(v->name, name) == 0) return &v->value;
        }
    }
    return NULL;
}