#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_hex(unsigned char c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int append_token(TokenList *list, TokenKind kind, const char *source,
                        size_t start, size_t end, unsigned line, unsigned column) {
    Token *token;
    size_t length = end - start;
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 64;
        Token *items = (Token *)realloc(list->items, capacity * sizeof(*items));
        if (!items) return 0;
        list->items = items;
        list->capacity = capacity;
    }
    token = &list->items[list->count++];
    token->text = (char *)malloc(length + 1);
    if (!token->text) {
        list->count--;
        return 0;
    }
    memcpy(token->text, source + start, length);
    token->text[length] = '\0';
    token->kind = kind;
    token->offset = start;
    token->length = length;
    token->line = line;
    token->column = column;
    return 1;
}

static void advance_one(const char *s, size_t *i, unsigned *line, unsigned *column) {
    if (s[*i] == '\r') {
        ++*i;
        if (s[*i] == '\n') ++*i;
        ++*line;
        *column = 1;
    } else if (s[*i] == '\n') {
        ++*i;
        ++*line;
        *column = 1;
    } else {
        ++*i;
        ++*column;
    }
}

static int is_space_byte(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int is_ident_start(unsigned char c) {
    return c == '_' || isalpha(c);
}

static int is_ident_part(unsigned char c) {
    return is_ident_start(c) || isdigit(c);
}

static size_t operator_length(const char *s) {
    static const char *const operators[] = {
        "<<=", ">>=", "->", "++", "--", "&&", "||", "==", "!=", "<=", ">=",
        "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<", ">>", NULL
    };
    size_t n;
    for (n = 0; operators[n]; ++n) {
        size_t len = strlen(operators[n]);
        if (strncmp(s, operators[n], len) == 0) return len;
    }
    return strchr("+-*/%&|^!~=<>", s[0]) ? 1 : 0;
}

static size_t scan_number(const char *s, size_t i) {
    size_t j = i;
    if (s[j] == '0' && (s[j + 1] == 'x' || s[j + 1] == 'X')) {
        j += 2;
        while (is_hex((unsigned char)s[j]) || s[j] == '_') ++j;
        if (s[j] == '.') {
            ++j;
            while (is_hex((unsigned char)s[j]) || s[j] == '_') ++j;
        }
        if (s[j] == 'p' || s[j] == 'P') {
            ++j;
            if (s[j] == '+' || s[j] == '-') ++j;
            while (isdigit((unsigned char)s[j])) ++j;
        }
    } else {
        while (isdigit((unsigned char)s[j]) || s[j] == '_') ++j;
        if (s[j] == '.') {
            ++j;
            while (isdigit((unsigned char)s[j]) || s[j] == '_') ++j;
        }
        if (s[j] == 'e' || s[j] == 'E') {
            ++j;
            if (s[j] == '+' || s[j] == '-') ++j;
            while (isdigit((unsigned char)s[j])) ++j;
        }
    }
    while (strchr("uUlLfF", s[j]) && s[j]) ++j;
    return j;
}

void free_tokens(TokenList *tokens) {
    size_t i;
    if (!tokens) return;
    for (i = 0; i < tokens->count; ++i) free(tokens->items[i].text);
    free(tokens->items);
    memset(tokens, 0, sizeof(*tokens));
}

int lex_source(const char *source, TokenList *out, char *error, size_t error_size) {
    size_t i = 0, length;
    unsigned line = 1, column = 1;
    if (!source || !out) return 0;
    memset(out, 0, sizeof(*out));
    if (error && error_size) error[0] = '\0';
    length = strlen(source);

    while (i < length) {
        size_t start = i;
        unsigned start_line = line, start_column = column;
        unsigned char c = (unsigned char)source[i];
        TokenKind kind = TOK_PUNCTUATION;

        if (is_space_byte(c)) {
            kind = TOK_WHITESPACE;
            while (i < length && is_space_byte((unsigned char)source[i]))
                advance_one(source, &i, &line, &column);
        } else if (c == '/' && i + 1 < length && source[i + 1] == '/') {
            kind = TOK_COMMENT;
            while (i < length && source[i] != '\n' && source[i] != '\r')
                advance_one(source, &i, &line, &column);
        } else if (c == '/' && i + 1 < length && source[i + 1] == '*') {
            int closed = 0;
            kind = TOK_COMMENT;
            advance_one(source, &i, &line, &column);
            advance_one(source, &i, &line, &column);
            while (i < length) {
                if (source[i] == '*' && i + 1 < length && source[i + 1] == '/') {
                    advance_one(source, &i, &line, &column);
                    advance_one(source, &i, &line, &column);
                    closed = 1;
                    break;
                }
                advance_one(source, &i, &line, &column);
            }
            if (!closed) {
                if (error && error_size) snprintf(error, error_size,
                    "line %u, column %u: unterminated block comment", start_line, start_column);
                free_tokens(out);
                return 0;
            }
        } else if (c == '"' || c == '\'') {
            int closed = 0;
            unsigned char quote = c;
            kind = quote == '"' ? TOK_STRING : TOK_CHARACTER;
            advance_one(source, &i, &line, &column);
            while (i < length) {
                if (source[i] == '\\') {
                    advance_one(source, &i, &line, &column);
                    if (i < length) advance_one(source, &i, &line, &column);
                    continue;
                }
                if ((unsigned char)source[i] == quote) {
                    advance_one(source, &i, &line, &column);
                    closed = 1;
                    break;
                }
                if (source[i] == '\n' || source[i] == '\r') break;
                advance_one(source, &i, &line, &column);
            }
            if (!closed) {
                if (error && error_size) snprintf(error, error_size,
                    "line %u, column %u: unterminated or multiline quoted literal",
                    start_line, start_column);
                free_tokens(out);
                return 0;
            }
        } else if (is_ident_start(c)) {
            kind = TOK_IDENTIFIER;
            do { advance_one(source, &i, &line, &column); }
            while (i < length && is_ident_part((unsigned char)source[i]));
        } else if (isdigit(c) || (c == '.' && i + 1 < length && isdigit((unsigned char)source[i + 1]))) {
            kind = TOK_NUMBER;
            {
                size_t end = scan_number(source, i);
                while (i < end) advance_one(source, &i, &line, &column);
            }
        } else {
            size_t op_len = operator_length(source + i);
            if (op_len) {
                kind = TOK_OPERATOR;
                while (op_len--) advance_one(source, &i, &line, &column);
            } else {
                advance_one(source, &i, &line, &column);
                if (!strchr("(){}[];,.:?@", c)) kind = TOK_OPERATOR;
            }
        }

        if (!append_token(out, kind, source, start, i, start_line, start_column)) {
            if (error && error_size) snprintf(error, error_size, "out of memory while tokenizing source");
            free_tokens(out);
            return 0;
        }
    }
    return 1;
}
