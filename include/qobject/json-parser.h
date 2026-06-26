
#ifndef QAPI_QMP_JSON_PARSER_H
#define QAPI_QMP_JSON_PARSER_H

typedef struct JSONLexer {
    int start_state, state;
    GString *token;
    int x, y;
} JSONLexer;

typedef struct JSONMessageParser {
    void (*emit)(void *opaque, QObject *json, Error *err);
    void *opaque;
    va_list *ap;
    JSONLexer lexer;
    int brace_count;
    int bracket_count;
    GQueue tokens;
    uint64_t token_size;
} JSONMessageParser;

void json_message_parser_init(JSONMessageParser *parser,
                              void (*emit)(void *opaque, QObject *json,
                                           Error *err),
                              void *opaque, va_list *ap);

void json_message_parser_feed(JSONMessageParser *parser,
                             const char *buffer, size_t size);

void json_message_parser_flush(JSONMessageParser *parser);

void json_message_parser_destroy(JSONMessageParser *parser);

#endif
