#ifndef OPENBOR_SMART_C_LEXER_H
#define OPENBOR_SMART_C_LEXER_H

#include <stddef.h>

typedef enum {
    TOK_WHITESPACE,
    TOK_COMMENT,
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_STRING,
    TOK_CHARACTER,
    TOK_OPERATOR,
    TOK_PUNCTUATION
} TokenKind;

typedef struct {
    TokenKind kind;
    char *text;
    size_t offset;
    size_t length;
    unsigned line;
    unsigned column;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenList;

int lex_source(const char *source, TokenList *out, char *error, size_t error_size);
void free_tokens(TokenList *tokens);

#endif
