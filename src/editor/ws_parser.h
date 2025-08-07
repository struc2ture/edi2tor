#pragma once

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    TOKEN_IDENT,
    TOKEN_EQUALS,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_COMMA,
    TOKEN_STRING,
    TOKEN_EOF
} Token_Kind;

typedef struct {
    Token_Kind kind;
    const char *start;
    int length;
} Token;

typedef struct {
    const char *src;
} Parser_State;

void ws_parser_skip_whitespace(Parser_State *ps)
{
    while (isspace(*ps->src)) ps->src++;
}

Token ws_parser_next_token(Parser_State *ps)
{
    ws_parser_skip_whitespace(ps);

    const char *s = ps->src;

    if (*s == '\0') return (Token){TOKEN_EOF, s, 0};
    if (*s == '=') return ps->src++, (Token){TOKEN_EQUALS, s, 1};
    if (*s == '{') return ps->src++, (Token){TOKEN_LBRACE, s, 1};
    if (*s == '}') return ps->src++, (Token){TOKEN_RBRACE, s, 1};
    if (*s == '(') return ps->src++, (Token){TOKEN_LPAREN, s, 1};
    if (*s == ')') return ps->src++, (Token){TOKEN_RPAREN, s, 1};
    if (*s == ',') return ps->src++, (Token){TOKEN_COMMA, s, 1};

    if (*s == '\'') {
        s++;
        const char *start = s;
        while (*s && *s != '\'') s++;
        Token t = {TOKEN_STRING, start, s - start};
        if (*s == '\'') s++;
        ps->src = s;
        return t;
    }

    if (isalpha(*s) || *s == '_')
    {
        const char *start = s;
        while (isalnum(*s) || *s == '_') s++;
        Token t = {TOKEN_IDENT, start, s - start};
        ps->src = s;
        return t;
    }

    // Fallback: treat as identifier-like
    const char *start = s;
    while (*s && !isspace(*s) && *s != '=' && *s != '{' && *s != '}' && *s != '(' && *s != ')' && *s != ',' && *s != '\'')
    {
        s++;
    }

    Token t = {TOKEN_IDENT, start, s - start};
    ps->src = s;
    return t;
}

const char *_ws_parser_print_tokens_get_token_kind_str(Token_Kind kind)
{
    switch (kind)
    {
        case TOKEN_IDENT:
            return "Identifier";
        case TOKEN_EQUALS:
            return "Equals";
        case TOKEN_LBRACE:
            return "LBrace";
        case TOKEN_RBRACE:
            return "RBrace";
        case TOKEN_LPAREN:
            return "LParen";
        case TOKEN_RPAREN:
            return "RParen";
        case TOKEN_COMMA:
            return "Comma";
        case TOKEN_STRING:
            return "String";
        case TOKEN_EOF:
            return "EOF";
    }
}

void ws_parser_print_tokens(Parser_State *ps)
{
    while (*ps->src)
    {
        Token token = ws_parser_next_token(ps);
        printf("Token: kind=%s, value=%.*s\n", _ws_parser_print_tokens_get_token_kind_str(token.kind), token.length, token.start);
    }
}

void ws_parser_print_block(Parser_State *ps);
void ws_parser_print_tuple(Parser_State *ps);

void ws_parser_print_kv(Parser_State *ps)
{
    Token key = ws_parser_next_token(ps);
    if (key.kind != TOKEN_IDENT) return;

    Token eq = ws_parser_next_token(ps);
    if (eq.kind != TOKEN_EQUALS) return;

    Token val = ws_parser_next_token(ps);
    switch (val.kind)
    {
        case TOKEN_LBRACE:
            printf("%.*s = block {\n", key.length, key.start);
            ws_parser_print_block(ps);
            printf("}\n");
            break;
        case TOKEN_LPAREN:
            printf("%.*s = tuple (", key.length, key.start);
            ws_parser_print_tuple(ps);
            printf(")\n");
            break;
        case TOKEN_STRING:
            printf("%.*s = string '%.*s'\n", key.length, key.start, val.length, val.start);
            break;
        case TOKEN_IDENT:
            printf("%.*s = %.*s\n", key.length, key.start, val.length, val.start);
            break;
        default: break;
    }
}

void ws_parser_print_block(Parser_State *ps)
{
    while (1)
    {
        Token t = ws_parser_next_token(ps);
        if (t.kind == TOKEN_RBRACE || t.kind == TOKEN_EOF) break;
        ps->src = t.start; // rewind so parse_kv can reconsume the ident
        ws_parser_print_kv(ps);
    }
}

void ws_parser_print_tuple(Parser_State *ps)
{
    while (1)
    {
        Token t = ws_parser_next_token(ps);
        if (t.kind == TOKEN_RPAREN || t.kind == TOKEN_EOF) break;
        if (t.kind != TOKEN_COMMA)
        {
            printf("%.*s ", t.length, t.start);
        }
    }
}

void ws_parser_print_kvs(Parser_State *ps)
{
    while (*ps->src)
    {
        ws_parser_print_kv(ps);
    }
}
