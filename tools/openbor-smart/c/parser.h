#ifndef OPENBOR_SMART_C_PARSER_H
#define OPENBOR_SMART_C_PARSER_H

#include "lexer.h"
#include <stddef.h>

typedef enum {
    ST_BLOCK, ST_DECLARATION, ST_RETURN, ST_EXPRESSION, ST_IF, ST_WHILE,
    ST_DO_WHILE, ST_FOR, ST_SWITCH, ST_BREAK, ST_CONTINUE, ST_EMPTY
} StatementKind;

typedef struct Statement Statement;
typedef struct {
    char *name;
    int pointer_depth;
    char **array_sizes;
    size_t array_count;
    char *initializer;
} Declarator;

typedef struct {
    char *type_name;
    Declarator *items;
    size_t count;
} Declaration;

typedef struct {
    char *value;
    int is_default;
    Statement **statements;
    size_t count;
} SwitchCase;

struct Statement {
    StatementKind kind;
    union {
        struct { Statement **items; size_t count; } block;
        Declaration declaration;
        struct { char *expression; } expression;
        struct { char *condition; Statement *then_branch; Statement *else_branch; } if_stmt;
        struct { char *condition; Statement *body; } while_stmt;
        struct { Statement *body; char *condition; } do_while_stmt;
        struct { Statement *initializer; char *condition; char *post; Statement *body; } for_stmt;
        struct { char *expression; SwitchCase *cases; size_t count; } switch_stmt;
    } as;
};

typedef struct {
    char *return_type;
    char *name;
    char **parameters;
    size_t parameter_count;
    Statement *body;
    unsigned line;
} CFunction;

typedef struct {
    CFunction *items;
    size_t count;
} FunctionList;

int parse_c_source(const char *source, FunctionList *out, char *error, size_t error_size);
void free_function_list(FunctionList *functions);

#endif
