
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "priv.h"

void lit_bytelist_init(LitAstByteList* bl)
{
    bl->values = NULL;
    bl->capacity = 0;
    bl->count = 0;
}

void lit_bytelist_destroy(LitState* state, LitAstByteList* bl)
{
    LIT_FREE_ARRAY(state, sizeof(uint8_t), bl->values, bl->capacity);
    lit_bytelist_init(bl);
}

void lit_bytelist_push(LitState* state, LitAstByteList* bl, uint8_t value)
{
    size_t oldcap;
    if(bl->capacity < bl->count + 1)
    {
        oldcap = bl->capacity;
        bl->capacity = LIT_GROW_CAPACITY(oldcap);
        bl->values = LIT_GROW_ARRAY(state, bl->values, sizeof(uint8_t), oldcap, bl->capacity);
    }
    bl->values[bl->count] = value;
    bl->count++;
}

static inline bool lit_lexutil_isdigit(char c)
{
    return c >= '0' && c <= '9';
}

static inline bool lit_lexutil_isalpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

void lit_astlex_init(LitState* state, LitAstScanner* scanner, const char* file_name, const char* source)
{
    scanner->line = 1;
    scanner->start = source;
    scanner->current = source;
    scanner->file_name = file_name;
    scanner->state = state;
    scanner->num_braces = 0;
    scanner->had_error = false;
}

static LitAstToken lit_astlex_maketoken(LitAstScanner* scanner, LitAstTokType type)
{
    LitAstToken token;
    token.type = type;
    token.start = scanner->start;
    token.length = (size_t)(scanner->current - scanner->start);
    token.line = scanner->line;
    return token;
}

static LitAstToken lit_astlex_makeerrortoken(LitAstScanner* scanner, const char* fmt, ...)
{
    va_list args;
    LitAstToken token;
    LitString* result;
    scanner->had_error = true;
    va_start(args, fmt);
    result = lit_vformat_error(scanner->state, scanner->line, fmt, args);
    va_end(args);
    token.type = LITTOK_ERROR;
    token.start = result->chars;
    token.length = lit_string_getlength(result);
    token.line = scanner->line;
    return token;
}

static bool lit_astlex_isatend(LitAstScanner* scanner)
{
    return *scanner->current == '\0';
}

static char lit_astlex_advance(LitAstScanner* scanner)
{
    scanner->current++;
    return scanner->current[-1];
}

static bool lit_astlex_matchchar(LitAstScanner* scanner, char expected)
{
    if(lit_astlex_isatend(scanner))
    {
        return false;
    }

    if(*scanner->current != expected)
    {
        return false;
    }
    scanner->current++;
    return true;
}

static LitAstToken lit_astlex_matchtoken(LitAstScanner* scanner, char c, LitAstTokType a, LitAstTokType b)
{
    return lit_astlex_maketoken(scanner, lit_astlex_matchchar(scanner, c) ? a : b);
}

static LitAstToken lit_astlex_matchntoken(LitAstScanner* scanner, char cr, char cb, LitAstTokType a, LitAstTokType b, LitAstTokType c)
{
    //return lit_astlex_maketoken(scanner, lit_astlex_matchchar(scanner, cr) ? a : (lit_astlex_matchchar(scanner, cb) ? b : c));
    if(lit_astlex_matchchar(scanner, cr))
    {
        return lit_astlex_maketoken(scanner, a);
    }
    if(lit_astlex_matchchar(scanner, cb))
    {
        return lit_astlex_maketoken(scanner, b);
    }
    return lit_astlex_maketoken(scanner, c);
}

static char lit_astlex_peekcurrent(LitAstScanner* scanner)
{
    return *scanner->current;
}

static char lit_astlex_peeknext(LitAstScanner* scanner)
{
    if(lit_astlex_isatend(scanner))
    {
        return '\0';
    }
    return scanner->current[1];
}

static bool lit_astlex_skipspace(LitAstScanner* scanner)
{
    char a;
    char b;
    char c;
    (void)a;
    (void)b;
    while(true)
    {
        c = lit_astlex_peekcurrent(scanner);
        switch(c)
        {
            case ' ':
            case '\r':
            case '\t':
                {
                    lit_astlex_advance(scanner);
                }
                break;
            case '\n':
                {
                    scanner->start = scanner->current;
                    lit_astlex_advance(scanner);
                    return true;
                }
                break;
            case '/':
                {
                    if(lit_astlex_peeknext(scanner) == '/')
                    {
                        while(lit_astlex_peekcurrent(scanner) != '\n' && !lit_astlex_isatend(scanner))
                        {
                            lit_astlex_advance(scanner);
                        }
                        return lit_astlex_skipspace(scanner);
                    }
                    else if(lit_astlex_peeknext(scanner) == '*')
                    {
                        lit_astlex_advance(scanner);
                        lit_astlex_advance(scanner);
                        a = lit_astlex_peekcurrent(scanner);
                        b = lit_astlex_peeknext(scanner);
                        while((lit_astlex_peekcurrent(scanner) != '*' || lit_astlex_peeknext(scanner) != '/') && !lit_astlex_isatend(scanner))
                        {
                            if(lit_astlex_peekcurrent(scanner) == '\n')
                            {
                                scanner->line++;
                            }
                            lit_astlex_advance(scanner);
                        }
                        lit_astlex_advance(scanner);
                        lit_astlex_advance(scanner);
                        return lit_astlex_skipspace(scanner);
                    }
                    return false;
                }
                break;
            default:
                {
                    return false;
                }
        }
    }
}

static LitAstToken lit_astlex_scanstring(LitAstScanner* scanner, bool interpolation)
{
    char c;
    LitState* state;
    LitAstByteList bytes;
    LitAstToken token;
    LitAstTokType stringtype;
    state = scanner->state;
    stringtype = LITTOK_STRING;
    lit_bytelist_init(&bytes);
    while(true)
    {
        c = lit_astlex_advance(scanner);
        if(c == '\"')
        {
            break;
        }
        else if(interpolation && c == '{')
        {
            if(scanner->num_braces >= LIT_MAX_INTERPOLATION_NESTING)
            {
                return lit_astlex_makeerrortoken(scanner, "interpolation nesting is too deep, maximum is %i", LIT_MAX_INTERPOLATION_NESTING);
            }
            stringtype = LITTOK_INTERPOLATION;
            scanner->braces[scanner->num_braces++] = 1;
            break;
        }
        switch(c)
        {
            case '\0':
                {
                    return lit_astlex_makeerrortoken(scanner, "unterminated string");
                }
                break;
            case '\n':
                {
                    scanner->line++;
                    lit_bytelist_push(state, &bytes, c);
                }
                break;
            case '\\':
                {
                    switch(lit_astlex_advance(scanner))
                    {
                        case '\"':
                            {
                                lit_bytelist_push(state, &bytes, '\"');
                            }
                            break;
                        case '\\':
                            {
                                lit_bytelist_push(state, &bytes, '\\');
                            }
                            break;
                        case '0':
                            {
                                lit_bytelist_push(state, &bytes, '\0');
                            }
                            break;
                        case '{':
                            {
                                lit_bytelist_push(state, &bytes, '{');
                            }
                            break;
                        case 'a':
                            {
                                lit_bytelist_push(state, &bytes, '\a');
                            }
                            break;
                        case 'b':
                            {
                                lit_bytelist_push(state, &bytes, '\b');
                            }
                            break;
                        case 'f':
                            {
                                lit_bytelist_push(state, &bytes, '\f');
                            }
                            break;
                        case 'n':
                            {
                                lit_bytelist_push(state, &bytes, '\n');
                            }
                            break;
                        case 'r':
                            {
                                lit_bytelist_push(state, &bytes, '\r');
                            }
                            break;
                        case 't':
                            {
                                lit_bytelist_push(state, &bytes, '\t');
                            }
                            break;
                        case 'v':
                            {
                                lit_bytelist_push(state, &bytes, '\v');
                            }
                            break;
                        default:
                            {
                                return lit_astlex_makeerrortoken(scanner, "invalid escape character '%c'", scanner->current[-1]);
                            }
                            break;
                    }
                }
                break;
            default:
                {
                    lit_bytelist_push(state, &bytes, c);
                }
                break;
        }
    }
    token = lit_astlex_maketoken(scanner, stringtype);
    token.value = lit_value_fromobject(lit_string_copy(state, (const char*)bytes.values, bytes.count));
    lit_bytelist_destroy(state, &bytes);
    return token;
}

static int lit_astlex_scanhexdigit(LitAstScanner* scanner)
{
    char c;
    c = lit_astlex_advance(scanner);
    if((c >= '0') && (c <= '9'))
    {
        return (c - '0');
    }
    if((c >= 'a') && (c <= 'f'))
    {
        return (c - 'a' + 10);
    }
    if((c >= 'A') && (c <= 'F'))
    {
        return (c - 'A' + 10);
    }
    scanner->current--;
    return -1;
}

static int lit_astlex_scanbinarydigit(LitAstScanner* scanner)
{
    char c;
    c = lit_astlex_advance(scanner);
    if(c >= '0' && c <= '1')
    {
        return c - '0';
    }
    scanner->current--;
    return -1;
}

static LitAstToken lit_astlex_makenumbertoken(LitAstScanner* scanner, bool ishex, bool isbinary)
{
    int64_t itmp;
    double tmp;
    LitAstToken token;
    LitValue value;
    errno = 0;
    if(ishex)
    {
        value = lit_value_makefixednumber(scanner->state, (double)strtoll(scanner->start, NULL, 16));
    }
    else if(isbinary)
    {
        value = lit_value_makefixednumber(scanner->state, (int)strtoll(scanner->start + 2, NULL, 2));
    }
    else
    {
        tmp = strtod(scanner->start, NULL);
        itmp = (int64_t)tmp;
        if(itmp == tmp)
        {
            value = lit_value_makefixednumber(scanner->state, tmp);
        }
        else
        {
            value = lit_value_makefloatnumber(scanner->state, tmp);
        }
    }

    if(errno == ERANGE)
    {
        errno = 0;
        return lit_astlex_makeerrortoken(scanner, "number is too big to be represented by a single literal");
    }
    token = lit_astlex_maketoken(scanner, LITTOK_NUMBER);
    token.value = value;
    return token;
}

static LitAstToken lit_astlex_scannumber(LitAstScanner* scanner)
{
    if(lit_astlex_matchchar(scanner, 'x'))
    {
        while(lit_astlex_scanhexdigit(scanner) != -1)
        {
            continue;
        }
        return lit_astlex_makenumbertoken(scanner, true, false);
    }
    if(lit_astlex_matchchar(scanner, 'b'))
    {
        while(lit_astlex_scanbinarydigit(scanner) != -1)
        {
            continue;
        }
        return lit_astlex_makenumbertoken(scanner, false, true);
    }
    while(lit_lexutil_isdigit(lit_astlex_peekcurrent(scanner)))
    {
        lit_astlex_advance(scanner);
    }
    // Look for a fractional part.
    if(lit_astlex_peekcurrent(scanner) == '.' && lit_lexutil_isdigit(lit_astlex_peeknext(scanner)))
    {
        // Consume the '.'
        lit_astlex_advance(scanner);
        while(lit_lexutil_isdigit(lit_astlex_peekcurrent(scanner)))
        {
            lit_astlex_advance(scanner);
        }
    }
    return lit_astlex_makenumbertoken(scanner, false, false);
}

static LitAstTokType lit_astlex_checkkeyword(LitAstScanner* scanner, int start, int length, const char* rest, LitAstTokType type)
{
    if(scanner->current - scanner->start == start + length && memcmp(scanner->start + start, rest, length) == 0)
    {
        return type;
    }
    return LITTOK_IDENTIFIER;
}

static LitAstTokType lit_astlex_scanidenttype(LitAstScanner* scanner)
{
    switch(scanner->start[0])
    {
        case 'b':
            return lit_astlex_checkkeyword(scanner, 1, 4, "reak", LITTOK_BREAK);

        case 'c':
            {
                if(scanner->current - scanner->start > 1)
                {
                    switch(scanner->start[1])
                    {
                        case 'l':
                            return lit_astlex_checkkeyword(scanner, 2, 3, "ass", LITTOK_CLASS);
                        case 'o':
                        {
                            if(scanner->current - scanner->start > 3)
                            {
                                switch(scanner->start[3])
                                {
                                    case 's':
                                        return lit_astlex_checkkeyword(scanner, 2, 3, "nst", LITTOK_CONST);
                                    case 't':
                                        return lit_astlex_checkkeyword(scanner, 2, 6, "ntinue", LITTOK_CONTINUE);
                                }
                            }
                        }
                    }
                }
            }
            break;
        case 'e':
            {
                if(scanner->current - scanner->start > 1)
                {
                    switch(scanner->start[1])
                    {
                        case 'l':
                            return lit_astlex_checkkeyword(scanner, 2, 2, "se", LITTOK_ELSE);
                        case 'x':
                            return lit_astlex_checkkeyword(scanner, 2, 4, "port", LITTOK_EXPORT);
                    }
                }
            }
            break;
        case 'f':
            {
                if(scanner->current - scanner->start > 1)
                {
                    switch(scanner->start[1])
                    {
                        case 'a':
                            return lit_astlex_checkkeyword(scanner, 2, 3, "lse", LITTOK_FALSE);
                        case 'o':
                            return lit_astlex_checkkeyword(scanner, 2, 1, "r", LITTOK_FOR);
                        case 'u':
                            return lit_astlex_checkkeyword(scanner, 2, 6, "nction", LITTOK_FUNCTION);
                    }
                }
            }
            break;
        case 'i':
            {
                if(scanner->current - scanner->start > 1)
                {
                    switch(scanner->start[1])
                    {
                        case 's':
                            return lit_astlex_checkkeyword(scanner, 2, 0, "", LITTOK_IS);
                        case 'f':
                            return lit_astlex_checkkeyword(scanner, 2, 0, "", LITTOK_IF);
                        case 'n':
                            return lit_astlex_checkkeyword(scanner, 2, 0, "", LITTOK_IN);
                    }
                }
            }
            break;
        case 'n':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'u':
                        return lit_astlex_checkkeyword(scanner, 2, 2, "ll", LITTOK_NULL);
                    case 'e':
                        return lit_astlex_checkkeyword(scanner, 2, 1, "w", LITTOK_NEW);
                }
            }

            break;
        }

        case 'r':
        {
            if(scanner->current - scanner->start > 2)
            {
                switch(scanner->start[2])
                {
                    case 'f':
                        return lit_astlex_checkkeyword(scanner, 3, 0, "", LITTOK_REF);
                    case 't':
                        return lit_astlex_checkkeyword(scanner, 3, 3, "urn", LITTOK_RETURN);
                }
            }

            break;
        }

        case 'o':
            return lit_astlex_checkkeyword(scanner, 1, 7, "perator", LITTOK_OPERATOR);

        case 's':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'u':
                        return lit_astlex_checkkeyword(scanner, 2, 3, "per", LITTOK_SUPER);
                    case 't':
                        return lit_astlex_checkkeyword(scanner, 2, 4, "atic", LITTOK_STATIC);
                    case 'e':
                        return lit_astlex_checkkeyword(scanner, 2, 1, "t", LITTOK_SET);
                }
            }

            break;
        }

        case 't':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'h':
                        return lit_astlex_checkkeyword(scanner, 2, 2, "is", LITTOK_THIS);
                    case 'r':
                        return lit_astlex_checkkeyword(scanner, 2, 2, "ue", LITTOK_TRUE);
                }
            }

            break;
        }

        case 'v':
            return lit_astlex_checkkeyword(scanner, 1, 2, "ar", LITTOK_VAR);
        case 'w':
            return lit_astlex_checkkeyword(scanner, 1, 4, "hile", LITTOK_WHILE);
        case 'g':
            return lit_astlex_checkkeyword(scanner, 1, 2, "et", LITTOK_GET);
    }

    return LITTOK_IDENTIFIER;
}

static LitAstToken lit_astlex_scanidentifier(LitAstScanner* scanner)
{
    while(lit_lexutil_isalpha(lit_astlex_peekcurrent(scanner)) || lit_lexutil_isdigit(lit_astlex_peekcurrent(scanner)))
    {
        lit_astlex_advance(scanner);
    }
    return lit_astlex_maketoken(scanner, lit_astlex_scanidenttype(scanner));
}

LitAstToken lit_astlex_scantoken(LitAstScanner* scanner)
{
    char c;
    LitAstToken token;
    if(lit_astlex_skipspace(scanner))
    {
        token = lit_astlex_maketoken(scanner, LITTOK_NEW_LINE);
        scanner->line++;
        return token;
    }
    scanner->start = scanner->current;
    if(lit_astlex_isatend(scanner))
    {
        return lit_astlex_maketoken(scanner, LITTOK_EOF);
    }
    c = lit_astlex_advance(scanner);
    if(lit_lexutil_isdigit(c))
    {
        return lit_astlex_scannumber(scanner);
    }
    if(lit_lexutil_isalpha(c))
    {
        return lit_astlex_scanidentifier(scanner);
    }
    switch(c)
    {
        case '(':
            return lit_astlex_maketoken(scanner, LITTOK_LEFT_PAREN);
        case ')':
            return lit_astlex_maketoken(scanner, LITTOK_RIGHT_PAREN);
        case '{':
            {
                if(scanner->num_braces > 0)
                {
                    scanner->braces[scanner->num_braces - 1]++;
                }
                return lit_astlex_maketoken(scanner, LITTOK_LEFT_BRACE);
            }
            break;
        case '}':
            {
                if(scanner->num_braces > 0 && --scanner->braces[scanner->num_braces - 1] == 0)
                {
                    scanner->num_braces--;
                    return lit_astlex_scanstring(scanner, true);
                }
                return lit_astlex_maketoken(scanner, LITTOK_RIGHT_BRACE);
            }
            break;
        case '[':
            return lit_astlex_maketoken(scanner, LITTOK_LEFT_BRACKET);
        case ']':
            return lit_astlex_maketoken(scanner, LITTOK_RIGHT_BRACKET);
        case ';':
            return lit_astlex_maketoken(scanner, LITTOK_SEMICOLON);
        case ',':
            return lit_astlex_maketoken(scanner, LITTOK_COMMA);
        case ':':
            return lit_astlex_maketoken(scanner, LITTOK_COLON);
        case '~':
            return lit_astlex_maketoken(scanner, LITTOK_TILDE);
        case '+':
            return lit_astlex_matchntoken(scanner, '=', '+', LITTOK_PLUS_EQUAL, LITTOK_PLUS_PLUS, LITTOK_PLUS);
        case '-':
            {
                if(lit_astlex_matchchar(scanner, '>'))
                {
                    return lit_astlex_maketoken(scanner, LITTOK_SMALL_ARROW);
                }
                return lit_astlex_matchntoken(scanner, '=', '-', LITTOK_MINUS_EQUAL, LITTOK_MINUS_MINUS, LITTOK_MINUS);
            }
        case '/':
            return lit_astlex_matchtoken(scanner, '=', LITTOK_SLASH_EQUAL, LITTOK_SLASH);
        case '#':
            return lit_astlex_matchtoken(scanner, '=', LITTOK_SHARP_EQUAL, LITTOK_SHARP);
        case '!':
            return lit_astlex_matchtoken(scanner, '=', LITTOK_BANG_EQUAL, LITTOK_BANG);
        case '?':
            return lit_astlex_matchtoken(scanner, '?', LITTOK_QUESTION_QUESTION, LITTOK_QUESTION);
        case '%':
            return lit_astlex_matchtoken(scanner, '=', LITTOK_PERCENT_EQUAL, LITTOK_PERCENT);
        case '^':
            return lit_astlex_matchtoken(scanner, '=', LITTOK_CARET_EQUAL, LITTOK_CARET);

        case '>':
            return lit_astlex_matchntoken(scanner, '=', '>', LITTOK_GREATER_EQUAL, LITTOK_GREATER_GREATER, LITTOK_GREATER);
        case '<':
            return lit_astlex_matchntoken(scanner, '=', '<', LITTOK_LESS_EQUAL, LITTOK_LESS_LESS, LITTOK_LESS);
        case '*':
            return lit_astlex_matchntoken(scanner, '=', '*', LITTOK_STAR_EQUAL, LITTOK_STAR_STAR, LITTOK_STAR);
        case '=':
            return lit_astlex_matchntoken(scanner, '=', '>', LITTOK_EQUAL_EQUAL, LITTOK_ARROW, LITTOK_EQUAL);
        case '|':
            return lit_astlex_matchntoken(scanner, '=', '|', LITTOK_BAR_EQUAL, LITTOK_BAR_BAR, LITTOK_BAR);
        case '&':
            {
                return lit_astlex_matchntoken(scanner, '=', '&', LITTOK_AMPERSAND_EQUAL, LITTOK_AMPERSAND_AMPERSAND, LITTOK_AMPERSAND);
            }
            break;
        case '.':
            {
                if(!lit_astlex_matchchar(scanner, '.'))
                {
                    return lit_astlex_maketoken(scanner, LITTOK_DOT);
                }
                return lit_astlex_matchtoken(scanner, '.', LITTOK_DOT_DOT_DOT, LITTOK_DOT_DOT);
            }
            break;
        case '$':
            {
                if(!lit_astlex_matchchar(scanner, '\"'))
                {
                    return lit_astlex_makeerrortoken(scanner, "expected '%c' after '%c', got '%c'", '\"', '$', lit_astlex_peekcurrent(scanner));
                }
                return lit_astlex_scanstring(scanner, true);
            }
        case '"':
            return lit_astlex_scanstring(scanner, false);
    }
    return lit_astlex_makeerrortoken(scanner, "unexpected character '%c'", c);
}
