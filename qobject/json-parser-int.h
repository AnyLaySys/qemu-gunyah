
#ifndef JSON_PARSER_INT_H
#define JSON_PARSER_INT_H

#include "qobject/json-parser.h"

typedef enum json_token_type {
    JSON_ERROR = 0,             /* must be zero, see json_lexer[] */
    JSON_LCURLY = 100,
    JSON_MIN = JSON_LCURLY,
    JSON_RCURLY,
    JSON_LSQUARE,
    JSON_RSQUARE,
    JSON_COLON,
    JSON_COMMA,
    JSON_INTEGER,
    JSON_FLOAT,
    JSON_KEYWORD,
    JSON_STRING,
    JSON_INTERP,
    JSON_END_OF_INPUT,
    JSON_MAX = JSON_END_OF_INPUT
} JSONTokenType;

typedef struct JSONToken JSONToken;

void json_lexer_init(JSONLexer *lexer, bool enable_interpolation);
void json_lexer_feed(JSONLexer *lexer, const char *buffer, size_t size);
void json_lexer_flush(JSONLexer *lexer);
void json_lexer_destroy(JSONLexer *lexer);

void json_message_process_token(JSONLexer *lexer, GString *input,
                                JSONTokenType type, int x, int y);

JSONToken *json_token(JSONTokenType type, int x, int y, GString *tokstr);
QObject *json_parser_parse(GQueue *tokens, va_list *ap, Error **errp);

#endif
