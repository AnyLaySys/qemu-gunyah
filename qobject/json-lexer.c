
#include "qemu/osdep.h"
#include "json-parser-int.h"

#define MAX_TOKEN_SIZE (64ULL << 20)


enum json_lexer_state {
    IN_RECOVERY = 1,
    IN_DQ_STRING_ESCAPE,
    IN_DQ_STRING,
    IN_SQ_STRING_ESCAPE,
    IN_SQ_STRING,
    IN_ZERO,
    IN_EXP_DIGITS,
    IN_EXP_SIGN,
    IN_EXP_E,
    IN_MANTISSA,
    IN_MANTISSA_DIGITS,
    IN_DIGITS,
    IN_SIGN,
    IN_KEYWORD,
    IN_INTERP,
    IN_START,
    IN_START_INTERP,            /* must be IN_START + 1 */
};

QEMU_BUILD_BUG_ON(JSON_ERROR != 0);
QEMU_BUILD_BUG_ON(IN_RECOVERY != JSON_ERROR + 1);
QEMU_BUILD_BUG_ON((int)JSON_MIN <= (int)IN_START_INTERP);
QEMU_BUILD_BUG_ON(JSON_MAX >= 0x80);
QEMU_BUILD_BUG_ON(IN_START_INTERP != IN_START + 1);

#define LOOKAHEAD 0x80
#define TERMINAL(state) [0 ... 0xFF] = ((state) | LOOKAHEAD)

static const uint8_t json_lexer[][256] =  {

    [IN_RECOVERY] = {
        [0 ... 0x1F] = IN_START | LOOKAHEAD,
        [0x20 ... 0xFD] = IN_RECOVERY,
        [0xFE ... 0xFF] = IN_START | LOOKAHEAD,
        ['\t'] = IN_RECOVERY,
        ['['] = IN_START | LOOKAHEAD,
        [']'] = IN_START | LOOKAHEAD,
        ['{'] = IN_START | LOOKAHEAD,
        ['}'] = IN_START | LOOKAHEAD,
        [':'] = IN_START | LOOKAHEAD,
        [','] = IN_START | LOOKAHEAD,
    },

    [IN_DQ_STRING_ESCAPE] = {
        [0x20 ... 0xFD] = IN_DQ_STRING,
    },
    [IN_DQ_STRING] = {
        [0x20 ... 0xFD] = IN_DQ_STRING,
        ['\\'] = IN_DQ_STRING_ESCAPE,
        ['"'] = JSON_STRING,
    },

    [IN_SQ_STRING_ESCAPE] = {
        [0x20 ... 0xFD] = IN_SQ_STRING,
    },
    [IN_SQ_STRING] = {
        [0x20 ... 0xFD] = IN_SQ_STRING,
        ['\\'] = IN_SQ_STRING_ESCAPE,
        ['\''] = JSON_STRING,
    },

    [IN_ZERO] = {
        TERMINAL(JSON_INTEGER),
        ['0' ... '9'] = JSON_ERROR,
        ['.'] = IN_MANTISSA,
    },

    [IN_EXP_DIGITS] = {
        TERMINAL(JSON_FLOAT),
        ['0' ... '9'] = IN_EXP_DIGITS,
    },

    [IN_EXP_SIGN] = {
        ['0' ... '9'] = IN_EXP_DIGITS,
    },

    [IN_EXP_E] = {
        ['-'] = IN_EXP_SIGN,
        ['+'] = IN_EXP_SIGN,
        ['0' ... '9'] = IN_EXP_DIGITS,
    },

    [IN_MANTISSA_DIGITS] = {
        TERMINAL(JSON_FLOAT),
        ['0' ... '9'] = IN_MANTISSA_DIGITS,
        ['e'] = IN_EXP_E,
        ['E'] = IN_EXP_E,
    },

    [IN_MANTISSA] = {
        ['0' ... '9'] = IN_MANTISSA_DIGITS,
    },

    [IN_DIGITS] = {
        TERMINAL(JSON_INTEGER),
        ['0' ... '9'] = IN_DIGITS,
        ['e'] = IN_EXP_E,
        ['E'] = IN_EXP_E,
        ['.'] = IN_MANTISSA,
    },

    [IN_SIGN] = {
        ['0'] = IN_ZERO,
        ['1' ... '9'] = IN_DIGITS,
    },

    [IN_KEYWORD] = {
        TERMINAL(JSON_KEYWORD),
        ['a' ... 'z'] = IN_KEYWORD,
    },

    [IN_INTERP] = {
        TERMINAL(JSON_INTERP),
        ['A' ... 'Z'] = IN_INTERP,
        ['a' ... 'z'] = IN_INTERP,
        ['0' ... '9'] = IN_INTERP,
    },

    [IN_START ... IN_START_INTERP] = {
        ['"'] = IN_DQ_STRING,
        ['\''] = IN_SQ_STRING,
        ['0'] = IN_ZERO,
        ['1' ... '9'] = IN_DIGITS,
        ['-'] = IN_SIGN,
        ['{'] = JSON_LCURLY,
        ['}'] = JSON_RCURLY,
        ['['] = JSON_LSQUARE,
        [']'] = JSON_RSQUARE,
        [','] = JSON_COMMA,
        [':'] = JSON_COLON,
        ['a' ... 'z'] = IN_KEYWORD,
        [' '] = IN_START,
        ['\t'] = IN_START,
        ['\r'] = IN_START,
        ['\n'] = IN_START,
    },
    [IN_START_INTERP]['%'] = IN_INTERP,
};

static inline uint8_t next_state(JSONLexer *lexer, char ch, bool flush,
                                 bool *char_consumed)
{
    uint8_t next;

    assert(lexer->state < ARRAY_SIZE(json_lexer));
    next = json_lexer[lexer->state][(uint8_t)ch];
    *char_consumed = !flush && !(next & LOOKAHEAD);
    return next & ~LOOKAHEAD;
}

void json_lexer_init(JSONLexer *lexer, bool enable_interpolation)
{
    lexer->start_state = lexer->state = enable_interpolation
        ? IN_START_INTERP : IN_START;
    lexer->token = g_string_sized_new(3);
    lexer->x = lexer->y = 0;
}

static void json_lexer_feed_char(JSONLexer *lexer, char ch, bool flush)
{
    int new_state;
    bool char_consumed = false;

    lexer->x++;
    if (ch == '\n') {
        lexer->x = 0;
        lexer->y++;
    }

    while (flush ? lexer->state != lexer->start_state : !char_consumed) {
        new_state = next_state(lexer, ch, flush, &char_consumed);
        if (char_consumed) {
            assert(!flush);
            g_string_append_c(lexer->token, ch);
        }

        switch (new_state) {
        case JSON_LCURLY:
        case JSON_RCURLY:
        case JSON_LSQUARE:
        case JSON_RSQUARE:
        case JSON_COLON:
        case JSON_COMMA:
        case JSON_INTERP:
        case JSON_INTEGER:
        case JSON_FLOAT:
        case JSON_KEYWORD:
        case JSON_STRING:
            json_message_process_token(lexer, lexer->token, new_state,
                                       lexer->x, lexer->y);
        case IN_START:
            g_string_truncate(lexer->token, 0);
            new_state = lexer->start_state;
            break;
        case JSON_ERROR:
            json_message_process_token(lexer, lexer->token, JSON_ERROR,
                                       lexer->x, lexer->y);
            new_state = IN_RECOVERY;
        case IN_RECOVERY:
            g_string_truncate(lexer->token, 0);
            break;
        default:
            break;
        }
        lexer->state = new_state;
    }

    if (lexer->token->len > MAX_TOKEN_SIZE) {
        json_message_process_token(lexer, lexer->token, lexer->state,
                                   lexer->x, lexer->y);
        g_string_truncate(lexer->token, 0);
        lexer->state = lexer->start_state;
    }
}

void json_lexer_feed(JSONLexer *lexer, const char *buffer, size_t size)
{
    size_t i;

    for (i = 0; i < size; i++) {
        json_lexer_feed_char(lexer, buffer[i], false);
    }
}

void json_lexer_flush(JSONLexer *lexer)
{
    json_lexer_feed_char(lexer, 0, true);
    assert(lexer->state == lexer->start_state);
    json_message_process_token(lexer, lexer->token, JSON_END_OF_INPUT,
                               lexer->x, lexer->y);
}

void json_lexer_destroy(JSONLexer *lexer)
{
    g_string_free(lexer->token, true);
}
