#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <stdbool.h>

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,    /* NEW */
    VAL_ERROR
} ValueType;

typedef struct {
    ValueType type;
    union {
        int   i_val;
        float f_val;
        char *s_val;
        bool  b_val;  /* NEW */
    } data;
} Value;

typedef struct VarEntry {
    char       *name;
    Value       value;
    struct VarEntry *next;
} VarEntry;

typedef struct Environment {
    VarEntry        *variables;
    struct Environment *parent;
} Environment;

void   push_scope(void);
void   pop_scope(void);
void   declare_var(const char *name, ValueType type, Value val);
bool   assign_var(const char *name, Value val);
Value *get_var(const char *name);

#endif
