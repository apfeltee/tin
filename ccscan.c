
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "priv.h"

void tin_bytelist_init(TinAstByteList* bl)
{
    bl->values = NULL;
    bl->capacity = 0;
    bl->count = 0;
}

void tin_bytelist_destroy(TinState* state, TinAstByteList* bl)
{
    TIN_FREE_ARRAY(state, sizeof(uint8_t), bl->values, bl->capacity);
    tin_bytelist_init(bl);
}

void tin_bytelist_push(TinState* state, TinAstByteList* bl, uint8_t value)
{
    size_t oldcap;
    if(bl->capacity < bl->count + 1)
    {
        oldcap = bl->capacity;
        bl->capacity = TIN_GROW_CAPACITY(oldcap);
        bl->values = TIN_GROW_ARRAY(state, bl->values, sizeof(uint8_t), oldcap, bl->capacity);
    }
    bl->values[bl->count] = value;
    bl->count++;
}

static inline bool tin_lexutil_isdigit(char c)
{
    return c >= '0' && c <= '9';
}

static inline bool tin_lexutil_isalpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

void tin_astlex_init(TinState* state, TinAstScanner* scanner, const char* filename, const char* source)
{
    scanner->line = 1;
    scanner->start = source;
    scanner->current = source;
    scanner->filename = filename;
    scanner->state = state;
    scanner->numbraces = 0;
    scanner->haderror = false;
}

static TinAstToken tin_astlex_maketoken(TinAstScanner* scanner, TinAstTokType type)
{
    TinAstToken token;
    token.type = type;
    token.start = scanner->start;
    token.length = (size_t)(scanner->current - scanner->start);
    token.line = scanner->line;
    return token;
}

static TinAstToken tin_astlex_makeerrortoken(TinAstScanner* scanner, const char* fmt, ...)
{
    va_list args;
    TinAstToken token;
    TinString* result;
    scanner->haderror = true;
    va_start(args, fmt);
    result = tin_vformat_error(scanner->state, scanner->line, fmt, args);
    va_end(args);
    token.type = TINTOK_ERROR;
    token.start = result->chars;
    token.length = tin_string_getlength(result);
    token.line = scanner->line;
    return token;
}

static bool tin_astlex_isatend(TinAstScanner* scanner)
{
    return *scanner->current == '\0';
}

static char tin_astlex_advance(TinAstScanner* scanner)
{
    scanner->current++;
    return scanner->current[-1];
}

static bool tin_astlex_matchchar(TinAstScanner* scanner, char expected)
{
    if(tin_astlex_isatend(scanner))
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

static TinAstToken tin_astlex_matchtoken(TinAstScanner* scanner, char c, TinAstTokType a, TinAstTokType b)
{
    return tin_astlex_maketoken(scanner, tin_astlex_matchchar(scanner, c) ? a : b);
}

static TinAstToken tin_astlex_matchntoken(TinAstScanner* scanner, char cr, char cb, TinAstTokType a, TinAstTokType b, TinAstTokType c)
{
    //return tin_astlex_maketoken(scanner, tin_astlex_matchchar(scanner, cr) ? a : (tin_astlex_matchchar(scanner, cb) ? b : c));
    if(tin_astlex_matchchar(scanner, cr))
    {
        return tin_astlex_maketoken(scanner, a);
    }
    if(tin_astlex_matchchar(scanner, cb))
    {
        return tin_astlex_maketoken(scanner, b);
    }
    return tin_astlex_maketoken(scanner, c);
}

static char tin_astlex_peekcurrent(TinAstScanner* scanner)
{
    return *scanner->current;
}

static char tin_astlex_peeknext(TinAstScanner* scanner)
{
    if(tin_astlex_isatend(scanner))
    {
        return '\0';
    }
    return scanner->current[1];
}

static bool tin_astlex_skipspace(TinAstScanner* scanner)
{
    char a;
    char b;
    char c;
    (void)a;
    (void)b;
    while(true)
    {
        c = tin_astlex_peekcurrent(scanner);
        switch(c)
        {
            case ' ':
            case '\r':
            case '\t':
                {
                    tin_astlex_advance(scanner);
                }
                break;
            case '\n':
                {
                    scanner->start = scanner->current;
                    tin_astlex_advance(scanner);
                    return true;
                }
                break;
            case '/':
                {
                    if(tin_astlex_peeknext(scanner) == '/')
                    {
                        while(tin_astlex_peekcurrent(scanner) != '\n' && !tin_astlex_isatend(scanner))
                        {
                            tin_astlex_advance(scanner);
                        }
                        return tin_astlex_skipspace(scanner);
                    }
                    else if(tin_astlex_peeknext(scanner) == '*')
                    {
                        tin_astlex_advance(scanner);
                        tin_astlex_advance(scanner);
                        a = tin_astlex_peekcurrent(scanner);
                        b = tin_astlex_peeknext(scanner);
                        while((tin_astlex_peekcurrent(scanner) != '*' || tin_astlex_peeknext(scanner) != '/') && !tin_astlex_isatend(scanner))
                        {
                            if(tin_astlex_peekcurrent(scanner) == '\n')
                            {
                                scanner->line++;
                            }
                            tin_astlex_advance(scanner);
                        }
                        tin_astlex_advance(scanner);
                        tin_astlex_advance(scanner);
                        return tin_astlex_skipspace(scanner);
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

static TinAstToken tin_astlex_scanstring(TinAstScanner* scanner, bool interpolation)
{
    char c;
    TinState* state;
    TinAstByteList bytes;
    TinAstToken token;
    TinAstTokType stringtype;
    state = scanner->state;
    stringtype = TINTOK_STRING;
    tin_bytelist_init(&bytes);
    while(true)
    {
        c = tin_astlex_advance(scanner);
        if(c == '\"')
        {
            break;
        }
        else if(interpolation && c == '{')
        {
            if(scanner->numbraces >= TIN_MAX_INTERPOLATION_NESTING)
            {
                return tin_astlex_makeerrortoken(scanner, "interpolation nesting is too deep, maximum is %i", TIN_MAX_INTERPOLATION_NESTING);
            }
            stringtype = TINTOK_INTERPOLATION;
            scanner->braces[scanner->numbraces++] = 1;
            break;
        }
        switch(c)
        {
            case '\0':
                {
                    return tin_astlex_makeerrortoken(scanner, "unterminated string");
                }
                break;
            case '\n':
                {
                    scanner->line++;
                    tin_bytelist_push(state, &bytes, c);
                }
                break;
            case '\\':
                {
                    switch(tin_astlex_advance(scanner))
                    {
                        case '\"':
                            {
                                tin_bytelist_push(state, &bytes, '\"');
                            }
                            break;
                        case '\\':
                            {
                                tin_bytelist_push(state, &bytes, '\\');
                            }
                            break;
                        case '0':
                            {
                                tin_bytelist_push(state, &bytes, '\0');
                            }
                            break;
                        case '{':
                            {
                                tin_bytelist_push(state, &bytes, '{');
                            }
                            break;
                        case 'a':
                            {
                                tin_bytelist_push(state, &bytes, '\a');
                            }
                            break;
                        case 'b':
                            {
                                tin_bytelist_push(state, &bytes, '\b');
                            }
                            break;
                        case 'f':
                            {
                                tin_bytelist_push(state, &bytes, '\f');
                            }
                            break;
                        case 'n':
                            {
                                tin_bytelist_push(state, &bytes, '\n');
                            }
                            break;
                        case 'r':
                            {
                                tin_bytelist_push(state, &bytes, '\r');
                            }
                            break;
                        case 't':
                            {
                                tin_bytelist_push(state, &bytes, '\t');
                            }
                            break;
                        case 'v':
                            {
                                tin_bytelist_push(state, &bytes, '\v');
                            }
                            break;
                        default:
                            {
                                return tin_astlex_makeerrortoken(scanner, "invalid escape character '%c'", scanner->current[-1]);
                            }
                            break;
                    }
                }
                break;
            default:
                {
                    tin_bytelist_push(state, &bytes, c);
                }
                break;
        }
    }
    token = tin_astlex_maketoken(scanner, stringtype);
    token.value = tin_value_fromobject(tin_string_copy(state, (const char*)bytes.values, bytes.count));
    tin_bytelist_destroy(state, &bytes);
    return token;
}

static int tin_astlex_scanhexdigit(TinAstScanner* scanner)
{
    char c;
    c = tin_astlex_advance(scanner);
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

static int tin_astlex_scanbinarydigit(TinAstScanner* scanner)
{
    char c;
    c = tin_astlex_advance(scanner);
    if(c >= '0' && c <= '1')
    {
        return c - '0';
    }
    scanner->current--;
    return -1;
}

static TinAstToken tin_astlex_makenumbertoken(TinAstScanner* scanner, bool ishex, bool isbinary)
{
    int64_t itmp;
    double tmp;
    TinAstToken token;
    TinValue value;
    errno = 0;
    if(ishex)
    {
        value = tin_value_makefixednumber(scanner->state, (double)strtoll(scanner->start, NULL, 16));
    }
    else if(isbinary)
    {
        value = tin_value_makefixednumber(scanner->state, (int)strtoll(scanner->start + 2, NULL, 2));
    }
    else
    {
        tmp = strtod(scanner->start, NULL);
        itmp = (int64_t)tmp;
        if(itmp == tmp)
        {
            value = tin_value_makefixednumber(scanner->state, tmp);
        }
        else
        {
            value = tin_value_makefloatnumber(scanner->state, tmp);
        }
    }

    if(errno == ERANGE)
    {
        errno = 0;
        return tin_astlex_makeerrortoken(scanner, "number is too big to be represented by a single literal");
    }
    token = tin_astlex_maketoken(scanner, TINTOK_NUMBER);
    token.value = value;
    return token;
}

static TinAstToken tin_astlex_scannumber(TinAstScanner* scanner)
{
    if(tin_astlex_matchchar(scanner, 'x'))
    {
        while(tin_astlex_scanhexdigit(scanner) != -1)
        {
            continue;
        }
        return tin_astlex_makenumbertoken(scanner, true, false);
    }
    if(tin_astlex_matchchar(scanner, 'b'))
    {
        while(tin_astlex_scanbinarydigit(scanner) != -1)
        {
            continue;
        }
        return tin_astlex_makenumbertoken(scanner, false, true);
    }
    while(tin_lexutil_isdigit(tin_astlex_peekcurrent(scanner)))
    {
        tin_astlex_advance(scanner);
    }
    // Look for a fractional part.
    if(tin_astlex_peekcurrent(scanner) == '.' && tin_lexutil_isdigit(tin_astlex_peeknext(scanner)))
    {
        // Consume the '.'
        tin_astlex_advance(scanner);
        while(tin_lexutil_isdigit(tin_astlex_peekcurrent(scanner)))
        {
            tin_astlex_advance(scanner);
        }
    }
    return tin_astlex_makenumbertoken(scanner, false, false);
}

static TinAstTokType tin_astlex_checkkeyword(TinAstScanner* scanner, int start, int length, const char* rest, TinAstTokType type)
{
    if(scanner->current - scanner->start == start + length && memcmp(scanner->start + start, rest, length) == 0)
    {
        return type;
    }
    return TINTOK_IDENTIFIER;
}

static TinAstTokType tin_astlex_scanidenttype(TinAstScanner* scanner)
{
    switch(scanner->start[0])
    {
        case 'b':
            return tin_astlex_checkkeyword(scanner, 1, 4, "reak", TINTOK_BREAK);

        case 'c':
            {
                if(scanner->current - scanner->start > 1)
                {
                    switch(scanner->start[1])
                    {
                        case 'l':
                            return tin_astlex_checkkeyword(scanner, 2, 3, "ass", TINTOK_CLASS);
                        case 'o':
                        {
                            if(scanner->current - scanner->start > 3)
                            {
                                switch(scanner->start[3])
                                {
                                    case 's':
                                        return tin_astlex_checkkeyword(scanner, 2, 3, "nst", TINTOK_CONST);
                                    case 't':
                                        return tin_astlex_checkkeyword(scanner, 2, 6, "ntinue", TINTOK_CONTINUE);
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
                            return tin_astlex_checkkeyword(scanner, 2, 2, "se", TINTOK_ELSE);
                        case 'x':
                            return tin_astlex_checkkeyword(scanner, 2, 4, "port", TINTOK_EXPORT);
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
                            return tin_astlex_checkkeyword(scanner, 2, 3, "lse", TINTOK_FALSE);
                        case 'o':
                            return tin_astlex_checkkeyword(scanner, 2, 1, "r", TINTOK_FOR);
                        case 'u':
                            return tin_astlex_checkkeyword(scanner, 2, 6, "nction", TINTOK_FUNCTION);
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
                            return tin_astlex_checkkeyword(scanner, 2, 0, "", TINTOK_IS);
                        case 'f':
                            return tin_astlex_checkkeyword(scanner, 2, 0, "", TINTOK_IF);
                        case 'n':
                            return tin_astlex_checkkeyword(scanner, 2, 0, "", TINTOK_IN);
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
                        return tin_astlex_checkkeyword(scanner, 2, 2, "ll", TINTOK_NULL);
                    case 'e':
                        return tin_astlex_checkkeyword(scanner, 2, 1, "w", TINTOK_NEW);
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
                        return tin_astlex_checkkeyword(scanner, 3, 0, "", TINTOK_REF);
                    case 't':
                        return tin_astlex_checkkeyword(scanner, 3, 3, "urn", TINTOK_RETURN);
                }
            }

            break;
        }

        case 'o':
            return tin_astlex_checkkeyword(scanner, 1, 7, "perator", TINTOK_OPERATOR);

        case 's':
        {
            if(scanner->current - scanner->start > 1)
            {
                switch(scanner->start[1])
                {
                    case 'u':
                        return tin_astlex_checkkeyword(scanner, 2, 3, "per", TINTOK_SUPER);
                    case 't':
                        return tin_astlex_checkkeyword(scanner, 2, 4, "atic", TINTOK_STATIC);
                    case 'e':
                        return tin_astlex_checkkeyword(scanner, 2, 1, "t", TINTOK_SET);
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
                        return tin_astlex_checkkeyword(scanner, 2, 2, "is", TINTOK_THIS);
                    case 'r':
                        return tin_astlex_checkkeyword(scanner, 2, 2, "ue", TINTOK_TRUE);
                }
            }

            break;
        }

        case 'v':
            return tin_astlex_checkkeyword(scanner, 1, 2, "ar", TINTOK_VAR);
        case 'w':
            return tin_astlex_checkkeyword(scanner, 1, 4, "hile", TINTOK_WHILE);
        case 'g':
            return tin_astlex_checkkeyword(scanner, 1, 2, "et", TINTOK_GET);
    }

    return TINTOK_IDENTIFIER;
}

static TinAstToken tin_astlex_scanidentifier(TinAstScanner* scanner)
{
    while(tin_lexutil_isalpha(tin_astlex_peekcurrent(scanner)) || tin_lexutil_isdigit(tin_astlex_peekcurrent(scanner)))
    {
        tin_astlex_advance(scanner);
    }
    return tin_astlex_maketoken(scanner, tin_astlex_scanidenttype(scanner));
}

TinAstToken tin_astlex_scantoken(TinAstScanner* scanner)
{
    char c;
    TinAstToken token;
    if(tin_astlex_skipspace(scanner))
    {
        token = tin_astlex_maketoken(scanner, TINTOK_NEW_LINE);
        scanner->line++;
        return token;
    }
    scanner->start = scanner->current;
    if(tin_astlex_isatend(scanner))
    {
        return tin_astlex_maketoken(scanner, TINTOK_EOF);
    }
    c = tin_astlex_advance(scanner);
    if(tin_lexutil_isdigit(c))
    {
        return tin_astlex_scannumber(scanner);
    }
    if(tin_lexutil_isalpha(c))
    {
        return tin_astlex_scanidentifier(scanner);
    }
    switch(c)
    {
        case '(':
            return tin_astlex_maketoken(scanner, TINTOK_LEFT_PAREN);
        case ')':
            return tin_astlex_maketoken(scanner, TINTOK_RIGHT_PAREN);
        case '{':
            {
                if(scanner->numbraces > 0)
                {
                    scanner->braces[scanner->numbraces - 1]++;
                }
                return tin_astlex_maketoken(scanner, TINTOK_LEFT_BRACE);
            }
            break;
        case '}':
            {
                if(scanner->numbraces > 0 && --scanner->braces[scanner->numbraces - 1] == 0)
                {
                    scanner->numbraces--;
                    return tin_astlex_scanstring(scanner, true);
                }
                return tin_astlex_maketoken(scanner, TINTOK_RIGHT_BRACE);
            }
            break;
        case '[':
            return tin_astlex_maketoken(scanner, TINTOK_LEFT_BRACKET);
        case ']':
            return tin_astlex_maketoken(scanner, TINTOK_RIGHT_BRACKET);
        case ';':
            return tin_astlex_maketoken(scanner, TINTOK_SEMICOLON);
        case ',':
            return tin_astlex_maketoken(scanner, TINTOK_COMMA);
        case ':':
            return tin_astlex_maketoken(scanner, TINTOK_COLON);
        case '~':
            return tin_astlex_maketoken(scanner, TINTOK_TILDE);
        case '+':
            return tin_astlex_matchntoken(scanner, '=', '+', TINTOK_PLUS_EQUAL, TINTOK_PLUS_PLUS, TINTOK_PLUS);
        case '-':
            {
                if(tin_astlex_matchchar(scanner, '>'))
                {
                    return tin_astlex_maketoken(scanner, TINTOK_SMALL_ARROW);
                }
                return tin_astlex_matchntoken(scanner, '=', '-', TINTOK_MINUS_EQUAL, TINTOK_MINUS_MINUS, TINTOK_MINUS);
            }
        case '/':
            return tin_astlex_matchtoken(scanner, '=', TINTOK_SLASH_EQUAL, TINTOK_SLASH);
        case '#':
            return tin_astlex_matchtoken(scanner, '=', TINTOK_SHARP_EQUAL, TINTOK_SHARP);
        case '!':
            return tin_astlex_matchtoken(scanner, '=', TINTOK_BANG_EQUAL, TINTOK_BANG);
        case '?':
            return tin_astlex_matchtoken(scanner, '?', TINTOK_QUESTION_QUESTION, TINTOK_QUESTION);
        case '%':
            return tin_astlex_matchtoken(scanner, '=', TINTOK_PERCENT_EQUAL, TINTOK_PERCENT);
        case '^':
            return tin_astlex_matchtoken(scanner, '=', TINTOK_CARET_EQUAL, TINTOK_CARET);

        case '>':
            return tin_astlex_matchntoken(scanner, '=', '>', TINTOK_GREATER_EQUAL, TINTOK_GREATER_GREATER, TINTOK_GREATER);
        case '<':
            return tin_astlex_matchntoken(scanner, '=', '<', TINTOK_LESS_EQUAL, TINTOK_LESS_LESS, TINTOK_LESS);
        case '*':
            return tin_astlex_matchntoken(scanner, '=', '*', TINTOK_STAR_EQUAL, TINTOK_STAR_STAR, TINTOK_STAR);
        case '=':
            return tin_astlex_matchntoken(scanner, '=', '>', TINTOK_EQUAL_EQUAL, TINTOK_ARROW, TINTOK_EQUAL);
        case '|':
            return tin_astlex_matchntoken(scanner, '=', '|', TINTOK_BAR_EQUAL, TINTOK_BAR_BAR, TINTOK_BAR);
        case '&':
            {
                return tin_astlex_matchntoken(scanner, '=', '&', TINTOK_AMPERSAND_EQUAL, TINTOK_AMPERSAND_AMPERSAND, TINTOK_AMPERSAND);
            }
            break;
        case '.':
            {
                if(!tin_astlex_matchchar(scanner, '.'))
                {
                    return tin_astlex_maketoken(scanner, TINTOK_DOT);
                }
                return tin_astlex_matchtoken(scanner, '.', TINTOK_DOT_DOT_DOT, TINTOK_DOT_DOT);
            }
            break;
        case '$':
            {
                if(!tin_astlex_matchchar(scanner, '\"'))
                {
                    return tin_astlex_makeerrortoken(scanner, "expected '%c' after '%c', got '%c'", '\"', '$', tin_astlex_peekcurrent(scanner));
                }
                return tin_astlex_scanstring(scanner, true);
            }
        case '"':
            return tin_astlex_scanstring(scanner, false);
    }
    return tin_astlex_makeerrortoken(scanner, "unexpected character '%c'", c);
}
