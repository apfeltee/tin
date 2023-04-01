
#include <stdlib.h>
#include <setjmp.h>
#include "priv.h"

static jmp_buf prs_jmpbuffer;
static TinAstParseRule rules[TINTOK_EOF + 1];


static TinAstTokType operators[]=
{
    TINTOK_PLUS, TINTOK_MINUS, TINTOK_STAR, TINTOK_PERCENT, TINTOK_SLASH,
    TINTOK_SHARP, TINTOK_BANG, TINTOK_LESSTHAN, TINTOK_LESSEQUAL, TINTOK_GREATERTHAN,
    TINTOK_GREATEREQUAL, TINTOK_EQUAL, TINTOK_BRACKETOPEN, TINTOK_EOF
};


static bool didsetuprules;
static void tin_astparser_setuprules();
static void tin_astparser_sync(TinAstParser* prs);

static TinAstExpression *tin_astparser_parseblock(TinAstParser *prs);
static TinAstExpression *tin_astparser_parseprecedence(TinAstParser *prs, TinAstPrecedence precedence, bool err, bool ignsemi);
static TinAstExpression *tin_astparser_parselambda(TinAstParser *prs, TinAstFunctionExpr *lambda);
static void tin_astparser_parseparameters(TinAstParser *prs, TinAstParamList *parameters);
static TinAstExpression *tin_astparser_parseexpression(TinAstParser *prs, bool ignsemi);
static TinAstExpression *tin_astparser_parsevar_declaration(TinAstParser *prs, bool ignsemi);
static TinAstExpression *tin_astparser_parseif(TinAstParser *prs);
static TinAstExpression *tin_astparser_parsefor(TinAstParser *prs);
static TinAstExpression *tin_astparser_parsewhile(TinAstParser *prs);
static TinAstExpression *tin_astparser_parsereturn(TinAstParser *prs);
static TinAstExpression *tin_astparser_parsefield(TinAstParser *prs, TinString *name, bool isstatic);
static TinAstExpression *tin_astparser_parsemethod(TinAstParser *prs, bool isstatic);
static TinAstExpression *tin_astparser_parseclass(TinAstParser *prs);
static TinAstExpression *tin_astparser_parsestatement(TinAstParser *prs);
static TinAstExpression *tin_astparser_parsedeclaration(TinAstParser *prs);

static TinAstExpression *tin_astparser_rulenumber(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_rulegroupingorlambda(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_rulecall(TinAstParser *prs, TinAstExpression *prev, bool canassign);
static TinAstExpression *tin_astparser_ruleunary(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_rulebinary(TinAstParser *prs, TinAstExpression *prev, bool canassign);
static TinAstExpression *tin_astparser_ruleand(TinAstParser *prs, TinAstExpression *prev, bool canassign);
static TinAstExpression *tin_astparser_ruleor(TinAstParser *prs, TinAstExpression *prev, bool canassign);
static TinAstExpression *tin_astparser_rulenull_filter(TinAstParser *prs, TinAstExpression *prev, bool canassign);
static TinAstExpression *tin_astparser_rulecompound(TinAstParser *prs, TinAstExpression *prev, bool canassign);
static TinAstExpression *tin_astparser_ruleliteral(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_rulestring(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_ruleinterpolation(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_ruleobject(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_rulevarexprbase(TinAstParser *prs, bool canassign, bool isnew);
static TinAstExpression *tin_astparser_rulevarexpr(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_rulenewexpr(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_ruledot(TinAstParser *prs, TinAstExpression *previous, bool canassign);
static TinAstExpression *tin_astparser_rulerange(TinAstParser *prs, TinAstExpression *previous, bool canassign);
static TinAstExpression *tin_astparser_ruleternary(TinAstParser *prs, TinAstExpression *previous, bool canassign);
static TinAstExpression *tin_astparser_rulearray(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_rulesubscript(TinAstParser *prs, TinAstExpression *previous, bool canassign);
static TinAstExpression *tin_astparser_rulethis(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_rulesuper(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_rulereference(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_rulenothing(TinAstParser *prs, bool canassign);
static TinAstExpression *tin_astparser_rulefunction(TinAstParser *prs, bool canassign);

#if defined(__cplusplus)
    #define TIN_MAKERULE(...) TinAstParseRule{__VA_ARGS__}
#else
    #define TIN_MAKERULE(...) (TinAstParseRule){__VA_ARGS__}
#endif

static void tin_astparser_setuprules()
{
    rules[TINTOK_PARENOPEN] = TIN_MAKERULE( tin_astparser_rulegroupingorlambda, tin_astparser_rulecall, TINPREC_CALL );
    rules[TINTOK_PLUS] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_TERM );
    rules[TINTOK_MINUS] = TIN_MAKERULE( tin_astparser_ruleunary, tin_astparser_rulebinary, TINPREC_TERM );
    rules[TINTOK_BANG] = TIN_MAKERULE( tin_astparser_ruleunary, tin_astparser_rulebinary, TINPREC_TERM );
    rules[TINTOK_STAR] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_FACTOR );
    rules[TINTOK_DOUBLESTAR] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_FACTOR );
    rules[TINTOK_SLASH] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_FACTOR );
    rules[TINTOK_SHARP] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_FACTOR );
    rules[TINTOK_STAR] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_FACTOR );
    rules[TINTOK_STAR] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_FACTOR );
    rules[TINTOK_BAR] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_BOR );
    rules[TINTOK_AMPERSAND] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_BAND );
    rules[TINTOK_TILDE] = TIN_MAKERULE( tin_astparser_ruleunary, NULL, TINPREC_UNARY );
    rules[TINTOK_CARET] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_BOR );
    rules[TINTOK_SHIFTLEFT] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_SHIFT );
    rules[TINTOK_SHIFTRIGHT] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_SHIFT );
    rules[TINTOK_PERCENT] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_FACTOR );
    rules[TINTOK_KWIS] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_IS );
    rules[TINTOK_NUMBER] = TIN_MAKERULE( tin_astparser_rulenumber, NULL, TINPREC_NONE );
    rules[TINTOK_KWTRUE] = TIN_MAKERULE( tin_astparser_ruleliteral, NULL, TINPREC_NONE );
    rules[TINTOK_KWFALSE] = TIN_MAKERULE( tin_astparser_ruleliteral, NULL, TINPREC_NONE );
    rules[TINTOK_KWNULL] = TIN_MAKERULE( tin_astparser_ruleliteral, NULL, TINPREC_NONE );
    rules[TINTOK_BANGEQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_EQUALITY );
    rules[TINTOK_EQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_EQUALITY );
    rules[TINTOK_GREATERTHAN] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_COMPARISON );
    rules[TINTOK_GREATEREQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_COMPARISON );
    rules[TINTOK_LESSTHAN] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_COMPARISON );
    rules[TINTOK_LESSEQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulebinary, TINPREC_COMPARISON );
    rules[TINTOK_STRING] = TIN_MAKERULE( tin_astparser_rulestring, NULL, TINPREC_NONE );
    rules[TINTOK_STRINTERPOL] = TIN_MAKERULE( tin_astparser_ruleinterpolation, NULL, TINPREC_NONE );
    rules[TINTOK_IDENT] = TIN_MAKERULE( tin_astparser_rulevarexpr, NULL, TINPREC_NONE );
    rules[TINTOK_KWNEW] = TIN_MAKERULE( tin_astparser_rulenewexpr, NULL, TINPREC_NONE );
    rules[TINTOK_PLUSEQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulecompound, TINPREC_COMPOUND );
    rules[TINTOK_MINUSEQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulecompound, TINPREC_COMPOUND );
    rules[TINTOK_STAREQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulecompound, TINPREC_COMPOUND );
    rules[TINTOK_SLASHEQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulecompound, TINPREC_COMPOUND );
    rules[TINTOK_SHARPEQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulecompound, TINPREC_COMPOUND );
    rules[TINTOK_PERCENTEQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulecompound, TINPREC_COMPOUND );
    rules[TINTOK_CARETEQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulecompound, TINPREC_COMPOUND );
    rules[TINTOK_ASSIGNEQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulecompound, TINPREC_COMPOUND );
    rules[TINTOK_AMPERSANDEQUAL] = TIN_MAKERULE( NULL, tin_astparser_rulecompound, TINPREC_COMPOUND );
    rules[TINTOK_DOUBLEPLUS] = TIN_MAKERULE( NULL, tin_astparser_rulecompound, TINPREC_COMPOUND );
    rules[TINTOK_DOUBLEMINUS] = TIN_MAKERULE( NULL, tin_astparser_rulecompound, TINPREC_COMPOUND );
    rules[TINTOK_DOUBLEAMPERSAND] = TIN_MAKERULE( NULL, tin_astparser_ruleand, TINPREC_AND );
    rules[TINTOK_DOUBLEBAR] = TIN_MAKERULE( NULL, tin_astparser_ruleor, TINPREC_AND );
    rules[TINTOK_DOUBLEQUESTION] = TIN_MAKERULE( NULL, tin_astparser_rulenull_filter, TINPREC_NULL );
    rules[TINTOK_DOT] = TIN_MAKERULE( NULL, tin_astparser_ruledot, TINPREC_CALL );
    rules[TINTOK_SMALLARROW] = TIN_MAKERULE( NULL, tin_astparser_ruledot, TINPREC_CALL );
    rules[TINTOK_DOUBLEDOT] = TIN_MAKERULE( NULL, tin_astparser_rulerange, TINPREC_RANGE );
    rules[TINTOK_TRIPLEDOT] = TIN_MAKERULE( tin_astparser_rulevarexpr, NULL, TINPREC_ASSIGNMENT );
    rules[TINTOK_BRACKETOPEN] = TIN_MAKERULE( tin_astparser_rulearray, tin_astparser_rulesubscript, TINPREC_NONE );
    rules[TINTOK_BRACEOPEN] = TIN_MAKERULE( tin_astparser_ruleobject, NULL, TINPREC_NONE );
    rules[TINTOK_KWTHIS] = TIN_MAKERULE( tin_astparser_rulethis, NULL, TINPREC_NONE );
    rules[TINTOK_KWSUPER] = TIN_MAKERULE( tin_astparser_rulesuper, NULL, TINPREC_NONE );
    rules[TINTOK_QUESTION] = TIN_MAKERULE( NULL, tin_astparser_ruleternary, TINPREC_EQUALITY );
    rules[TINTOK_KWREF] = TIN_MAKERULE( tin_astparser_rulereference, NULL, TINPREC_NONE );
    rules[TINTOK_KWFUNCTION] = TIN_MAKERULE(tin_astparser_rulefunction, NULL, TINPREC_NONE);
    rules[TINTOK_SEMICOLON] = TIN_MAKERULE(tin_astparser_rulenothing, NULL, TINPREC_NONE);
}


const char* tin_astparser_token2name(int t)
{
    switch(t)
    {
        case TINTOK_NEWLINE: return "TINTOK_NEWLINE";
        case TINTOK_PARENOPEN: return "TINTOK_PARENOPEN";
        case TINTOK_PARENCLOSE: return "TINTOK_PARENCLOSE";
        case TINTOK_BRACEOPEN: return "TINTOK_BRACEOPEN";
        case TINTOK_BRACECLOSE: return "TINTOK_BRACECLOSE";
        case TINTOK_BRACKETOPEN: return "TINTOK_BRACKETOPEN";
        case TINTOK_BRACKETCLOSE: return "TINTOK_BRACKETCLOSE";
        case TINTOK_COMMA: return "TINTOK_COMMA";
        case TINTOK_SEMICOLON: return "TINTOK_SEMICOLON";
        case TINTOK_COLON: return "TINTOK_COLON";
        case TINTOK_ASSIGNEQUAL: return "TINTOK_ASSIGNEQUAL";
        case TINTOK_BAR: return "TINTOK_BAR";
        case TINTOK_DOUBLEBAR: return "TINTOK_DOUBLEBAR";
        case TINTOK_AMPERSANDEQUAL: return "TINTOK_AMPERSANDEQUAL";
        case TINTOK_AMPERSAND: return "TINTOK_AMPERSAND";
        case TINTOK_DOUBLEAMPERSAND: return "TINTOK_DOUBLEAMPERSAND";
        case TINTOK_BANG: return "TINTOK_BANG";
        case TINTOK_BANGEQUAL: return "TINTOK_BANGEQUAL";
        case TINTOK_ASSIGN: return "TINTOK_ASSIGN";
        case TINTOK_EQUAL: return "TINTOK_EQUAL";
        case TINTOK_GREATERTHAN: return "TINTOK_GREATERTHAN";
        case TINTOK_GREATEREQUAL: return "TINTOK_GREATEREQUAL";
        case TINTOK_SHIFTRIGHT: return "TINTOK_SHIFTRIGHT";
        case TINTOK_LESSTHAN: return "TINTOK_LESSTHAN";
        case TINTOK_LESSEQUAL: return "TINTOK_LESSEQUAL";
        case TINTOK_SHIFTLEFT: return "TINTOK_SHIFTLEFT";
        case TINTOK_PLUS: return "TINTOK_PLUS";
        case TINTOK_PLUSEQUAL: return "TINTOK_PLUSEQUAL";
        case TINTOK_DOUBLEPLUS: return "TINTOK_DOUBLEPLUS";
        case TINTOK_MINUS: return "TINTOK_MINUS";
        case TINTOK_MINUSEQUAL: return "TINTOK_MINUSEQUAL";
        case TINTOK_DOUBLEMINUS: return "TINTOK_DOUBLEMINUS";
        case TINTOK_STAR: return "TINTOK_STAR";
        case TINTOK_STAREQUAL: return "TINTOK_STAREQUAL";
        case TINTOK_DOUBLESTAR: return "TINTOK_DOUBLESTAR";
        case TINTOK_SLASH: return "TINTOK_SLASH";
        case TINTOK_SLASHEQUAL: return "TINTOK_SLASHEQUAL";
        case TINTOK_QUESTION: return "TINTOK_QUESTION";
        case TINTOK_DOUBLEQUESTION: return "TINTOK_DOUBLEQUESTION";
        case TINTOK_PERCENT: return "TINTOK_PERCENT";
        case TINTOK_PERCENTEQUAL: return "TINTOK_PERCENTEQUAL";
        case TINTOK_ARROW: return "TINTOK_ARROW";
        case TINTOK_SMALLARROW: return "TINTOK_SMALLARROW";
        case TINTOK_TILDE: return "TINTOK_TILDE";
        case TINTOK_CARET: return "TINTOK_CARET";
        case TINTOK_CARETEQUAL: return "TINTOK_CARETEQUAL";
        case TINTOK_DOT: return "TINTOK_DOT";
        case TINTOK_DOUBLEDOT: return "TINTOK_DOUBLEDOT";
        case TINTOK_TRIPLEDOT: return "TINTOK_TRIPLEDOT";
        case TINTOK_SHARP: return "TINTOK_SHARP";
        case TINTOK_SHARPEQUAL: return "TINTOK_SHARPEQUAL";
        case TINTOK_IDENT: return "TINTOK_IDENT";
        case TINTOK_STRING: return "TINTOK_STRING";
        case TINTOK_STRINTERPOL: return "TINTOK_STRINTERPOL";
        case TINTOK_NUMBER: return "TINTOK_NUMBER";
        case TINTOK_KWCLASS: return "TINTOK_KWCLASS";
        case TINTOK_KWELSE: return "TINTOK_KWELSE";
        case TINTOK_KWFALSE: return "TINTOK_KWFALSE";
        case TINTOK_KWFOR: return "TINTOK_KWFOR";
        case TINTOK_KWFUNCTION: return "TINTOK_KWFUNCTION";
        case TINTOK_KWIF: return "TINTOK_KWIF";
        case TINTOK_KWNULL: return "TINTOK_KWNULL";
        case TINTOK_KWRETURN: return "TINTOK_KWRETURN";
        case TINTOK_KWSUPER: return "TINTOK_KWSUPER";
        case TINTOK_KWTHIS: return "TINTOK_KWTHIS";
        case TINTOK_KWTRUE: return "TINTOK_KWTRUE";
        case TINTOK_KWVAR: return "TINTOK_KWVAR";
        case TINTOK_KWWHILE: return "TINTOK_KWWHILE";
        case TINTOK_KWCONTINUE: return "TINTOK_KWCONTINUE";
        case TINTOK_KWBREAK: return "TINTOK_KWBREAK";
        case TINTOK_KWNEW: return "TINTOK_KWNEW";
        case TINTOK_KWEXPORT: return "TINTOK_KWEXPORT";
        case TINTOK_KWIS: return "TINTOK_KWIS";
        case TINTOK_KWSTATIC: return "TINTOK_KWSTATIC";
        case TINTOK_KWOPERATOR: return "TINTOK_KWOPERATOR";
        case TINTOK_KWGET: return "TINTOK_KWGET";
        case TINTOK_KWSET: return "TINTOK_KWSET";
        case TINTOK_KWIN: return "TINTOK_KWIN";
        case TINTOK_KWCONST: return "TINTOK_KWCONST";
        case TINTOK_KWREF: return "TINTOK_KWREF";
        case TINTOK_ERROR: return "TINTOK_ERROR";
        case TINTOK_EOF: return "TINTOK_EOF";
        default:
            break;
    }
    return "?unknown?";
}


static void tin_astparser_initcompiler(TinAstParser* prs, TinAstCompiler* compiler)
{
    compiler->scopedepth = 0;
    compiler->function = NULL;
    compiler->enclosing = (struct TinAstCompiler*)prs->compiler;

    prs->compiler = compiler;
}

static void tin_astparser_endcompiler(TinAstParser* prs, TinAstCompiler* compiler)
{
    prs->compiler = (TinAstCompiler*)compiler->enclosing;
}

static void tin_astparser_beginscope(TinAstParser* prs)
{
    prs->compiler->scopedepth++;
}

static void tin_astparser_endscope(TinAstParser* prs)
{
    prs->compiler->scopedepth--;
}

static TinAstParseRule* tin_astparser_getrule(TinAstTokType type)
{
    return &rules[type];
}

static inline bool prs_is_at_end(TinAstParser* prs)
{
    return prs->current.type == TINTOK_EOF;
}

void tin_astparser_init(TinState* state, TinAstParser* prs)
{
    if(!didsetuprules)
    {
        didsetuprules = true;
        tin_astparser_setuprules();
    }
    prs->state = state;
    prs->haderror = false;
    prs->panicmode = false;
}

void tin_astparser_destroy(TinAstParser* prs)
{
    (void)prs;
}

static void tin_astparser_raisestring(TinAstParser* prs, TinAstToken* token, const char* message)
{
    (void)token;
    if(prs->panicmode)
    {
        return;
    }
    tin_state_raiseerror(prs->state, COMPILE_ERROR, message);
    prs->haderror = true;
    tin_astparser_sync(prs);
}

static void tin_astparser_raiseat(TinAstParser* prs, TinAstToken* token, const char* fmt, va_list args)
{
    tin_astparser_raisestring(prs, token, tin_vformat_error(prs->state, token->line, fmt, args)->data);
}

static void tin_astparser_raiseatcurrent(TinAstParser* prs, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    tin_astparser_raiseat(prs, &prs->current, fmt, args);
    va_end(args);
}

static void tin_astparser_raiseerror(TinAstParser* prs, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    tin_astparser_raiseat(prs, &prs->previous, fmt, args);
    va_end(args);
}

static void tin_astparser_advance(TinAstParser* prs)
{
    prs->prevprev = prs->previous;
    prs->previous = prs->current;
    while(true)
    {
        prs->current = tin_astlex_scantoken(prs->state->scanner);
        if(prs->current.type != TINTOK_ERROR)
        {
            break;
        }
        tin_astparser_raisestring(prs, &prs->current, prs->current.start);
    }
}

static bool tin_astparser_check(TinAstParser* prs, TinAstTokType type)
{
    return prs->current.type == type;
}

static bool tin_astparser_match(TinAstParser* prs, TinAstTokType type)
{
    if(prs->current.type == type)
    {
        tin_astparser_advance(prs);
        return true;
    }
    return false;
}

static bool tin_astparser_matchnewline(TinAstParser* prs)
{
    while(true)
    {
        if(!tin_astparser_match(prs, TINTOK_NEWLINE))
        {
            return false;
        }
    }
    return true;
}

static void tin_astparser_ignorenewlines(TinAstParser* prs, bool checksemi)
{
    (void)checksemi;
    tin_astparser_matchnewline(prs);
}

static void tin_astparser_consume(TinAstParser* prs, TinAstTokType type, const char* onerror)
{
    bool line;
    size_t chlen;
    size_t olen;
    const char* fmt;
    const char* otext;
    TinString* ts;
    if(prs->current.type == type)
    {
        tin_astparser_advance(prs);
        return;
    }
    //fprintf(stderr, "in tin_astparser_consume: failed?\n");
    line = (prs->previous.type == TINTOK_NEWLINE);
    otext = "new line";
    olen = strlen(otext);
    if(!line)
    {
        olen = prs->previous.length;
        otext = prs->previous.start; 
    }
    chlen = strlen(otext);
    if(olen > chlen)
    {
        olen = chlen;
    }
    ts = tin_format_error(prs->state, prs->current.line, "expected %s, got '%.*s'", onerror, olen, otext);
    fmt = ts->data;
    tin_astparser_raisestring(prs, &prs->current,fmt);
}

static TinAstExpression* tin_astparser_parseblock(TinAstParser* prs)
{
    TinAstBlockExpr* statement;
    tin_astparser_beginscope(prs);
    statement = tin_ast_make_blockexpr(prs->state, prs->previous.line);
    while(true)
    {
        tin_astparser_ignorenewlines(prs, true);
        if(tin_astparser_check(prs, TINTOK_BRACECLOSE) || tin_astparser_check(prs, TINTOK_EOF))
        {
            break;
        }
        tin_astparser_ignorenewlines(prs, true);
        tin_exprlist_push(prs->state, &statement->statements, tin_astparser_parsestatement(prs));
        tin_astparser_ignorenewlines(prs, true);
    }
    tin_astparser_ignorenewlines(prs, true);
    tin_astparser_consume(prs, TINTOK_BRACECLOSE, "'}'");
    tin_astparser_ignorenewlines(prs, true);
    tin_astparser_endscope(prs);
    return (TinAstExpression*)statement;
}

static TinAstExpression* tin_astparser_parseprecedence(TinAstParser* prs, TinAstPrecedence precedence, bool err, bool ignsemi)
{
    bool expisnewline;
    bool gotisnewline;
    bool canassign;
    size_t explen;
    size_t gotlen;
    size_t nllen;
    const char* exptext;
    const char* gottxt;
    const char* nltext;
    TinAstExpression* expr;
    TinAstParsePrefixFn prefixrule;
    TinAstParseInfixFn infixrule;
    TinAstToken previous;
    previous = prs->previous;
    tin_astparser_ignorenewlines(prs, true);
    tin_astparser_advance(prs);
    prefixrule = tin_astparser_getrule(prs->previous.type)->prefix;
    if(prefixrule == NULL)
    {
        nltext = "new line";
        nllen = strlen(nltext);
        {
            // todo: file start
            expisnewline = ((previous.start != NULL) && (*previous.start == '\n'));
            gotisnewline = ((prs->previous.start != NULL) && (*prs->previous.start == '\n'));
            explen = (expisnewline ? nllen : previous.length);
            exptext = (expisnewline ? nltext : previous.start);
            gotlen = (gotisnewline ? nllen : prs->previous.length);
            gottxt = (gotisnewline ? nltext : prs->previous.start);
            tin_astparser_raiseerror(prs, "expected expression after '%.*s', got '%.*s'", explen, exptext, gotlen, gottxt);
            return NULL;
        }
    }
    canassign = precedence <= TINPREC_ASSIGNMENT;
    expr = prefixrule(prs, canassign);
    tin_astparser_ignorenewlines(prs, ignsemi);
    while(precedence <= tin_astparser_getrule(prs->current.type)->precedence)
    {
        tin_astparser_ignorenewlines(prs, true);
        tin_astparser_advance(prs);
        infixrule = tin_astparser_getrule(prs->previous.type)->infix;
        expr = infixrule(prs, expr, canassign);
    }
    if(err && canassign && tin_astparser_match(prs, TINTOK_ASSIGN))
    {
        tin_astparser_raiseerror(prs, "invalid assigment target");
    }
    return expr;
}

static TinAstExpression* tin_astparser_rulenumber(TinAstParser* prs, bool canassign)
{
    (void)canassign;
    return (TinAstExpression*)tin_ast_make_literalexpr(prs->state, prs->previous.line, prs->previous.value);
}

static TinAstExpression* tin_astparser_parselambda(TinAstParser* prs, TinAstFunctionExpr* lambda)
{
    lambda->body = tin_astparser_parsestatement(prs);
    return (TinAstExpression*)lambda;
}

static inline TinAstParameter tin_astparser_makeparam(const char* name, size_t length, TinAstExpression* defexpr)
{
    TinAstParameter tp;
    tp.name = name;
    tp.length = length;
    tp.defaultexpr = defexpr;
    return tp;
}

static void tin_astparser_parseparameters(TinAstParser* prs, TinAstParamList* parameters)
{
    bool haddefault;
    size_t arglength;
    const char* argname;
    TinAstExpression* defexpr;
    haddefault = false;
    while(!tin_astparser_check(prs, TINTOK_PARENCLOSE))
    {
        // Vararg ...
        if(tin_astparser_match(prs, TINTOK_TRIPLEDOT))
        {
            tin_paramlist_push(prs->state, parameters, tin_astparser_makeparam("...", 3, NULL));
            return;
        }
        tin_astparser_consume(prs, TINTOK_IDENT, "argument name");
        argname = prs->previous.start;
        arglength = prs->previous.length;
        defexpr = NULL;
        if(tin_astparser_match(prs, TINTOK_ASSIGN))
        {
            haddefault = true;
            defexpr = tin_astparser_parseexpression(prs, true);
        }
        else if(haddefault)
        {
            tin_astparser_raiseerror(prs, "default arguments must always be in the end of the argument list.");
        }
        tin_paramlist_push(prs->state, parameters, tin_astparser_makeparam(argname, arglength, defexpr));
        if(!tin_astparser_match(prs, TINTOK_COMMA))
        {
            break;
        }
    }
}

/*
* this is extremely not working at all.
*/
static TinAstExpression* tin_astparser_rulegroupingorlambda(TinAstParser* prs, bool canassign)
{
    bool stop;
    bool haddefault;
    bool hadvararg;
    bool had_array;
    bool hadarrow;
    size_t line;
    size_t firstarglength;
    size_t arglength;
    const char* start;
    const char* argname;
    const char* firstargstart;
    TinAstExpression* expression;
    TinAstExpression* defexpr;
    TinAstScanner* scanner;
    (void)canassign;
    (void)hadarrow;
    (void)had_array;
    hadarrow = false;
    if(tin_astparser_match(prs, TINTOK_PARENCLOSE))
    {
        tin_astparser_consume(prs, TINTOK_ARROW, "=> after lambda arguments");
        return tin_astparser_parselambda(prs, tin_ast_make_lambdaexpr(prs->state, prs->previous.line));
    }
    start = prs->previous.start;
    line = prs->previous.line;
    if(tin_astparser_match(prs, TINTOK_IDENT) || tin_astparser_match(prs, TINTOK_TRIPLEDOT))
    {
        TinState* state = prs->state;
        firstargstart = prs->previous.start;
        firstarglength = prs->previous.length;
        if(tin_astparser_match(prs, TINTOK_COMMA) || (tin_astparser_match(prs, TINTOK_PARENCLOSE) && tin_astparser_match(prs, TINTOK_ARROW)))
        {
            had_array = prs->previous.type == TINTOK_ARROW;
            hadvararg= prs->previous.type == TINTOK_TRIPLEDOT;
            // This is a lambda
            TinAstFunctionExpr* lambda = tin_ast_make_lambdaexpr(state, line);
            TinAstExpression* defvalue = NULL;
            haddefault = tin_astparser_match(prs, TINTOK_ASSIGN);
            if(haddefault)
            {
                defvalue = tin_astparser_parseexpression(prs, true);
            }
            tin_paramlist_push(state, &lambda->parameters, tin_astparser_makeparam(firstargstart, firstarglength, defvalue));
            if(!hadvararg && prs->previous.type == TINTOK_COMMA)
            {
                do
                {
                    stop = false;
                    if(tin_astparser_match(prs, TINTOK_TRIPLEDOT))
                    {
                        stop = true;
                    }
                    else
                    {
                        tin_astparser_consume(prs, TINTOK_IDENT, "argument name");
                    }

                    argname = prs->previous.start;
                    arglength = prs->previous.length;
                    defexpr = NULL;
                    if(tin_astparser_match(prs, TINTOK_ASSIGN))
                    {
                        defexpr = tin_astparser_parseexpression(prs, true);
                        haddefault = true;
                    }
                    else if(haddefault)
                    {
                        tin_astparser_raiseerror(prs, "default arguments must always be in the end of the argument list.");
                    }
                    tin_paramlist_push(state, &lambda->parameters, tin_astparser_makeparam(argname, arglength, defexpr));
                    if(stop)
                    {
                        break;
                    }
                } while(tin_astparser_match(prs, TINTOK_COMMA));
            }
            #if 0
            if(!hadarrow)
            {
                tin_astparser_consume(prs, TINTOK_PARENCLOSE, "')' after lambda parameters");
                tin_astparser_consume(prs, TINTOK_ARROW, "=> after lambda parameters");
            }
            #endif
            return tin_astparser_parselambda(prs, lambda);
        }
        else
        {
            // Ouch, this was a grouping with a single identifier
            scanner = state->scanner;
            scanner->current = start;
            scanner->line = line;
            prs->current = tin_astlex_scantoken(scanner);
            tin_astparser_advance(prs);
        }
    }
    expression = tin_astparser_parseexpression(prs, true);
    tin_astparser_consume(prs, TINTOK_PARENCLOSE, "')' after grouping expression");
    return expression;
}

static TinAstExpression* tin_astparser_rulecall(TinAstParser* prs, TinAstExpression* prev, bool canassign)
{
    int pplen;
    /*
    int prevlen;
    int currlen;
    */
    const char* ppstr;
    /*
    const char* prevstr;
    const char* currstr;
    */
    TinString* ts;
    TinAstExpression* argexpr;
    TinAstVarExpr* varex;
    TinAstCallExpr* callexpr;
    (void)canassign;
    ts = NULL;
    callexpr = tin_ast_make_callexpr(prs->state, prs->previous.line, prev, ts);

    if(ts == NULL)
    {
        pplen = prs->prevprev.length;
        ppstr = prs->prevprev.start;
        /*
        prevlen = prs->previous.length;
        prevstr = prs->previous.start;
        currlen = prs->current.length;
        currstr = prs->current.start;
        fprintf(stderr, "call name: prevprev: '%.*s' previous: '%.*s' current: '%.*s'\n", pplen, ppstr, prevlen, prevstr, currlen, currstr);
        */
        ts = tin_string_copy(prs->state, ppstr, pplen);
    }
    while(!tin_astparser_check(prs, TINTOK_PARENCLOSE))
    {
        argexpr = tin_astparser_parseexpression(prs, true);

        tin_exprlist_push(prs->state, &callexpr->args, argexpr);
        if(!tin_astparser_match(prs, TINTOK_COMMA))
        {
            break;
        }
        if(argexpr->type == TINEXPR_VAREXPR)
        {
            varex = (TinAstVarExpr*)argexpr;
            // Vararg ...
            if(varex->length == 3 && memcmp(varex->name, "...", 3) == 0)
            {
                break;
            }
        }
    }
    if(callexpr->args.count > 255)
    {
        tin_astparser_raiseerror(prs, "function cannot have more than 255 arguments, got %i", (int)callexpr->args.count);
    }
    tin_astparser_consume(prs, TINTOK_PARENCLOSE, "')' after arguments");
    callexpr->name = ts;
    return (TinAstExpression*)callexpr;
}

static TinAstExpression* tin_astparser_ruleunary(TinAstParser* prs, bool canassign)
{
    (void)canassign;
    size_t line;
    TinAstExpression* unexpr;
    TinAstTokType op;
    op = prs->previous.type;
    line = prs->previous.line;
    unexpr = tin_astparser_parseprecedence(prs, TINPREC_UNARY, true, true);
    return (TinAstExpression*)tin_ast_make_unaryexpr(prs->state, line, unexpr, op);
}

static TinAstExpression* tin_astparser_rulebinary(TinAstParser* prs, TinAstExpression* prev, bool canassign)
{
    (void)canassign;
    bool invert;
    size_t line;
    TinAstParseRule* rule;
    TinAstExpression* expression;
    TinAstTokType op;
    invert = prs->previous.type == TINTOK_BANG;
    if(invert)
    {
        tin_astparser_consume(prs, TINTOK_KWIS, "'is' after '!'");
    }
    op = prs->previous.type;
    line = prs->previous.line;
    rule = tin_astparser_getrule(op);
    expression = tin_astparser_parseprecedence(prs, (TinAstPrecedence)(rule->precedence + 1), true, true);
    expression = (TinAstExpression*)tin_ast_make_binaryexpr(prs->state, line, prev, expression, op);
    if(invert)
    {
        expression = (TinAstExpression*)tin_ast_make_unaryexpr(prs->state, line, expression, TINTOK_BANG);
    }
    return expression;
}

static TinAstExpression* tin_astparser_ruleand(TinAstParser* prs, TinAstExpression* prev, bool canassign)
{
    (void)canassign;
    size_t line;
    TinAstTokType op;
    op = prs->previous.type;
    line = prs->previous.line;
    return (TinAstExpression*)tin_ast_make_binaryexpr(prs->state, line, prev, tin_astparser_parseprecedence(prs, TINPREC_AND, true, true), op);
}

static TinAstExpression* tin_astparser_ruleor(TinAstParser* prs, TinAstExpression* prev, bool canassign)
{
    (void)canassign;
    size_t line;
    TinAstTokType op;
    op = prs->previous.type;
    line = prs->previous.line;
    return (TinAstExpression*)tin_ast_make_binaryexpr(prs->state, line, prev, tin_astparser_parseprecedence(prs, TINPREC_OR, true, true), op);
}

static TinAstExpression* tin_astparser_rulenull_filter(TinAstParser* prs, TinAstExpression* prev, bool canassign)
{
    (void)canassign;
    size_t line;
    TinAstTokType op;
    op = prs->previous.type;
    line = prs->previous.line;
    return (TinAstExpression*)tin_ast_make_binaryexpr(prs->state, line, prev, tin_astparser_parseprecedence(prs, TINPREC_NULL, true, true), op);
}

static TinAstTokType tin_astparser_convertcompoundop(TinAstTokType op)
{
    switch(op)
    {
        case TINTOK_PLUSEQUAL:
            {
                return TINTOK_PLUS;
            }
            break;
        case TINTOK_MINUSEQUAL:
            {
                return TINTOK_MINUS;
            }
            break;
        case TINTOK_STAREQUAL:
            {
                return TINTOK_STAR;
            }
            break;
        case TINTOK_SLASHEQUAL:
            {
                return TINTOK_SLASH;
            }
            break;
        case TINTOK_SHARPEQUAL:
            {
                return TINTOK_SHARP;
            }
            break;
        case TINTOK_PERCENTEQUAL:
            {
                return TINTOK_PERCENT;
            }
            break;
        case TINTOK_CARETEQUAL:
            {
                return TINTOK_CARET;
            }
            break;
        case TINTOK_ASSIGNEQUAL:
            {
                return TINTOK_BAR;
            }
            break;
        case TINTOK_AMPERSANDEQUAL:
            {
                return TINTOK_AMPERSAND;
            }
            break;
        case TINTOK_DOUBLEPLUS:
            {
                return TINTOK_PLUS;
            }
            break;
        case TINTOK_DOUBLEMINUS:
            {
                return TINTOK_MINUS;
            }
            break;
        default:
            {
                assert(!"missing or invalid instruction for operator in convertcompound");
            }
            break;
    }
    return (TinAstTokType)-1;
}

static TinAstExpression* tin_astparser_rulecompound(TinAstParser* prs, TinAstExpression* prev, bool canassign)
{
    (void)canassign;
    size_t line;
    TinAstBinaryExpr* binary;
    TinAstExpression* expression;
    TinAstParseRule* rule;
    TinAstTokType op;
    op = prs->previous.type;
    line = prs->previous.line;
    rule = tin_astparser_getrule(op);
    if(op == TINTOK_DOUBLEPLUS || op == TINTOK_DOUBLEMINUS)
    {
        expression = (TinAstExpression*)tin_ast_make_literalexpr(prs->state, line, tin_value_makefixednumber(prs->state, 1));
    }
    else
    {
        expression = tin_astparser_parseprecedence(prs, (TinAstPrecedence)(rule->precedence + 1), true, true);
    }
    binary = tin_ast_make_binaryexpr(prs->state, line, prev, expression, tin_astparser_convertcompoundop(op));
    // To make sure we don't free it twice
    binary->ignoreleft = true;
    return (TinAstExpression*)tin_ast_make_assignexpr(prs->state, line, prev, (TinAstExpression*)binary);
}

static TinAstExpression* tin_astparser_ruleliteral(TinAstParser* prs, bool canassign)
{
    (void)canassign;
    size_t line;
    line = prs->previous.line;
    switch(prs->previous.type)
    {
        case TINTOK_KWTRUE:
            {
                return (TinAstExpression*)tin_ast_make_literalexpr(prs->state, line, tin_value_makebool(prs->state, true));
            }
            break;
        case TINTOK_KWFALSE:
            {
                return (TinAstExpression*)tin_ast_make_literalexpr(prs->state, line, tin_value_makebool(prs->state, false));
            }
            break;
        case TINTOK_KWNULL:
            {
                return (TinAstExpression*)tin_ast_make_literalexpr(prs->state, line, tin_value_makenull(prs->state));
            }
            break;
        default:
            {
                assert(!"missing or invalid instruction for ruleliteral");
            }
            break;
    }
    return NULL;
}

static TinAstExpression* tin_astparser_rulestring(TinAstParser* prs, bool canassign)
{
    (void)canassign;
    TinAstExpression* expression;
    expression = (TinAstExpression*)tin_ast_make_literalexpr(prs->state, prs->previous.line, prs->previous.value);
    if(tin_astparser_match(prs, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(prs, expression, canassign);
    }
    return expression;
}

static TinAstExpression* tin_astparser_ruleinterpolation(TinAstParser* prs, bool canassign)
{
    TinAstStrInterExpr* expression;
    (void)canassign;
    expression = tin_ast_make_strinterpolexpr(prs->state, prs->previous.line);
    do
    {
        if(tin_string_getlength(tin_value_asstring(prs->previous.value)) > 0)
        {
            tin_exprlist_push(
            prs->state, &expression->expressions,
            (TinAstExpression*)tin_ast_make_literalexpr(prs->state, prs->previous.line, prs->previous.value));
        }
        tin_exprlist_push(prs->state, &expression->expressions, tin_astparser_parseexpression(prs, true));
    } while(tin_astparser_match(prs, TINTOK_STRINTERPOL));
    tin_astparser_consume(prs, TINTOK_STRING, "end of interpolation");
    if(tin_string_getlength(tin_value_asstring(prs->previous.value)) > 0)
    {
        tin_exprlist_push(
        prs->state, &expression->expressions,
        (TinAstExpression*)tin_ast_make_literalexpr(prs->state, prs->previous.line, prs->previous.value));
    }
    if(tin_astparser_match(prs, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(prs, (TinAstExpression*)expression, canassign);
    }
    return (TinAstExpression*)expression;
}

static TinAstExpression* tin_astparser_ruleobject(TinAstParser* prs, bool canassign)
{
    TinString* ts;
    TinValue tv;
    TinAstExpression* expr;
    TinAstObjectExpr* object;
    (void)canassign;
    (void)ts;
    object = tin_ast_make_objectexpr(prs->state, prs->previous.line);
    tin_astparser_ignorenewlines(prs, true);
    while(!tin_astparser_check(prs, TINTOK_BRACECLOSE))
    {
        tin_astparser_ignorenewlines(prs, true);
        if(tin_astparser_check(prs, TINTOK_IDENT))
        {
            tin_astparser_raiseerror(prs, "cannot use bare names (for now)");
            //tin_astparser_consume(prs, TINTOK_IDENT, "key string after '{'");
            
            //tin_vallist_push(prs->state, &object->keys, tin_value_fromobject(tin_string_copy(prs->state, prs->previous.start, prs->previous.length)));
            expr = tin_astparser_parseexpression(prs, true);
            tin_exprlist_push(prs->state, &object->keys, expr);

        }
        else if(tin_astparser_check(prs, TINTOK_STRING))
        {
            expr = tin_astparser_parseexpression(prs, true);
            tv = prs->previous.value;
            ts = tin_value_asstring(tv);
            //tin_vallist_push(prs->state, &object->keys, tin_value_fromobject(tin_string_copy(prs->state, ts->data, tin_string_getlength(ts))));
            //tin_vallist_push(prs->state, &object->keys, tv);
            tin_exprlist_push(prs->state, &object->keys, expr);

            //tin_ast_destroyexpression(prs->state, expr);
        }
        
        else
        {
            tin_astparser_raiseerror(prs, "expect identifier or string as object key");
        }
    
        tin_astparser_ignorenewlines(prs, true);
        tin_astparser_consume(prs, TINTOK_COLON, "':' after key string");
        tin_astparser_ignorenewlines(prs, true);
        tin_exprlist_push(prs->state, &object->values, tin_astparser_parseexpression(prs, true));
        if(!tin_astparser_match(prs, TINTOK_COMMA))
        {
            break;
        }
    }
    tin_astparser_ignorenewlines(prs, true);
    tin_astparser_consume(prs, TINTOK_BRACECLOSE, "'}' after object");
    return (TinAstExpression*)object;
}

static TinAstExpression* tin_astparser_rulevarexprbase(TinAstParser* prs, bool canassign, bool isnew)
{
    (void)canassign;
    bool hadargs;
    TinString* ts;
    TinAstCallExpr* callex;
    TinAstExpression* expression;
    expression = (TinAstExpression*)tin_ast_make_varexpr(prs->state, prs->previous.line, prs->previous.start, prs->previous.length);
    if(isnew)
    {
        hadargs = tin_astparser_check(prs, TINTOK_PARENOPEN);
        callex = NULL;
        if(hadargs)
        {
            tin_astparser_advance(prs);
            callex = (TinAstCallExpr*)tin_astparser_rulecall(prs, expression, false);
        }
        if(tin_astparser_match(prs, TINTOK_BRACEOPEN))
        {
            if(callex == NULL)
            {
                ts =  tin_string_copy(prs->state, prs->previous.start, prs->previous.length);
                callex = tin_ast_make_callexpr(prs->state, expression->line, expression, ts);
            }
            callex->init = tin_astparser_ruleobject(prs, false);
        }
        else if(!hadargs)
        {
            tin_astparser_raiseatcurrent(prs, "expected %s, got '%.*s'", "argument list for instance creation",
                             prs->previous.length, prs->previous.start);
        }
        return (TinAstExpression*)callex;
    }
    if(tin_astparser_match(prs, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(prs, expression, canassign);
    }
    if(canassign && tin_astparser_match(prs, TINTOK_ASSIGN))
    {
        return (TinAstExpression*)tin_ast_make_assignexpr(prs->state, prs->previous.line, expression,
                                                            tin_astparser_parseexpression(prs, true));
    }
    return expression;
}

static TinAstExpression* tin_astparser_rulevarexpr(TinAstParser* prs, bool canassign)
{
    return tin_astparser_rulevarexprbase(prs, canassign, false);
}

static TinAstExpression* tin_astparser_rulenewexpr(TinAstParser* prs, bool canassign)
{
    (void)canassign;
    tin_astparser_consume(prs, TINTOK_IDENT, "class name after 'new'");
    return tin_astparser_rulevarexprbase(prs, false, true);
}

static TinAstExpression* tin_astparser_ruledot(TinAstParser* prs, TinAstExpression* previous, bool canassign)
{
    (void)canassign;
    bool ignored;
    size_t line;
    size_t length;
    const char* name;
    TinAstExpression* expression;
    line = prs->previous.line;
    ignored = prs->previous.type == TINTOK_SMALLARROW;
    if(!(tin_astparser_match(prs, TINTOK_KWCLASS) || tin_astparser_match(prs, TINTOK_KWSUPER)))
    {// class and super are allowed field names
        tin_astparser_consume(prs, TINTOK_IDENT, ignored ? "propety name after '->'" : "property name after '.'");
    }
    name = prs->previous.start;
    length = prs->previous.length;
    if(!ignored && canassign && tin_astparser_match(prs, TINTOK_ASSIGN))
    {
        return (TinAstExpression*)tin_ast_make_setexpr(prs->state, line, previous, name, length, tin_astparser_parseexpression(prs, true));
    }
    expression = (TinAstExpression*)tin_ast_make_getexpr(prs->state, line, previous, name, length, false, ignored);
    if(!ignored && tin_astparser_match(prs, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(prs, expression, canassign);
    }
    return expression;
}

static TinAstExpression* tin_astparser_rulerange(TinAstParser* prs, TinAstExpression* previous, bool canassign)
{
    (void)canassign;
    size_t line;
    line = prs->previous.line;
    return (TinAstExpression*)tin_ast_make_rangeexpr(prs->state, line, previous, tin_astparser_parseexpression(prs, true));
}

static TinAstExpression* tin_astparser_ruleternary(TinAstParser* prs, TinAstExpression* previous, bool canassign)
{
    (void)canassign;
    bool ignored;
    size_t line;
    TinAstExpression* ifbranch;
    TinAstExpression* elsebranch;
    line = prs->previous.line;
    if(tin_astparser_match(prs, TINTOK_DOT) || tin_astparser_match(prs, TINTOK_SMALLARROW))
    {
        ignored = prs->previous.type == TINTOK_SMALLARROW;
        tin_astparser_consume(prs, TINTOK_IDENT, ignored ? "property name after '->'" : "property name after '.'");
        return (TinAstExpression*)tin_ast_make_getexpr(prs->state, line, previous, prs->previous.start,
                                                         prs->previous.length, true, ignored);
    }
    ifbranch = tin_astparser_parseexpression(prs, true);
    tin_astparser_consume(prs, TINTOK_COLON, "':' after expression");
    elsebranch = tin_astparser_parseexpression(prs, true);
    return (TinAstExpression*)tin_ast_make_ternaryexpr(prs->state, line, previous, ifbranch, elsebranch);
}

static TinAstExpression* tin_astparser_rulearray(TinAstParser* prs, bool canassign)
{
    (void)canassign;
    TinAstExpression* expr;
    TinAstArrayExpr* array;
    array = tin_ast_make_arrayexpr(prs->state, prs->previous.line);
    tin_astparser_ignorenewlines(prs, true);
    while(!tin_astparser_check(prs, TINTOK_BRACKETCLOSE))
    {
        expr = NULL;
        tin_astparser_ignorenewlines(prs, true);
        #if 1
            expr = tin_astparser_parseexpression(prs, true);
        #else
            if(tin_astparser_check(prs, TINTOK_COMMA))
            {
                //tin_astparser_rulenull_filter(TinAstParser *prs, TinAstExpression *prev, _Bool canassign)
                expr = tin_astparser_rulenull_filter(prs, NULL, false);
            }
            else
            {
                expr = tin_astparser_parseexpression(prs, true);
            }
        #endif
        tin_exprlist_push(prs->state, &array->values, expr);
        if(!tin_astparser_match(prs, TINTOK_COMMA))
        {
            break;
        }
        tin_astparser_ignorenewlines(prs, true);
    }
    tin_astparser_ignorenewlines(prs, true);
    tin_astparser_consume(prs, TINTOK_BRACKETCLOSE, "']' after array");
    if(tin_astparser_match(prs, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(prs, (TinAstExpression*)array, canassign);
    }
    return (TinAstExpression*)array;
}

static TinAstExpression* tin_astparser_rulesubscript(TinAstParser* prs, TinAstExpression* previous, bool canassign)
{
    size_t line;
    TinAstExpression* index;
    TinAstExpression* expression;
    line = prs->previous.line;
    index = tin_astparser_parseexpression(prs, true);
    tin_astparser_consume(prs, TINTOK_BRACKETCLOSE, "']' after subscript");
    expression = (TinAstExpression*)tin_ast_make_subscriptexpr(prs->state, line, previous, index);
    if(tin_astparser_match(prs, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(prs, expression, canassign);
    }
    else if(canassign && tin_astparser_match(prs, TINTOK_ASSIGN))
    {
        return (TinAstExpression*)tin_ast_make_assignexpr(prs->state, prs->previous.line, expression, tin_astparser_parseexpression(prs, true));
    }
    return expression;
}

static TinAstExpression* tin_astparser_rulethis(TinAstParser* prs, bool canassign)
{
    TinAstExpression* expression;
    expression = (TinAstExpression*)tin_ast_make_thisexpr(prs->state, prs->previous.line);
    if(tin_astparser_match(prs, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(prs, expression, canassign);
    }
    return expression;
}

static TinAstExpression* tin_astparser_rulesuper(TinAstParser* prs, bool canassign)
{
    (void)canassign;
    bool ignoring;
    size_t line;
    TinAstExpression* expression;
    line = prs->previous.line;
    if(!(tin_astparser_match(prs, TINTOK_DOT) || tin_astparser_match(prs, TINTOK_SMALLARROW)))
    {
        expression = (TinAstExpression*)tin_ast_make_superexpr(
        prs->state, line, tin_string_copy(prs->state, TIN_VALUE_CTORNAME, strlen(TIN_VALUE_CTORNAME)), false);
        tin_astparser_consume(prs, TINTOK_PARENOPEN, "'(' after 'super'");
        return tin_astparser_rulecall(prs, expression, false);
    }
    ignoring = prs->previous.type == TINTOK_SMALLARROW;
    tin_astparser_consume(prs, TINTOK_IDENT, ignoring ? "super method name after '->'" : "super method name after '.'");
    expression = (TinAstExpression*)tin_ast_make_superexpr(
    prs->state, line, tin_string_copy(prs->state, prs->previous.start, prs->previous.length), ignoring);
    if(tin_astparser_match(prs, TINTOK_PARENOPEN))
    {
        return tin_astparser_rulecall(prs, expression, false);
    }
    return expression;
}

static TinAstExpression *tin_astparser_rulenothing(TinAstParser *prs, bool canassign)
{
    (void)prs;
    (void)canassign;
    return NULL;
}

static TinAstExpression* tin_astparser_rulereference(TinAstParser* prs, bool canassign)
{
    size_t line;
    TinAstRefExpr* expression;
    (void)canassign;
    line = prs->previous.line;
    tin_astparser_ignorenewlines(prs, true);
    expression = tin_ast_make_referenceexpr(prs->state, line, tin_astparser_parseprecedence(prs, TINPREC_CALL, false, true));
    if(tin_astparser_match(prs, TINTOK_ASSIGN))
    {
        return (TinAstExpression*)tin_ast_make_assignexpr(prs->state, line, (TinAstExpression*)expression, tin_astparser_parseexpression(prs, true));
    }
    return (TinAstExpression*)expression;
}



static TinAstExpression* tin_astparser_parsestatement(TinAstParser* prs)
{
    TinAstExpression* expression;
    tin_astparser_ignorenewlines(prs, true);
    if(setjmp(prs_jmpbuffer))
    {
        return NULL;
    }
    if(tin_astparser_match(prs, TINTOK_KWVAR) || tin_astparser_match(prs, TINTOK_KWCONST))
    {
        return tin_astparser_parsevar_declaration(prs, true);
    }
    else if(tin_astparser_match(prs, TINTOK_KWIF))
    {
        return tin_astparser_parseif(prs);
    }
    else if(tin_astparser_match(prs, TINTOK_KWFOR))
    {
        return tin_astparser_parsefor(prs);
    }
    else if(tin_astparser_match(prs, TINTOK_KWWHILE))
    {
        return tin_astparser_parsewhile(prs);
    }
    else if(tin_astparser_match(prs, TINTOK_KWCONTINUE))
    {
        return (TinAstExpression*)tin_ast_make_continueexpr(prs->state, prs->previous.line);
    }
    else if(tin_astparser_match(prs, TINTOK_KWBREAK))
    {
        return (TinAstExpression*)tin_ast_make_breakexpr(prs->state, prs->previous.line);
    }
    else if(tin_astparser_match(prs, TINTOK_KWFUNCTION) || tin_astparser_match(prs, TINTOK_KWEXPORT))
    {
        return tin_astparser_rulefunction(prs, false);
    }
    else if(tin_astparser_match(prs, TINTOK_KWRETURN))
    {
        return tin_astparser_parsereturn(prs);
    }
    else if(tin_astparser_match(prs, TINTOK_BRACEOPEN))
    {
        return tin_astparser_parseblock(prs);
    }
    expression = tin_astparser_parseexpression(prs, true);
    if(expression == NULL)
    {
        return NULL;
    }
    return (TinAstExpression*)tin_ast_make_exprstmt(prs->state, prs->previous.line, expression);
}

static TinAstExpression* tin_astparser_parseexpression(TinAstParser* prs, bool ignsemi)
{
    tin_astparser_ignorenewlines(prs, ignsemi);
    return tin_astparser_parseprecedence(prs, TINPREC_ASSIGNMENT, true, ignsemi);
}

static TinAstExpression* tin_astparser_parsevar_declaration(TinAstParser* prs, bool ignsemi)
{
    bool constant;
    size_t line;
    size_t length;
    const char* name;
    TinAstExpression* init;
    constant = prs->previous.type == TINTOK_KWCONST;
    line = prs->previous.line;
    tin_astparser_consume(prs, TINTOK_IDENT, "variable name");
    name = prs->previous.start;
    length = prs->previous.length;
    init = NULL;
    if(tin_astparser_match(prs, TINTOK_ASSIGN))
    {
        init = tin_astparser_parseexpression(prs, ignsemi);
    }
    return (TinAstExpression*)tin_ast_make_assignvarexpr(prs->state, line, name, length, init, constant);
}

static TinAstExpression* tin_astparser_parseif(TinAstParser* prs)
{
    size_t line;
    bool invert;
    bool hadparen;
    TinAstExpression* condition;
    TinAstExpression* ifbranch;
    TinAstExprList* elseifconds;
    TinAstExprList* elseifbranches;
    TinAstExpression* elsebranch;
    TinAstExpression* e;
    line = prs->previous.line;
    invert = tin_astparser_match(prs, TINTOK_BANG);
    hadparen = tin_astparser_match(prs, TINTOK_PARENOPEN);
    condition = tin_astparser_parseexpression(prs, true);
    if(hadparen)
    {
        tin_astparser_consume(prs, TINTOK_PARENCLOSE, "')'");
    }
    if(invert)
    {
        condition = (TinAstExpression*)tin_ast_make_unaryexpr(prs->state, condition->line, condition, TINTOK_BANG);
    }
    tin_astparser_ignorenewlines(prs, true);
    ifbranch = tin_astparser_parsestatement(prs);
    elseifconds = NULL;
    elseifbranches = NULL;
    elsebranch = NULL;
    tin_astparser_ignorenewlines(prs, true);
    while(tin_astparser_match(prs, TINTOK_KWELSE))
    {
        // else if
        tin_astparser_ignorenewlines(prs, true);
        if(tin_astparser_match(prs, TINTOK_KWIF))
        {
            if(elseifconds == NULL)
            {
                elseifconds = tin_ast_allocexprlist(prs->state);
                elseifbranches = tin_ast_allocate_stmtlist(prs->state);
            }
            invert = tin_astparser_match(prs, TINTOK_BANG);
            hadparen = tin_astparser_match(prs, TINTOK_PARENOPEN);
            tin_astparser_ignorenewlines(prs, true);
            e = tin_astparser_parseexpression(prs, true);
            if(hadparen)
            {
                tin_astparser_consume(prs, TINTOK_PARENCLOSE, "')'");
            }
            tin_astparser_ignorenewlines(prs, true);
            if(invert)
            {
                e = (TinAstExpression*)tin_ast_make_unaryexpr(prs->state, condition->line, e, TINTOK_BANG);
            }
            tin_exprlist_push(prs->state, elseifconds, e);
            tin_exprlist_push(prs->state, elseifbranches, tin_astparser_parsestatement(prs));
            continue;
        }
        // else
        if(elsebranch != NULL)
        {
            tin_astparser_raiseerror(prs, "if-statement can have only one else-branch");
        }
        tin_astparser_ignorenewlines(prs, true);
        elsebranch = tin_astparser_parsestatement(prs);
    }
    return (TinAstExpression*)tin_ast_make_ifexpr(prs->state, line, condition, ifbranch, elsebranch, elseifconds, elseifbranches);
}

static TinAstExpression* tin_astparser_parsefor(TinAstParser* prs)
{
    bool cstyle;
    bool hadparen;
    size_t line;
    TinAstExpression* condition;
    TinAstExpression* increment;
    TinAstExpression* var;
    TinAstExpression* init;
    line= prs->previous.line;
    hadparen = tin_astparser_match(prs, TINTOK_PARENOPEN);
    var = NULL;
    init = NULL;
    if(!tin_astparser_check(prs, TINTOK_SEMICOLON))
    {
        if(tin_astparser_match(prs, TINTOK_KWVAR))
        {
            var = tin_astparser_parsevar_declaration(prs, false);
        }
        else
        {
            init = tin_astparser_parseexpression(prs, false);
        }
    }
    cstyle = !tin_astparser_match(prs, TINTOK_KWIN);
    condition= NULL;
    increment = NULL;
    if(cstyle)
    {
        tin_astparser_consume(prs, TINTOK_SEMICOLON, "';'");
        condition = NULL;
    
        if(!tin_astparser_check(prs, TINTOK_SEMICOLON))
        {
            condition = tin_astparser_parseexpression(prs, false);
        }
        tin_astparser_consume(prs, TINTOK_SEMICOLON, "';'");
        increment = NULL;
        if(!tin_astparser_check(prs, TINTOK_PARENCLOSE))
        {
            increment = tin_astparser_parseexpression(prs, false);
        }
    }
    else
    {
        condition = tin_astparser_parseexpression(prs, true);
        if(var == NULL)
        {
            tin_astparser_raiseerror(prs, "for-loops using in-iteration must declare a new variable");
        }
    }
    if(hadparen)
    {
        tin_astparser_consume(prs, TINTOK_PARENCLOSE, "')'");
    }
    tin_astparser_ignorenewlines(prs, true);
    return (TinAstExpression*)tin_ast_make_forexpr(prs->state, line, init, var, condition, increment,
                                                   tin_astparser_parsestatement(prs), cstyle);
}

static TinAstExpression* tin_astparser_parsewhile(TinAstParser* prs)
{
    bool hadparen;
    size_t line;
    TinAstExpression* body;
    line = prs->previous.line;
    hadparen = tin_astparser_match(prs, TINTOK_PARENOPEN);
    TinAstExpression* condition = tin_astparser_parseexpression(prs, true);
    if(hadparen)
    {
        tin_astparser_consume(prs, TINTOK_PARENCLOSE, "')'");
    }
    tin_astparser_ignorenewlines(prs, true);
    body = tin_astparser_parsestatement(prs);
    return (TinAstExpression*)tin_ast_make_whileexpr(prs->state, line, condition, body);
}

static TinAstExpression* tin_astparser_rulefunction(TinAstParser* prs, bool canassign)
{
    bool isexport;
    bool islambda;
    size_t line;
    size_t fnamelen;
    const char* fnamestr;
    TinAstCompiler compiler;
    TinAstFunctionExpr* function;
    TinAstFunctionExpr* lambda;
    TinAstSetExpr* to;
    islambda = canassign;
    isexport = prs->previous.type == TINTOK_KWEXPORT;
    fnamestr = "<anonymous>";
    fnamelen = strlen(fnamestr);
    if(isexport)
    {
        tin_astparser_consume(prs, TINTOK_KWFUNCTION, "'function' after 'export'");
    }
    line = prs->previous.line;
    if(tin_astparser_check(prs, TINTOK_IDENT))
    {
        tin_astparser_consume(prs, TINTOK_IDENT, "function name");
        fnamestr = prs->previous.start;
        fnamelen = prs->previous.length;
    }
    if(tin_astparser_match(prs, TINTOK_DOT) || islambda)
    //if(tin_astparser_match(prs, TINTOK_DOT))
    {
        to = NULL;
        if(tin_astparser_check(prs, TINTOK_IDENT))
        {
            tin_astparser_consume(prs, TINTOK_IDENT, "function name");
        }
        lambda = tin_ast_make_lambdaexpr(prs->state, line);
        //if(islambda)
        /*
        {
            to = tin_ast_make_setexpr(
                prs->state,
                line,
                (TinAstExpression*)tin_ast_make_varexpr(prs->state, line, fnamestr, fnamelen),
                prs->previous.start,
                prs->previous.length,
                (TinAstExpression*)lambda
            );
        }
        */
        tin_astparser_consume(prs, TINTOK_PARENOPEN, "'(' after function name");
        tin_astparser_initcompiler(prs, &compiler);
        tin_astparser_beginscope(prs);
        tin_astparser_parseparameters(prs, &lambda->parameters);
        if(lambda->parameters.count > 255)
        {
            tin_astparser_raiseerror(prs, "function cannot have more than 255 arguments, got %i", (int)lambda->parameters.count);
        }
        tin_astparser_consume(prs, TINTOK_PARENCLOSE, "')' after function arguments");
        tin_astparser_ignorenewlines(prs, true);
        lambda->body = tin_astparser_parsestatement(prs);
        tin_astparser_endscope(prs);
        tin_astparser_endcompiler(prs, &compiler);
        if(islambda)
        {
            return (TinAstExpression*)lambda;
        }
        return (TinAstExpression*)tin_ast_make_exprstmt(prs->state, line, (TinAstExpression*)to);
    }
    function = tin_ast_make_funcexpr(prs->state, line, fnamestr, fnamelen);
    function->exported = isexport;
    tin_astparser_consume(prs, TINTOK_PARENOPEN, "'(' after function name");
    tin_astparser_initcompiler(prs, &compiler);
    tin_astparser_beginscope(prs);
    tin_astparser_parseparameters(prs, &function->parameters);
    if(function->parameters.count > 255)
    {
        tin_astparser_raiseerror(prs, "function cannot have more than 255 arguments, got %i", (int)function->parameters.count);
    }
    tin_astparser_consume(prs, TINTOK_PARENCLOSE, "')' after function arguments");
    function->body = tin_astparser_parsestatement(prs);
    tin_astparser_endscope(prs);
    tin_astparser_endcompiler(prs, &compiler);
    return (TinAstExpression*)function;
}

static TinAstExpression* tin_astparser_parsereturn(TinAstParser* prs)
{
    size_t line;
    TinAstExpression* expression;
    line = prs->previous.line;
    expression = NULL;
    if(!tin_astparser_check(prs, TINTOK_NEWLINE) && !tin_astparser_check(prs, TINTOK_BRACECLOSE))
    {
        expression = tin_astparser_parseexpression(prs, true);
    }
    return (TinAstExpression*)tin_ast_make_returnexpr(prs->state, line, expression);
}

static TinAstExpression* tin_astparser_parsefield(TinAstParser* prs, TinString* name, bool isstatic)
{
    size_t line;
    TinAstExpression* getter;
    TinAstExpression* setter;
    line = prs->previous.line;
    getter = NULL;
    setter = NULL;
    if(tin_astparser_match(prs, TINTOK_ARROW))
    {
        getter = tin_astparser_parsestatement(prs);
    }
    else
    {
        tin_astparser_match(prs, TINTOK_BRACEOPEN);// Will be TINTOK_BRACEOPEN, otherwise this method won't be called
        tin_astparser_ignorenewlines(prs, true);
        if(tin_astparser_match(prs, TINTOK_KWGET))
        {
            tin_astparser_match(prs, TINTOK_ARROW);// Ignore it if it's present
            getter = tin_astparser_parsestatement(prs);
        }
        tin_astparser_ignorenewlines(prs, true);
        if(tin_astparser_match(prs, TINTOK_KWSET))
        {
            tin_astparser_match(prs, TINTOK_ARROW);// Ignore it if it's present
            setter = tin_astparser_parsestatement(prs);
        }
        if(getter == NULL && setter == NULL)
        {
            tin_astparser_raiseerror(prs, "expected declaration of either getter or setter, got none");
        }
        tin_astparser_ignorenewlines(prs, true);
        tin_astparser_consume(prs, TINTOK_BRACECLOSE, "'}' after field declaration");
    }
    return (TinAstExpression*)tin_ast_make_fieldexpr(prs->state, line, name, getter, setter, isstatic);
}

static TinAstExpression* tin_astparser_parsemethod(TinAstParser* prs, bool isstatic)
{
    size_t i;
    TinAstCompiler compiler;
    TinAstMethodExpr* method;
    TinString* name;
    if(tin_astparser_match(prs, TINTOK_KWSTATIC))
    {
        isstatic = true;
    }
    name = NULL;
    if(tin_astparser_match(prs, TINTOK_KWOPERATOR))
    {
        if(isstatic)
        {
            tin_astparser_raiseerror(prs, "operator methods cannot be static or defined in static classes");
        }
        i = 0;
        while(operators[i] != TINTOK_EOF)
        {
            if(tin_astparser_match(prs, operators[i]))
            {
                break;
            }
            i++;
        }
        if(prs->previous.type == TINTOK_BRACKETOPEN)
        {
            tin_astparser_consume(prs, TINTOK_BRACKETCLOSE, "']' after '[' in op method declaration");
            name = tin_string_copy(prs->state, "[]", 2);
        }
        else
        {
            name = tin_string_copy(prs->state, prs->previous.start, prs->previous.length);
        }
    }
    else
    {
        tin_astparser_consume(prs, TINTOK_IDENT, "method name");
        name = tin_string_copy(prs->state, prs->previous.start, prs->previous.length);
        if(tin_astparser_check(prs, TINTOK_BRACEOPEN) || tin_astparser_check(prs, TINTOK_ARROW))
        {
            return tin_astparser_parsefield(prs, name, isstatic);
        }
    }
    method = tin_ast_make_methodexpr(prs->state, prs->previous.line, name, isstatic);
    tin_astparser_initcompiler(prs, &compiler);
    tin_astparser_beginscope(prs);
    tin_astparser_consume(prs, TINTOK_PARENOPEN, "'(' after method name");
    tin_astparser_parseparameters(prs, &method->parameters);
    if(method->parameters.count > 255)
    {
        tin_astparser_raiseerror(prs, "function cannot have more than 255 arguments, got %i", (int)method->parameters.count);
    }
    tin_astparser_consume(prs, TINTOK_PARENCLOSE, "')' after method arguments");
    method->body = tin_astparser_parsestatement(prs);
    tin_astparser_endscope(prs);
    tin_astparser_endcompiler(prs, &compiler);
    return (TinAstExpression*)method;
}

static TinAstExpression* tin_astparser_parseclass(TinAstParser* prs)
{
    bool finishedparsingfields;
    bool fieldisstatic;
    size_t line;
    bool isstatic;
    TinString* name;
    TinString* super;
    TinAstClassExpr* klass;
    TinAstExpression* var;
    TinAstExpression* method;
    if(setjmp(prs_jmpbuffer))
    {
        return NULL;
    }
    line = prs->previous.line;
    isstatic = prs->previous.type == TINTOK_KWSTATIC;
    if(isstatic)
    {
        tin_astparser_consume(prs, TINTOK_KWCLASS, "'class' after 'static'");
    }
    tin_astparser_consume(prs, TINTOK_IDENT, "class name after 'class'");
    name = tin_string_copy(prs->state, prs->previous.start, prs->previous.length);
    super = NULL;
    if(tin_astparser_match(prs, TINTOK_COLON))
    {
        tin_astparser_consume(prs, TINTOK_IDENT, "super class name after ':'");
        super = tin_string_copy(prs->state, prs->previous.start, prs->previous.length);
        if(super == name)
        {
            tin_astparser_raiseerror(prs, "class cannot inherit itself");
        }
    }
    klass = tin_ast_make_classexpr(prs->state, line, name, super);
    tin_astparser_ignorenewlines(prs, true);
    tin_astparser_consume(prs, TINTOK_BRACEOPEN, "'{' before class body");
    tin_astparser_ignorenewlines(prs, true);
    finishedparsingfields = false;
    while(!tin_astparser_check(prs, TINTOK_BRACECLOSE))
    {
        fieldisstatic = false;
        if(tin_astparser_match(prs, TINTOK_KWSTATIC))
        {
            fieldisstatic = true;
            if(tin_astparser_match(prs, TINTOK_KWVAR))
            {
                if(finishedparsingfields)
                {
                    tin_astparser_raiseerror(prs, "all static fields must be defined before the methods");
                }
                var = tin_astparser_parsevar_declaration(prs, true);
                if(var != NULL)
                {
                    tin_exprlist_push(prs->state, &klass->fields, var);
                }
                tin_astparser_ignorenewlines(prs, true);
                continue;
            }
            else
            {
                finishedparsingfields = true;
            }
        }
        method = tin_astparser_parsemethod(prs, isstatic || fieldisstatic);
        if(method != NULL)
        {
            tin_exprlist_push(prs->state, &klass->fields, method);
        }
        tin_astparser_ignorenewlines(prs, true);
    }
    tin_astparser_consume(prs, TINTOK_BRACECLOSE, "'}' after class body");
    return (TinAstExpression*)klass;
}

static void tin_astparser_sync(TinAstParser* prs)
{
    prs->panicmode = false;
    while(prs->current.type != TINTOK_EOF)
    {
        if(prs->previous.type == TINTOK_NEWLINE)
        {
            longjmp(prs_jmpbuffer, 1);
            return;
        }
        switch(prs->current.type)
        {
            case TINTOK_KWCLASS:
            case TINTOK_KWFUNCTION:
            case TINTOK_KWEXPORT:
            case TINTOK_KWVAR:
            case TINTOK_KWCONST:
            case TINTOK_KWFOR:
            case TINTOK_KWSTATIC:
            case TINTOK_KWIF:
            case TINTOK_KWWHILE:
            case TINTOK_KWRETURN:
            {
                longjmp(prs_jmpbuffer, 1);
                return;
            }
            default:
            {
                tin_astparser_advance(prs);
            }
        }
    }
}

static TinAstExpression* tin_astparser_parsedeclaration(TinAstParser* prs)
{
    TinAstExpression* statement;
    statement = NULL;
    if(tin_astparser_match(prs, TINTOK_KWCLASS) || tin_astparser_match(prs, TINTOK_KWSTATIC))
    {
        statement = tin_astparser_parseclass(prs);
    }
    else
    {
        statement = tin_astparser_parsestatement(prs);
    }
    return statement;
}

bool tin_astparser_parsesource(TinAstParser* prs, const char* filename, const char* source, size_t srclength, TinAstExprList* statements)
{
    TinAstCompiler compiler;
    TinAstExpression* statement;
    prs->haderror = false;
    prs->panicmode = false;
    tin_astlex_init(prs->state, prs->state->scanner, filename, source, srclength);
    tin_astparser_initcompiler(prs, &compiler);
    tin_astparser_advance(prs);
    tin_astparser_ignorenewlines(prs, true);
    if(!prs_is_at_end(prs))
    {
        do
        {
            statement = tin_astparser_parsedeclaration(prs);
            if(statement != NULL)
            {
                tin_exprlist_push(prs->state, statements, statement);
            }
            if(!tin_astparser_matchnewline(prs))
            {
                if(tin_astparser_match(prs, TINTOK_EOF))
                {
                    break;
                }
            }
        } while(!prs_is_at_end(prs));
    }
    return prs->haderror || prs->state->scanner->haderror;
}
