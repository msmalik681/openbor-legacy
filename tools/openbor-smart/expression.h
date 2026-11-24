#ifndef OPENBOR_SMART_C_EXPRESSION_H
#define OPENBOR_SMART_C_EXPRESSION_H

#include <stddef.h>

typedef enum {
    EXPR_IDENTIFIER, EXPR_LITERAL, EXPR_CAST, EXPR_SIZEOF_TYPE, EXPR_UNARY,
    EXPR_BINARY, EXPR_CONDITIONAL, EXPR_CALL, EXPR_INDEX, EXPR_MEMBER, EXPR_COMMA
} ExprKind;

typedef struct Expr Expr;
struct Expr {
    ExprKind kind;
    char *text;
    int flag;
    Expr *left;
    Expr *right;
    Expr *third;
    Expr **items;
    size_t count;
};

Expr *parse_c_expression(const char *source, char *error, size_t error_size);
void free_expression(Expr *expr);
char *expression_to_lua(const Expr *expr, char *error, size_t error_size);
char *statement_expression_to_lua(const char *source, char *error, size_t error_size);

#endif
