
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
        bl->values = (uint8_t*)TIN_GROW_ARRAY(state, bl->values, sizeof(uint8_t), oldcap, bl->capacity);
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

void tin_astlex_init(TinState* state, TinAstScanner* scn, const char* filename, const char* source, size_t srclength)
{
    scn->line = 1;
    scn->start = source;
    scn->srclength = srclength;
    scn->current = source;
    scn->filename = filename;
    scn->state = state;
    scn->numbraces = 0;
    scn->haderror = false;
}

static TinAstToken tin_astlex_maketoken(TinAstScanner* scn, TinAstTokType type)
{
    TinAstToken token;
    token.type = type;
    token.start = scn->start;
    token.length = (size_t)(scn->current - scn->start);
    token.line = scn->line;
    return token;
}

static TinAstToken tin_astlex_makeerrortoken(TinAstScanner* scn, const char* fmt, ...)
{
    va_list args;
    TinAstToken token;
    TinString* result;
    scn->haderror = true;
    va_start(args, fmt);
    result = tin_vformat_error(scn->state, scn->line, fmt, args);
    va_end(args);
    token.type = TINTOK_ERROR;
    token.start = result->data;
    token.length = tin_string_getlength(result);
    token.line = scn->line;
    return token;
}

static bool tin_astlex_isatend(TinAstScanner* scn)
{
    return *scn->current == '\0';
}

static char tin_astlex_advance(TinAstScanner* scn)
{
    scn->current++;
    return scn->current[-1];
}

static bool tin_astlex_matchchar(TinAstScanner* scn, char expected)
{
    if(tin_astlex_isatend(scn))
    {
        return false;
    }

    if(*scn->current != expected)
    {
        return false;
    }
    scn->current++;
    return true;
}

static TinAstToken tin_astlex_matchtoken(TinAstScanner* scn, char c, TinAstTokType a, TinAstTokType b)
{
    return tin_astlex_maketoken(scn, tin_astlex_matchchar(scn, c) ? a : b);
}

static TinAstToken tin_astlex_matchntoken(TinAstScanner* scn, char cr, char cb, TinAstTokType a, TinAstTokType b, TinAstTokType c)
{
    //return tin_astlex_maketoken(scn, tin_astlex_matchchar(scn, cr) ? a : (tin_astlex_matchchar(scn, cb) ? b : c));
    if(tin_astlex_matchchar(scn, cr))
    {
        return tin_astlex_maketoken(scn, a);
    }
    if(tin_astlex_matchchar(scn, cb))
    {
        return tin_astlex_maketoken(scn, b);
    }
    return tin_astlex_maketoken(scn, c);
}

static char tin_astlex_peekcurrent(TinAstScanner* scn)
{
    return *scn->current;
}

static char tin_astlex_peeknext(TinAstScanner* scn)
{
    if(tin_astlex_isatend(scn))
    {
        return '\0';
    }
    return scn->current[1];
}

static bool tin_astlex_skipspace(TinAstScanner* scn)
{
    char a;
    char b;
    char c;
    (void)a;
    (void)b;
    while(true)
    {
        c = tin_astlex_peekcurrent(scn);
        switch(c)
        {
            case ' ':
            case '\r':
            case '\t':
                {
                    tin_astlex_advance(scn);
                }
                break;
            case '\n':
                {
                    scn->start = scn->current;
                    tin_astlex_advance(scn);
                    return true;
                }
                break;
            case '#':
                {
                    while(tin_astlex_peekcurrent(scn) != '\n' && !tin_astlex_isatend(scn))
                    {
                        tin_astlex_advance(scn);
                    }
                    return tin_astlex_skipspace(scn);   
                }
                break;
            case '/':
                {
                    if(tin_astlex_peeknext(scn) == '/')
                    {
                        while(tin_astlex_peekcurrent(scn) != '\n' && !tin_astlex_isatend(scn))
                        {
                            tin_astlex_advance(scn);
                        }
                        return tin_astlex_skipspace(scn);
                    }
                    else if(tin_astlex_peeknext(scn) == '*')
                    {
                        tin_astlex_advance(scn);
                        tin_astlex_advance(scn);
                        a = tin_astlex_peekcurrent(scn);
                        b = tin_astlex_peeknext(scn);
                        while((tin_astlex_peekcurrent(scn) != '*' || tin_astlex_peeknext(scn) != '/') && !tin_astlex_isatend(scn))
                        {
                            if(tin_astlex_peekcurrent(scn) == '\n')
                            {
                                scn->line++;
                            }
                            tin_astlex_advance(scn);
                        }
                        tin_astlex_advance(scn);
                        tin_astlex_advance(scn);
                        return tin_astlex_skipspace(scn);
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

static TinAstToken tin_astlex_scanstring(TinAstScanner* scn, bool interpolation)
{
    char c;
    TinState* state;
    TinAstByteList bytes;
    TinAstToken token;
    TinAstTokType stringtype;
    state = scn->state;
    stringtype = TINTOK_STRING;
    tin_bytelist_init(&bytes);
    while(true)
    {
        c = tin_astlex_advance(scn);
        if(c == '\"')
        {
            break;
        }
        else if(interpolation && c == '{')
        {
            if(scn->numbraces >= TIN_MAX_INTERPOLATION_NESTING)
            {
                return tin_astlex_makeerrortoken(scn, "interpolation nesting is too deep, maximum is %i", TIN_MAX_INTERPOLATION_NESTING);
            }
            stringtype = TINTOK_STRINTERPOL;
            scn->braces[scn->numbraces++] = 1;
            break;
        }
        switch(c)
        {
            case '\0':
                {
                    return tin_astlex_makeerrortoken(scn, "unterminated string");
                }
                break;
            case '\n':
                {
                    scn->line++;
                    tin_bytelist_push(state, &bytes, c);
                }
                break;
            case '\\':
                {
                    switch(tin_astlex_advance(scn))
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
                                return tin_astlex_makeerrortoken(scn, "invalid escape character '%c'", scn->current[-1]);
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
    token = tin_astlex_maketoken(scn, stringtype);
    token.value = tin_value_fromobject(tin_string_copy(state, (const char*)bytes.values, bytes.count));
    tin_bytelist_destroy(state, &bytes);
    return token;
}

static int tin_astlex_scanhexdigit(TinAstScanner* scn)
{
    char c;
    c = tin_astlex_advance(scn);
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
    scn->current--;
    return -1;
}

static int tin_astlex_scanbinarydigit(TinAstScanner* scn)
{
    char c;
    c = tin_astlex_advance(scn);
    if(c >= '0' && c <= '1')
    {
        return c - '0';
    }
    scn->current--;
    return -1;
}

static TinAstToken tin_astlex_makenumbertoken(TinAstScanner* scn, bool ishex, bool isbinary)
{
    int64_t itmp;
    double tmp;
    TinAstToken token;
    TinValue value;
    errno = 0;
    if(ishex)
    {
        value = tin_value_makefixednumber(scn->state, (double)strtoll(scn->start, NULL, 16));
    }
    else if(isbinary)
    {
        value = tin_value_makefixednumber(scn->state, (int)strtoll(scn->start + 2, NULL, 2));
    }
    else
    {
        tmp = strtod(scn->start, NULL);
        itmp = (int64_t)tmp;
        if(itmp == tmp)
        {
            value = tin_value_makefixednumber(scn->state, tmp);
        }
        else
        {
            value = tin_value_makefloatnumber(scn->state, tmp);
        }
    }

    if(errno == ERANGE)
    {
        errno = 0;
        return tin_astlex_makeerrortoken(scn, "number is too big to be represented by a single literal");
    }
    token = tin_astlex_maketoken(scn, TINTOK_NUMBER);
    token.value = value;
    return token;
}

static TinAstToken tin_astlex_scannumber(TinAstScanner* scn)
{
    if(tin_astlex_matchchar(scn, 'x'))
    {
        while(tin_astlex_scanhexdigit(scn) != -1)
        {
            continue;
        }
        return tin_astlex_makenumbertoken(scn, true, false);
    }
    if(tin_astlex_matchchar(scn, 'b'))
    {
        while(tin_astlex_scanbinarydigit(scn) != -1)
        {
            continue;
        }
        return tin_astlex_makenumbertoken(scn, false, true);
    }
    while(tin_lexutil_isdigit(tin_astlex_peekcurrent(scn)))
    {
        tin_astlex_advance(scn);
    }
    // Look for a fractional part.
    if(tin_astlex_peekcurrent(scn) == '.' && tin_lexutil_isdigit(tin_astlex_peeknext(scn)))
    {
        // Consume the '.'
        tin_astlex_advance(scn);
        while(tin_lexutil_isdigit(tin_astlex_peekcurrent(scn)))
        {
            tin_astlex_advance(scn);
        }
    }
    return tin_astlex_makenumbertoken(scn, false, false);
}

static TinAstTokType tin_astlex_checkkeyword(TinAstScanner* scn, int start, int length, const char* rest, TinAstTokType type)
{
    if(scn->current - scn->start == start + length && memcmp(scn->start + start, rest, length) == 0)
    {
        return type;
    }
    return TINTOK_IDENT;
}

static TinAstTokType tin_astlex_scanidenttype(TinAstScanner* scn)
{
    static struct {
        TinAstTokType type;
        const char* kw;
    } keywords[] =
    {
        {TINTOK_KWCLASS, "class"},
        {TINTOK_KWELSE, "else"},
        {TINTOK_KWFALSE, "false"},
        {TINTOK_KWFOR, "for"},
        {TINTOK_KWFUNCTION, "function"},
        {TINTOK_KWIF, "if"},
        {TINTOK_KWNULL, "null"},
        {TINTOK_KWRETURN, "return"},
        {TINTOK_KWSUPER, TIN_VALUE_SUPERNAME},
        {TINTOK_KWTHIS, TIN_VALUE_THISNAME},
        {TINTOK_KWTRUE, "true"},
        {TINTOK_KWVAR, "var"},
        {TINTOK_KWWHILE, "while"},
        {TINTOK_KWCONTINUE, "continue"},
        {TINTOK_KWBREAK, "break"},
        {TINTOK_KWNEW, "new"},
        {TINTOK_KWEXPORT, "export"},
        {TINTOK_KWIS, "is"},
        {TINTOK_KWSTATIC, "static"},
        {TINTOK_KWOPERATOR, "operator"},
        {TINTOK_KWGET, "get"},
        {TINTOK_KWSET, "set"},
        {TINTOK_KWIN, "in"},
        {TINTOK_KWCONST, "const"},
        {TINTOK_KWREF, "ref"},
        {0, NULL},
    };
    size_t i;
    size_t kwlen;
    size_t ofs;
    const char* kwtext;
    for(i=0; keywords[i].kw != NULL; i++)
    {
        kwtext = keywords[i].kw;
        kwlen = strlen(keywords[i].kw);
        ofs = (scn->current - scn->start);
        if((ofs == (0 + kwlen)) && (memcmp(scn->start + 0, kwtext, kwlen) == 0))
        {
            return keywords[i].type;
        }
    }
    /*
    switch(scn->start[0])
    {
        case 'b':
            return tin_astlex_checkkeyword(scn, 1, 4, "reak", TINTOK_KWBREAK);

        case 'c':
            {
                if(scn->current - scn->start > 1)
                {
                    switch(scn->start[1])
                    {
                        case 'l':
                            return tin_astlex_checkkeyword(scn, 2, 3, "ass", TINTOK_KWCLASS);
                        case 'o':
                        {
                            if(scn->current - scn->start > 3)
                            {
                                switch(scn->start[3])
                                {
                                    case 's':
                                        return tin_astlex_checkkeyword(scn, 2, 3, "nst", TINTOK_KWCONST);
                                    case 't':
                                        return tin_astlex_checkkeyword(scn, 2, 6, "ntinue", TINTOK_KWCONTINUE);
                                }
                            }
                        }
                    }
                }
            }
            break;
        case 'e':
            {
                if(scn->current - scn->start > 1)
                {
                    switch(scn->start[1])
                    {
                        case 'l':
                            return tin_astlex_checkkeyword(scn, 2, 2, "se", TINTOK_KWELSE);
                        case 'x':
                            return tin_astlex_checkkeyword(scn, 2, 4, "port", TINTOK_KWEXPORT);
                    }
                }
            }
            break;
        case 'f':
            {
                if(scn->current - scn->start > 1)
                {
                    switch(scn->start[1])
                    {
                        case 'a':
                            return tin_astlex_checkkeyword(scn, 2, 3, "lse", TINTOK_KWFALSE);
                        case 'o':
                            return tin_astlex_checkkeyword(scn, 2, 1, "r", TINTOK_KWFOR);
                        case 'u':
                            return tin_astlex_checkkeyword(scn, 2, 6, "nction", TINTOK_KWFUNCTION);
                    }
                }
            }
            break;
        case 'i':
            {
                if(scn->current - scn->start > 1)
                {
                    switch(scn->start[1])
                    {
                        case 's':
                            return tin_astlex_checkkeyword(scn, 2, 0, "", TINTOK_KWIS);
                        case 'f':
                            return tin_astlex_checkkeyword(scn, 2, 0, "", TINTOK_KWIF);
                        case 'n':
                            return tin_astlex_checkkeyword(scn, 2, 0, "", TINTOK_KWIN);
                    }
                }
            }
            break;
        case 'n':
        {
            if(scn->current - scn->start > 1)
            {
                switch(scn->start[1])
                {
                    case 'u':
                        return tin_astlex_checkkeyword(scn, 2, 2, "ll", TINTOK_KWNULL);
                    case 'e':
                        return tin_astlex_checkkeyword(scn, 2, 1, "w", TINTOK_KWNEW);
                }
            }

            break;
        }

        case 'r':
        {
            if(scn->current - scn->start > 2)
            {
                switch(scn->start[2])
                {
                    case 'f':
                        return tin_astlex_checkkeyword(scn, 3, 0, "", TINTOK_KWREF);
                    case 't':
                        return tin_astlex_checkkeyword(scn, 3, 3, "urn", TINTOK_KWRETURN);
                }
            }

            break;
        }

        case 'o':
            return tin_astlex_checkkeyword(scn, 1, 7, "perator", TINTOK_KWOPERATOR);

        case 's':
        {
            if(scn->current - scn->start > 1)
            {
                switch(scn->start[1])
                {
                    case 'u':
                        return tin_astlex_checkkeyword(scn, 2, 3, "per", TINTOK_KWSUPER);
                    case 't':
                        return tin_astlex_checkkeyword(scn, 2, 4, "atic", TINTOK_KWSTATIC);
                    case 'e':
                        return tin_astlex_checkkeyword(scn, 2, 1, "t", TINTOK_KWSET);
                }
            }

            break;
        }

        case 't':
        {
            if(scn->current - scn->start > 1)
            {
                switch(scn->start[1])
                {
                    case 'h':
                        return tin_astlex_checkkeyword(scn, 2, 2, "is", TINTOK_KWTHIS);
                    case 'r':
                        return tin_astlex_checkkeyword(scn, 2, 2, "ue", TINTOK_KWTRUE);
                }
            }

            break;
        }

        case 'v':
            return tin_astlex_checkkeyword(scn, 1, 2, "ar", TINTOK_KWVAR);
        case 'w':
            return tin_astlex_checkkeyword(scn, 1, 4, "hile", TINTOK_KWWHILE);
        case 'g':
            return tin_astlex_checkkeyword(scn, 1, 2, "et", TINTOK_KWGET);
    }
    */
    return TINTOK_IDENT;
}

static TinAstToken tin_astlex_scanidentifier(TinAstScanner* scn)
{
    while(tin_lexutil_isalpha(tin_astlex_peekcurrent(scn)) || tin_lexutil_isdigit(tin_astlex_peekcurrent(scn)))
    {
        tin_astlex_advance(scn);
    }
    return tin_astlex_maketoken(scn, tin_astlex_scanidenttype(scn));
}

TinAstToken tin_astlex_scantoken(TinAstScanner* scn)
{
    char c;
    TinAstToken token;
    if(tin_astlex_skipspace(scn))
    {
        token = tin_astlex_maketoken(scn, TINTOK_NEWLINE);
        scn->line++;
        return token;
    }
    scn->start = scn->current;
    if(tin_astlex_isatend(scn))
    {
        return tin_astlex_maketoken(scn, TINTOK_EOF);
    }
    c = tin_astlex_advance(scn);
    if(tin_lexutil_isdigit(c))
    {
        return tin_astlex_scannumber(scn);
    }
    if(tin_lexutil_isalpha(c))
    {
        return tin_astlex_scanidentifier(scn);
    }
    switch(c)
    {
        case '(':
            return tin_astlex_maketoken(scn, TINTOK_PARENOPEN);
        case ')':
            return tin_astlex_maketoken(scn, TINTOK_PARENCLOSE);
        case '{':
            {
                if(scn->numbraces > 0)
                {
                    scn->braces[scn->numbraces - 1]++;
                }
                return tin_astlex_maketoken(scn, TINTOK_BRACEOPEN);
            }
            break;
        case '}':
            {
                if(scn->numbraces > 0 && --scn->braces[scn->numbraces - 1] == 0)
                {
                    scn->numbraces--;
                    return tin_astlex_scanstring(scn, true);
                }
                return tin_astlex_maketoken(scn, TINTOK_BRACECLOSE);
            }
            break;
        case '[':
            return tin_astlex_maketoken(scn, TINTOK_BRACKETOPEN);
        case ']':
            return tin_astlex_maketoken(scn, TINTOK_BRACKETCLOSE);
        case ';':
            return tin_astlex_maketoken(scn, TINTOK_SEMICOLON);
        case ',':
            return tin_astlex_maketoken(scn, TINTOK_COMMA);
        case ':':
            return tin_astlex_maketoken(scn, TINTOK_COLON);
        case '~':
            return tin_astlex_maketoken(scn, TINTOK_TILDE);
        case '+':
            return tin_astlex_matchntoken(scn, '=', '+', TINTOK_PLUSEQUAL, TINTOK_DOUBLEPLUS, TINTOK_PLUS);
        case '-':
            {
                if(tin_astlex_matchchar(scn, '>'))
                {
                    return tin_astlex_maketoken(scn, TINTOK_SMALLARROW);
                }
                return tin_astlex_matchntoken(scn, '=', '-', TINTOK_MINUSEQUAL, TINTOK_DOUBLEMINUS, TINTOK_MINUS);
            }
            break;
        case '/':
            return tin_astlex_matchtoken(scn, '=', TINTOK_SLASHEQUAL, TINTOK_SLASH);
        case '!':
            return tin_astlex_matchtoken(scn, '=', TINTOK_BANGEQUAL, TINTOK_BANG);
        case '?':
            return tin_astlex_matchtoken(scn, '?', TINTOK_DOUBLEQUESTION, TINTOK_QUESTION);
        case '%':
            return tin_astlex_matchtoken(scn, '=', TINTOK_PERCENTEQUAL, TINTOK_PERCENT);
        case '^':
            return tin_astlex_matchtoken(scn, '=', TINTOK_CARETEQUAL, TINTOK_CARET);
        case '>':
            return tin_astlex_matchntoken(scn, '=', '>', TINTOK_GREATEREQUAL, TINTOK_SHIFTRIGHT, TINTOK_GREATERTHAN);
        case '<':
            return tin_astlex_matchntoken(scn, '=', '<', TINTOK_LESSEQUAL, TINTOK_SHIFTLEFT, TINTOK_LESSTHAN);
        case '*':
            return tin_astlex_matchntoken(scn, '=', '*', TINTOK_STAREQUAL, TINTOK_DOUBLESTAR, TINTOK_STAR);
        case '=':
            return tin_astlex_matchntoken(scn, '=', '>', TINTOK_EQUAL, TINTOK_ARROW, TINTOK_ASSIGN);
        case '|':
            return tin_astlex_matchntoken(scn, '=', '|', TINTOK_ASSIGNEQUAL, TINTOK_DOUBLEBAR, TINTOK_BAR);
        case '&':
            {
                return tin_astlex_matchntoken(scn, '=', '&', TINTOK_AMPERSANDEQUAL, TINTOK_DOUBLEAMPERSAND, TINTOK_AMPERSAND);
            }
            break;
        case '.':
            {
                if(!tin_astlex_matchchar(scn, '.'))
                {
                    return tin_astlex_maketoken(scn, TINTOK_DOT);
                }
                return tin_astlex_matchtoken(scn, '.', TINTOK_TRIPLEDOT, TINTOK_DOUBLEDOT);
            }
            break;
        case '$':
            {
                if(!tin_astlex_matchchar(scn, '\"'))
                {
                    return tin_astlex_makeerrortoken(scn, "expected '%c' after '%c', got '%c'", '\"', '$', tin_astlex_peekcurrent(scn));
                }
                return tin_astlex_scanstring(scn, true);
            }
        case '"':
            return tin_astlex_scanstring(scn, false);
    }
    return tin_astlex_makeerrortoken(scn, "unexpected character '%c'", c);
}
