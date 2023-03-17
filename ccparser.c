
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
static void tin_astparser_sync(TinAstParser* parser);

static TinAstExpression *tin_astparser_parseblock(TinAstParser *parser);
static TinAstExpression *tin_astparser_parseprecedence(TinAstParser *parser, TinAstPrecedence precedence, bool err, bool ignsemi);
static TinAstExpression *tin_astparser_parselambda(TinAstParser *parser, TinAstFunctionExpr *lambda);
static void tin_astparser_parseparameters(TinAstParser *parser, TinAstParamList *parameters);
static TinAstExpression *tin_astparser_parseexpression(TinAstParser *parser, bool ignsemi);
static TinAstExpression *tin_astparser_parsevar_declaration(TinAstParser *parser, bool ignsemi);
static TinAstExpression *tin_astparser_parseif(TinAstParser *parser);
static TinAstExpression *tin_astparser_parsefor(TinAstParser *parser);
static TinAstExpression *tin_astparser_parsewhile(TinAstParser *parser);
static TinAstExpression *tin_astparser_parsereturn(TinAstParser *parser);
static TinAstExpression *tin_astparser_parsefield(TinAstParser *parser, TinString *name, bool isstatic);
static TinAstExpression *tin_astparser_parsemethod(TinAstParser *parser, bool isstatic);
static TinAstExpression *tin_astparser_parseclass(TinAstParser *parser);
static TinAstExpression *tin_astparser_parsestatement(TinAstParser *parser);
static TinAstExpression *tin_astparser_parsedeclaration(TinAstParser *parser);

static TinAstExpression *tin_astparser_rulenumber(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_rulegroupingorlambda(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_rulecall(TinAstParser *parser, TinAstExpression *prev, bool canassign);
static TinAstExpression *tin_astparser_ruleunary(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_rulebinary(TinAstParser *parser, TinAstExpression *prev, bool canassign);
static TinAstExpression *tin_astparser_ruleand(TinAstParser *parser, TinAstExpression *prev, bool canassign);
static TinAstExpression *tin_astparser_ruleor(TinAstParser *parser, TinAstExpression *prev, bool canassign);
static TinAstExpression *tin_astparser_rulenull_filter(TinAstParser *parser, TinAstExpression *prev, bool canassign);
static TinAstExpression *tin_astparser_rulecompound(TinAstParser *parser, TinAstExpression *prev, bool canassign);
static TinAstExpression *tin_astparser_ruleliteral(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_rulestring(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_ruleinterpolation(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_ruleobject(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_rulevarexprbase(TinAstParser *parser, bool canassign, bool isnew);
static TinAstExpression *tin_astparser_rulevarexpr(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_rulenewexpr(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_ruledot(TinAstParser *parser, TinAstExpression *previous, bool canassign);
static TinAstExpression *tin_astparser_rulerange(TinAstParser *parser, TinAstExpression *previous, bool canassign);
static TinAstExpression *tin_astparser_ruleternary(TinAstParser *parser, TinAstExpression *previous, bool canassign);
static TinAstExpression *tin_astparser_rulearray(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_rulesubscript(TinAstParser *parser, TinAstExpression *previous, bool canassign);
static TinAstExpression *tin_astparser_rulethis(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_rulesuper(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_rulereference(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_rulenothing(TinAstParser *parser, bool canassign);
static TinAstExpression *tin_astparser_rulefunction(TinAstParser *parser, bool canassign);


static void tin_astparser_setuprules()
{
    rules[TINTOK_PARENOPEN] = (TinAstParseRule){ tin_astparser_rulegroupingorlambda, tin_astparser_rulecall, TINPREC_CALL };
    rules[TINTOK_PLUS] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_TERM };
    rules[TINTOK_MINUS] = (TinAstParseRule){ tin_astparser_ruleunary, tin_astparser_rulebinary, TINPREC_TERM };
    rules[TINTOK_BANG] = (TinAstParseRule){ tin_astparser_ruleunary, tin_astparser_rulebinary, TINPREC_TERM };
    rules[TINTOK_STAR] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_FACTOR };
    rules[TINTOK_DOUBLESTAR] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_FACTOR };
    rules[TINTOK_SLASH] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_FACTOR };
    rules[TINTOK_SHARP] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_FACTOR };
    rules[TINTOK_STAR] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_FACTOR };
    rules[TINTOK_STAR] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_FACTOR };
    rules[TINTOK_BAR] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_BOR };
    rules[TINTOK_AMPERSAND] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_BAND };
    rules[TINTOK_TILDE] = (TinAstParseRule){ tin_astparser_ruleunary, NULL, TINPREC_UNARY };
    rules[TINTOK_CARET] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_BOR };
    rules[TINTOK_SHIFTLEFT] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_SHIFT };
    rules[TINTOK_SHIFTRIGHT] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_SHIFT };
    rules[TINTOK_PERCENT] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_FACTOR };
    rules[TINTOK_KWIS] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_IS };
    rules[TINTOK_NUMBER] = (TinAstParseRule){ tin_astparser_rulenumber, NULL, TINPREC_NONE };
    rules[TINTOK_KWTRUE] = (TinAstParseRule){ tin_astparser_ruleliteral, NULL, TINPREC_NONE };
    rules[TINTOK_KWFALSE] = (TinAstParseRule){ tin_astparser_ruleliteral, NULL, TINPREC_NONE };
    rules[TINTOK_KWNULL] = (TinAstParseRule){ tin_astparser_ruleliteral, NULL, TINPREC_NONE };
    rules[TINTOK_BANGEQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_EQUALITY };
    rules[TINTOK_EQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_EQUALITY };
    rules[TINTOK_GREATERTHAN] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_COMPARISON };
    rules[TINTOK_GREATEREQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_COMPARISON };
    rules[TINTOK_LESSTHAN] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_COMPARISON };
    rules[TINTOK_LESSEQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulebinary, TINPREC_COMPARISON };
    rules[TINTOK_STRING] = (TinAstParseRule){ tin_astparser_rulestring, NULL, TINPREC_NONE };
    rules[TINTOK_STRINTERPOL] = (TinAstParseRule){ tin_astparser_ruleinterpolation, NULL, TINPREC_NONE };
    rules[TINTOK_IDENT] = (TinAstParseRule){ tin_astparser_rulevarexpr, NULL, TINPREC_NONE };
    rules[TINTOK_KWNEW] = (TinAstParseRule){ tin_astparser_rulenewexpr, NULL, TINPREC_NONE };
    rules[TINTOK_PLUSEQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulecompound, TINPREC_COMPOUND };
    rules[TINTOK_MINUSEQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulecompound, TINPREC_COMPOUND };
    rules[TINTOK_STAREQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulecompound, TINPREC_COMPOUND };
    rules[TINTOK_SLASHEQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulecompound, TINPREC_COMPOUND };
    rules[TINTOK_SHARPEQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulecompound, TINPREC_COMPOUND };
    rules[TINTOK_PERCENTEQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulecompound, TINPREC_COMPOUND };
    rules[TINTOK_CARETEQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulecompound, TINPREC_COMPOUND };
    rules[TINTOK_ASSIGNEQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulecompound, TINPREC_COMPOUND };
    rules[TINTOK_AMPERSANDEQUAL] = (TinAstParseRule){ NULL, tin_astparser_rulecompound, TINPREC_COMPOUND };
    rules[TINTOK_DOUBLEPLUS] = (TinAstParseRule){ NULL, tin_astparser_rulecompound, TINPREC_COMPOUND };
    rules[TINTOK_DOUBLEMINUS] = (TinAstParseRule){ NULL, tin_astparser_rulecompound, TINPREC_COMPOUND };
    rules[TINTOK_DOUBLEAMPERSAND] = (TinAstParseRule){ NULL, tin_astparser_ruleand, TINPREC_AND };
    rules[TINTOK_DOUBLEBAR] = (TinAstParseRule){ NULL, tin_astparser_ruleor, TINPREC_AND };
    rules[TINTOK_DOUBLEQUESTION] = (TinAstParseRule){ NULL, tin_astparser_rulenull_filter, TINPREC_NULL };
    rules[TINTOK_DOT] = (TinAstParseRule){ NULL, tin_astparser_ruledot, TINPREC_CALL };
    rules[TINTOK_SMALLARROW] = (TinAstParseRule){ NULL, tin_astparser_ruledot, TINPREC_CALL };
    rules[TINTOK_DOUBLEDOT] = (TinAstParseRule){ NULL, tin_astparser_rulerange, TINPREC_RANGE };
    rules[TINTOK_TRIPLEDOT] = (TinAstParseRule){ tin_astparser_rulevarexpr, NULL, TINPREC_ASSIGNMENT };
    rules[TINTOK_BRACKETOPEN] = (TinAstParseRule){ tin_astparser_rulearray, tin_astparser_rulesubscript, TINPREC_NONE };
    rules[TINTOK_BRACEOPEN] = (TinAstParseRule){ tin_astparser_ruleobject, NULL, TINPREC_NONE };
    rules[TINTOK_KWTHIS] = (TinAstParseRule){ tin_astparser_rulethis, NULL, TINPREC_NONE };
    rules[TINTOK_KWSUPER] = (TinAstParseRule){ tin_astparser_rulesuper, NULL, TINPREC_NONE };
    rules[TINTOK_QUESTION] = (TinAstParseRule){ NULL, tin_astparser_ruleternary, TINPREC_EQUALITY };
    rules[TINTOK_KWREF] = (TinAstParseRule){ tin_astparser_rulereference, NULL, TINPREC_NONE };
    rules[TINTOK_KWFUNCTION] = (TinAstParseRule){tin_astparser_rulefunction, NULL, TINPREC_NONE};
    rules[TINTOK_SEMICOLON] = (TinAstParseRule){tin_astparser_rulenothing, NULL, TINPREC_NONE};
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


static void tin_astparser_initcompiler(TinAstParser* parser, TinAstCompiler* compiler)
{
    compiler->scope_depth = 0;
    compiler->function = NULL;
    compiler->enclosing = (struct TinAstCompiler*)parser->compiler;

    parser->compiler = compiler;
}

static void tin_astparser_endcompiler(TinAstParser* parser, TinAstCompiler* compiler)
{
    parser->compiler = (TinAstCompiler*)compiler->enclosing;
}

static void tin_astparser_beginscope(TinAstParser* parser)
{
    parser->compiler->scope_depth++;
}

static void tin_astparser_endscope(TinAstParser* parser)
{
    parser->compiler->scope_depth--;
}

static TinAstParseRule* tin_astparser_getrule(TinAstTokType type)
{
    return &rules[type];
}

static inline bool prs_is_at_end(TinAstParser* parser)
{
    return parser->current.type == TINTOK_EOF;
}

void tin_astparser_init(TinState* state, TinAstParser* parser)
{
    if(!didsetuprules)
    {
        didsetuprules = true;
        tin_astparser_setuprules();
    }
    parser->state = state;
    parser->haderror = false;
    parser->panic_mode = false;
}

void tin_astparser_destroy(TinAstParser* parser)
{
    (void)parser;
}

static void tin_astparser_raisestring(TinAstParser* parser, TinAstToken* token, const char* message)
{
    (void)token;
    if(parser->panic_mode)
    {
        return;
    }
    tin_state_raiseerror(parser->state, COMPILE_ERROR, message);
    parser->haderror = true;
    tin_astparser_sync(parser);
}

static void tin_astparser_raiseat(TinAstParser* parser, TinAstToken* token, const char* fmt, va_list args)
{
    tin_astparser_raisestring(parser, token, tin_vformat_error(parser->state, token->line, fmt, args)->chars);
}

static void tin_astparser_raiseatcurrent(TinAstParser* parser, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    tin_astparser_raiseat(parser, &parser->current, fmt, args);
    va_end(args);
}

static void tin_astparser_raiseerror(TinAstParser* parser, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    tin_astparser_raiseat(parser, &parser->previous, fmt, args);
    va_end(args);
}

static void tin_astparser_advance(TinAstParser* parser)
{
    parser->previous = parser->current;

    while(true)
    {
        parser->current = tin_astlex_scantoken(parser->state->scanner);
        if(parser->current.type != TINTOK_ERROR)
        {
            break;
        }
        tin_astparser_raisestring(parser, &parser->current, parser->current.start);
    }
}

static bool tin_astparser_check(TinAstParser* parser, TinAstTokType type)
{
    return parser->current.type == type;
}

static bool tin_astparser_match(TinAstParser* parser, TinAstTokType type)
{
    if(parser->current.type == type)
    {
        tin_astparser_advance(parser);
        return true;
    }
    return false;
}

static bool tin_astparser_matchnewline(TinAstParser* parser)
{
    while(true)
    {
        if(!tin_astparser_match(parser, TINTOK_NEWLINE))
        {
            return false;
        }
    }
    return true;
}

static void tin_astparser_ignorenewlines(TinAstParser* parser, bool checksemi)
{
    (void)checksemi;
    tin_astparser_matchnewline(parser);
}

static void tin_astparser_consume(TinAstParser* parser, TinAstTokType type, const char* onerror)
{
    bool line;
    size_t olen;
    const char* fmt;
    const char* otext;
    if(parser->current.type == type)
    {
        tin_astparser_advance(parser);
        return;
    }
    //fprintf(stderr, "in tin_astparser_consume: failed?\n");
    line = parser->previous.type == TINTOK_NEWLINE;
    olen = (line ? 8 : parser->previous.length);
    otext = (line ? "new line" : parser->previous.start);
    fmt = tin_format_error(parser->state, parser->current.line, "expected %s, got '%.*s'", onerror, olen, otext)->chars;
    tin_astparser_raisestring(parser, &parser->current,fmt);
}

static TinAstExpression* tin_astparser_parseblock(TinAstParser* parser)
{
    TinAstBlockExpr* statement;
    tin_astparser_beginscope(parser);
    statement = tin_ast_make_blockexpr(parser->state, parser->previous.line);
    while(true)
    {
        tin_astparser_ignorenewlines(parser, true);
        if(tin_astparser_check(parser, TINTOK_BRACECLOSE) || tin_astparser_check(parser, TINTOK_EOF))
        {
            break;
        }
        tin_astparser_ignorenewlines(parser, true);
        tin_exprlist_push(parser->state, &statement->statements, tin_astparser_parsestatement(parser));
        tin_astparser_ignorenewlines(parser, true);
    }
    tin_astparser_ignorenewlines(parser, true);
    tin_astparser_consume(parser, TINTOK_BRACECLOSE, "'}'");
    tin_astparser_ignorenewlines(parser, true);
    tin_astparser_endscope(parser);
    return (TinAstExpression*)statement;
}

static TinAstExpression* tin_astparser_parseprecedence(TinAstParser* parser, TinAstPrecedence precedence, bool err, bool ignsemi)
{
    bool new_line;
    bool prevnewline;
    bool parserprevnewline;
    bool canassign;
    TinAstExpression* expr;
    TinAstParsePrefixFn prefix_rule;
    TinAstParseInfixFn infix_rule;
    TinAstToken previous;
    (void)new_line;
    prevnewline = false;
    previous = parser->previous;
    tin_astparser_advance(parser);
    prefix_rule = tin_astparser_getrule(parser->previous.type)->prefix;
    if(prefix_rule == NULL)
    {
        //if(parser->previous.type != parser->current.type)
        {
            // todo: file start
            new_line = previous.start != NULL && *previous.start == '\n';
            parserprevnewline = parser->previous.start != NULL && *parser->previous.start == '\n';
            tin_astparser_raiseerror(parser, "expected expression after '%.*s', got '%.*s'",
                (prevnewline ? 8 : previous.length),
                (prevnewline ? "new line" : previous.start),
                (parserprevnewline ? 8 : parser->previous.length),
                (parserprevnewline ? "new line" : parser->previous.start)
            );
            return NULL;
        }
    }
    canassign = precedence <= TINPREC_ASSIGNMENT;
    expr = prefix_rule(parser, canassign);
    tin_astparser_ignorenewlines(parser, ignsemi);
    while(precedence <= tin_astparser_getrule(parser->current.type)->precedence)
    {
        tin_astparser_advance(parser);
        infix_rule = tin_astparser_getrule(parser->previous.type)->infix;
        expr = infix_rule(parser, expr, canassign);
    }
    if(err && canassign && tin_astparser_match(parser, TINTOK_ASSIGN))
    {
        tin_astparser_raiseerror(parser, "invalid assigment target");
    }
    return expr;
}

static TinAstExpression* tin_astparser_rulenumber(TinAstParser* parser, bool canassign)
{
    (void)canassign;
    return (TinAstExpression*)tin_ast_make_literalexpr(parser->state, parser->previous.line, parser->previous.value);
}

static TinAstExpression* tin_astparser_parselambda(TinAstParser* parser, TinAstFunctionExpr* lambda)
{
    lambda->body = tin_astparser_parsestatement(parser);
    return (TinAstExpression*)lambda;
}

static void tin_astparser_parseparameters(TinAstParser* parser, TinAstParamList* parameters)
{
    bool haddefault;
    size_t arglength;
    const char* argname;
    TinAstExpression* defexpr;
    haddefault = false;
    while(!tin_astparser_check(parser, TINTOK_PARENCLOSE))
    {
        // Vararg ...
        if(tin_astparser_match(parser, TINTOK_TRIPLEDOT))
        {
            tin_paramlist_push(parser->state, parameters, (TinAstParameter){ "...", 3, NULL });
            return;
        }
        tin_astparser_consume(parser, TINTOK_IDENT, "argument name");
        argname = parser->previous.start;
        arglength = parser->previous.length;
        defexpr = NULL;
        if(tin_astparser_match(parser, TINTOK_ASSIGN))
        {
            haddefault = true;
            defexpr = tin_astparser_parseexpression(parser, true);
        }
        else if(haddefault)
        {
            tin_astparser_raiseerror(parser, "default arguments must always be in the end of the argument list.");
        }
        tin_paramlist_push(parser->state, parameters, (TinAstParameter){ argname, arglength, defexpr });
        if(!tin_astparser_match(parser, TINTOK_COMMA))
        {
            break;
        }
    }
}

/*
* this is extremely not working at all.
*/
static TinAstExpression* tin_astparser_rulegroupingorlambda(TinAstParser* parser, bool canassign)
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
    if(tin_astparser_match(parser, TINTOK_PARENCLOSE))
    {
        tin_astparser_consume(parser, TINTOK_ARROW, "=> after lambda arguments");
        return tin_astparser_parselambda(parser, tin_ast_make_lambdaexpr(parser->state, parser->previous.line));
    }
    start = parser->previous.start;
    line = parser->previous.line;
    if(tin_astparser_match(parser, TINTOK_IDENT) || tin_astparser_match(parser, TINTOK_TRIPLEDOT))
    {
        TinState* state = parser->state;
        firstargstart = parser->previous.start;
        firstarglength = parser->previous.length;
        if(tin_astparser_match(parser, TINTOK_COMMA) || (tin_astparser_match(parser, TINTOK_PARENCLOSE) && tin_astparser_match(parser, TINTOK_ARROW)))
        {
            had_array = parser->previous.type == TINTOK_ARROW;
            hadvararg= parser->previous.type == TINTOK_TRIPLEDOT;
            // This is a lambda
            TinAstFunctionExpr* lambda = tin_ast_make_lambdaexpr(state, line);
            TinAstExpression* defvalue = NULL;
            haddefault = tin_astparser_match(parser, TINTOK_ASSIGN);
            if(haddefault)
            {
                defvalue = tin_astparser_parseexpression(parser, true);
            }
            tin_paramlist_push(state, &lambda->parameters, (TinAstParameter){ firstargstart, firstarglength, defvalue });
            if(!hadvararg && parser->previous.type == TINTOK_COMMA)
            {
                do
                {
                    stop = false;
                    if(tin_astparser_match(parser, TINTOK_TRIPLEDOT))
                    {
                        stop = true;
                    }
                    else
                    {
                        tin_astparser_consume(parser, TINTOK_IDENT, "argument name");
                    }

                    argname = parser->previous.start;
                    arglength = parser->previous.length;
                    defexpr = NULL;
                    if(tin_astparser_match(parser, TINTOK_ASSIGN))
                    {
                        defexpr = tin_astparser_parseexpression(parser, true);
                        haddefault = true;
                    }
                    else if(haddefault)
                    {
                        tin_astparser_raiseerror(parser, "default arguments must always be in the end of the argument list.");
                    }
                    tin_paramlist_push(state, &lambda->parameters, (TinAstParameter){ argname, arglength, defexpr });
                    if(stop)
                    {
                        break;
                    }
                } while(tin_astparser_match(parser, TINTOK_COMMA));
            }
            #if 0
            if(!hadarrow)
            {
                tin_astparser_consume(parser, TINTOK_PARENCLOSE, "')' after lambda parameters");
                tin_astparser_consume(parser, TINTOK_ARROW, "=> after lambda parameters");
            }
            #endif
            return tin_astparser_parselambda(parser, lambda);
        }
        else
        {
            // Ouch, this was a grouping with a single identifier
            scanner = state->scanner;
            scanner->current = start;
            scanner->line = line;
            parser->current = tin_astlex_scantoken(scanner);
            tin_astparser_advance(parser);
        }
    }
    expression = tin_astparser_parseexpression(parser, true);
    tin_astparser_consume(parser, TINTOK_PARENCLOSE, "')' after grouping expression");
    return expression;
}

static TinAstExpression* tin_astparser_rulecall(TinAstParser* parser, TinAstExpression* prev, bool canassign)
{
    (void)canassign;
    TinAstExpression* e;
    TinAstVarExpr* ee;
    TinAstCallExpr* expression;
    expression = tin_ast_make_callexpr(parser->state, parser->previous.line, prev);
    while(!tin_astparser_check(parser, TINTOK_PARENCLOSE))
    {
        e = tin_astparser_parseexpression(parser, true);
        tin_exprlist_push(parser->state, &expression->args, e);
        if(!tin_astparser_match(parser, TINTOK_COMMA))
        {
            break;
        }
        if(e->type == TINEXPR_VAREXPR)
        {
            ee = (TinAstVarExpr*)e;
            // Vararg ...
            if(ee->length == 3 && memcmp(ee->name, "...", 3) == 0)
            {
                break;
            }
        }
    }
    if(expression->args.count > 255)
    {
        tin_astparser_raiseerror(parser, "function cannot have more than 255 arguments, got %i", (int)expression->args.count);
    }
    tin_astparser_consume(parser, TINTOK_PARENCLOSE, "')' after arguments");
    return (TinAstExpression*)expression;
}

static TinAstExpression* tin_astparser_ruleunary(TinAstParser* parser, bool canassign)
{
    (void)canassign;
    size_t line;
    TinAstExpression* expression;
    TinAstTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    expression = tin_astparser_parseprecedence(parser, TINPREC_UNARY, true, true);
    return (TinAstExpression*)tin_ast_make_unaryexpr(parser->state, line, expression, op);
}

static TinAstExpression* tin_astparser_rulebinary(TinAstParser* parser, TinAstExpression* prev, bool canassign)
{
    (void)canassign;
    bool invert;
    size_t line;
    TinAstParseRule* rule;
    TinAstExpression* expression;
    TinAstTokType op;
    invert = parser->previous.type == TINTOK_BANG;
    if(invert)
    {
        tin_astparser_consume(parser, TINTOK_KWIS, "'is' after '!'");
    }
    op = parser->previous.type;
    line = parser->previous.line;
    rule = tin_astparser_getrule(op);
    expression = tin_astparser_parseprecedence(parser, (TinAstPrecedence)(rule->precedence + 1), true, true);
    expression = (TinAstExpression*)tin_ast_make_binaryexpr(parser->state, line, prev, expression, op);
    if(invert)
    {
        expression = (TinAstExpression*)tin_ast_make_unaryexpr(parser->state, line, expression, TINTOK_BANG);
    }
    return expression;
}

static TinAstExpression* tin_astparser_ruleand(TinAstParser* parser, TinAstExpression* prev, bool canassign)
{
    (void)canassign;
    size_t line;
    TinAstTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    return (TinAstExpression*)tin_ast_make_binaryexpr(parser->state, line, prev, tin_astparser_parseprecedence(parser, TINPREC_AND, true, true), op);
}

static TinAstExpression* tin_astparser_ruleor(TinAstParser* parser, TinAstExpression* prev, bool canassign)
{
    (void)canassign;
    size_t line;
    TinAstTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    return (TinAstExpression*)tin_ast_make_binaryexpr(parser->state, line, prev, tin_astparser_parseprecedence(parser, TINPREC_OR, true, true), op);
}

static TinAstExpression* tin_astparser_rulenull_filter(TinAstParser* parser, TinAstExpression* prev, bool canassign)
{
    (void)canassign;
    size_t line;
    TinAstTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    return (TinAstExpression*)tin_ast_make_binaryexpr(parser->state, line, prev, tin_astparser_parseprecedence(parser, TINPREC_NULL, true, true), op);
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
                UNREACHABLE
            }
            break;
    }
    return (TinAstTokType)-1;
}

static TinAstExpression* tin_astparser_rulecompound(TinAstParser* parser, TinAstExpression* prev, bool canassign)
{
    (void)canassign;
    size_t line;
    TinAstBinaryExpr* binary;
    TinAstExpression* expression;
    TinAstParseRule* rule;
    TinAstTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    rule = tin_astparser_getrule(op);
    if(op == TINTOK_DOUBLEPLUS || op == TINTOK_DOUBLEMINUS)
    {
        expression = (TinAstExpression*)tin_ast_make_literalexpr(parser->state, line, tin_value_makefixednumber(parser->state, 1));
    }
    else
    {
        expression = tin_astparser_parseprecedence(parser, (TinAstPrecedence)(rule->precedence + 1), true, true);
    }
    binary = tin_ast_make_binaryexpr(parser->state, line, prev, expression, tin_astparser_convertcompoundop(op));
    // To make sure we don't free it twice
    binary->ignore_left = true;
    return (TinAstExpression*)tin_ast_make_assignexpr(parser->state, line, prev, (TinAstExpression*)binary);
}

static TinAstExpression* tin_astparser_ruleliteral(TinAstParser* parser, bool canassign)
{
    (void)canassign;
    size_t line;
    line = parser->previous.line;
    switch(parser->previous.type)
    {
        case TINTOK_KWTRUE:
            {
                return (TinAstExpression*)tin_ast_make_literalexpr(parser->state, line, TRUE_VALUE);
            }
            break;
        case TINTOK_KWFALSE:
            {
                return (TinAstExpression*)tin_ast_make_literalexpr(parser->state, line, FALSE_VALUE);
            }
            break;
        case TINTOK_KWNULL:
            {
                return (TinAstExpression*)tin_ast_make_literalexpr(parser->state, line, NULL_VALUE);
            }
            break;
        default:
            {
                UNREACHABLE
            }
            break;
    }
    return NULL;
}

static TinAstExpression* tin_astparser_rulestring(TinAstParser* parser, bool canassign)
{
    (void)canassign;
    TinAstExpression* expression;
    expression = (TinAstExpression*)tin_ast_make_literalexpr(parser->state, parser->previous.line, parser->previous.value);
    if(tin_astparser_match(parser, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(parser, expression, canassign);
    }
    return expression;
}

static TinAstExpression* tin_astparser_ruleinterpolation(TinAstParser* parser, bool canassign)
{
    TinAstStrInterExpr* expression;
    (void)canassign;
    expression = tin_ast_make_strinterpolexpr(parser->state, parser->previous.line);
    do
    {
        if(tin_string_getlength(tin_value_asstring(parser->previous.value)) > 0)
        {
            tin_exprlist_push(
            parser->state, &expression->expressions,
            (TinAstExpression*)tin_ast_make_literalexpr(parser->state, parser->previous.line, parser->previous.value));
        }
        tin_exprlist_push(parser->state, &expression->expressions, tin_astparser_parseexpression(parser, true));
    } while(tin_astparser_match(parser, TINTOK_STRINTERPOL));
    tin_astparser_consume(parser, TINTOK_STRING, "end of interpolation");
    if(tin_string_getlength(tin_value_asstring(parser->previous.value)) > 0)
    {
        tin_exprlist_push(
        parser->state, &expression->expressions,
        (TinAstExpression*)tin_ast_make_literalexpr(parser->state, parser->previous.line, parser->previous.value));
    }
    if(tin_astparser_match(parser, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(parser, (TinAstExpression*)expression, canassign);
    }
    return (TinAstExpression*)expression;
}

static TinAstExpression* tin_astparser_ruleobject(TinAstParser* parser, bool canassign)
{
    TinString* ts;
    TinValue tv;
    TinAstExpression* expr;
    TinAstObjectExpr* object;
    (void)canassign;

    object = tin_ast_make_objectexpr(parser->state, parser->previous.line);
    tin_astparser_ignorenewlines(parser, true);
    while(!tin_astparser_check(parser, TINTOK_BRACECLOSE))
    {
        tin_astparser_ignorenewlines(parser, true);
        if(tin_astparser_check(parser, TINTOK_IDENT))
        {
            tin_astparser_raiseerror(parser, "cannot use bare names (for now)");
            //tin_astparser_consume(parser, TINTOK_IDENT, "key string after '{'");
            
            //tin_vallist_push(parser->state, &object->keys, tin_value_fromobject(tin_string_copy(parser->state, parser->previous.start, parser->previous.length)));
            expr = tin_astparser_parseexpression(parser, true);
            tin_exprlist_push(parser->state, &object->keys, expr);

        }
        else if(tin_astparser_check(parser, TINTOK_STRING))
        {
            expr = tin_astparser_parseexpression(parser, true);
            tv = parser->previous.value;
            ts = tin_value_asstring(tv);
            //tin_vallist_push(parser->state, &object->keys, tin_value_fromobject(tin_string_copy(parser->state, ts->chars, tin_string_getlength(ts))));
            //tin_vallist_push(parser->state, &object->keys, tv);
            tin_exprlist_push(parser->state, &object->keys, expr);

            //tin_ast_destroyexpression(parser->state, expr);
        }
        
        else
        {
            tin_astparser_raiseerror(parser, "expect identifier or string as object key");
        }
    
        tin_astparser_ignorenewlines(parser, true);
        tin_astparser_consume(parser, TINTOK_COLON, "':' after key string");
        tin_astparser_ignorenewlines(parser, true);
        tin_exprlist_push(parser->state, &object->values, tin_astparser_parseexpression(parser, true));
        if(!tin_astparser_match(parser, TINTOK_COMMA))
        {
            break;
        }
    }
    tin_astparser_ignorenewlines(parser, true);
    tin_astparser_consume(parser, TINTOK_BRACECLOSE, "'}' after object");
    return (TinAstExpression*)object;
}

static TinAstExpression* tin_astparser_rulevarexprbase(TinAstParser* parser, bool canassign, bool isnew)
{
    (void)canassign;
    bool hadargs;
    TinAstCallExpr* callex;
    TinAstExpression* expression;
    expression = (TinAstExpression*)tin_ast_make_varexpr(parser->state, parser->previous.line, parser->previous.start, parser->previous.length);
    if(isnew)
    {
        hadargs = tin_astparser_check(parser, TINTOK_PARENOPEN);
        callex = NULL;
        if(hadargs)
        {
            tin_astparser_advance(parser);
            callex = (TinAstCallExpr*)tin_astparser_rulecall(parser, expression, false);
        }
        if(tin_astparser_match(parser, TINTOK_BRACEOPEN))
        {
            if(callex == NULL)
            {
                callex = tin_ast_make_callexpr(parser->state, expression->line, expression);
            }
            callex->init = tin_astparser_ruleobject(parser, false);
        }
        else if(!hadargs)
        {
            tin_astparser_raiseatcurrent(parser, "expected %s, got '%.*s'", "argument list for instance creation",
                             parser->previous.length, parser->previous.start);
        }
        return (TinAstExpression*)callex;
    }
    if(tin_astparser_match(parser, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(parser, expression, canassign);
    }
    if(canassign && tin_astparser_match(parser, TINTOK_ASSIGN))
    {
        return (TinAstExpression*)tin_ast_make_assignexpr(parser->state, parser->previous.line, expression,
                                                            tin_astparser_parseexpression(parser, true));
    }
    return expression;
}

static TinAstExpression* tin_astparser_rulevarexpr(TinAstParser* parser, bool canassign)
{
    return tin_astparser_rulevarexprbase(parser, canassign, false);
}

static TinAstExpression* tin_astparser_rulenewexpr(TinAstParser* parser, bool canassign)
{
    (void)canassign;
    tin_astparser_consume(parser, TINTOK_IDENT, "class name after 'new'");
    return tin_astparser_rulevarexprbase(parser, false, true);
}

static TinAstExpression* tin_astparser_ruledot(TinAstParser* parser, TinAstExpression* previous, bool canassign)
{
    (void)canassign;
    bool ignored;
    size_t line;
    size_t length;
    const char* name;
    TinAstExpression* expression;
    line = parser->previous.line;
    ignored = parser->previous.type == TINTOK_SMALLARROW;
    if(!(tin_astparser_match(parser, TINTOK_KWCLASS) || tin_astparser_match(parser, TINTOK_KWSUPER)))
    {// class and super are allowed field names
        tin_astparser_consume(parser, TINTOK_IDENT, ignored ? "propety name after '->'" : "property name after '.'");
    }
    name = parser->previous.start;
    length = parser->previous.length;
    if(!ignored && canassign && tin_astparser_match(parser, TINTOK_ASSIGN))
    {
        return (TinAstExpression*)tin_ast_make_setexpr(parser->state, line, previous, name, length, tin_astparser_parseexpression(parser, true));
    }
    expression = (TinAstExpression*)tin_ast_make_getexpr(parser->state, line, previous, name, length, false, ignored);
    if(!ignored && tin_astparser_match(parser, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(parser, expression, canassign);
    }
    return expression;
}

static TinAstExpression* tin_astparser_rulerange(TinAstParser* parser, TinAstExpression* previous, bool canassign)
{
    (void)canassign;
    size_t line;
    line = parser->previous.line;
    return (TinAstExpression*)tin_ast_make_rangeexpr(parser->state, line, previous, tin_astparser_parseexpression(parser, true));
}

static TinAstExpression* tin_astparser_ruleternary(TinAstParser* parser, TinAstExpression* previous, bool canassign)
{
    (void)canassign;
    bool ignored;
    size_t line;
    TinAstExpression* ifbranch;
    TinAstExpression* elsebranch;
    line = parser->previous.line;
    if(tin_astparser_match(parser, TINTOK_DOT) || tin_astparser_match(parser, TINTOK_SMALLARROW))
    {
        ignored = parser->previous.type == TINTOK_SMALLARROW;
        tin_astparser_consume(parser, TINTOK_IDENT, ignored ? "property name after '->'" : "property name after '.'");
        return (TinAstExpression*)tin_ast_make_getexpr(parser->state, line, previous, parser->previous.start,
                                                         parser->previous.length, true, ignored);
    }
    ifbranch = tin_astparser_parseexpression(parser, true);
    tin_astparser_consume(parser, TINTOK_COLON, "':' after expression");
    elsebranch = tin_astparser_parseexpression(parser, true);
    return (TinAstExpression*)tin_ast_make_ternaryexpr(parser->state, line, previous, ifbranch, elsebranch);
}

static TinAstExpression* tin_astparser_rulearray(TinAstParser* parser, bool canassign)
{
    (void)canassign;
    TinAstExpression* expr;
    TinAstArrayExpr* array;
    array = tin_ast_make_arrayexpr(parser->state, parser->previous.line);
    tin_astparser_ignorenewlines(parser, true);
    while(!tin_astparser_check(parser, TINTOK_BRACKETCLOSE))
    {
        expr = NULL;
        tin_astparser_ignorenewlines(parser, true);
        #if 1
            expr = tin_astparser_parseexpression(parser, true);
        #else
            if(tin_astparser_check(parser, TINTOK_COMMA))
            {
                //tin_astparser_rulenull_filter(TinAstParser *parser, TinAstExpression *prev, _Bool canassign)
                expr = tin_astparser_rulenull_filter(parser, NULL, false);
            }
            else
            {
                expr = tin_astparser_parseexpression(parser, true);
            }
        #endif
        tin_exprlist_push(parser->state, &array->values, expr);
        if(!tin_astparser_match(parser, TINTOK_COMMA))
        {
            break;
        }
        tin_astparser_ignorenewlines(parser, true);
    }
    tin_astparser_ignorenewlines(parser, true);
    tin_astparser_consume(parser, TINTOK_BRACKETCLOSE, "']' after array");
    if(tin_astparser_match(parser, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(parser, (TinAstExpression*)array, canassign);
    }
    return (TinAstExpression*)array;
}

static TinAstExpression* tin_astparser_rulesubscript(TinAstParser* parser, TinAstExpression* previous, bool canassign)
{
    size_t line;
    TinAstExpression* index;
    TinAstExpression* expression;
    line = parser->previous.line;
    index = tin_astparser_parseexpression(parser, true);
    tin_astparser_consume(parser, TINTOK_BRACKETCLOSE, "']' after subscript");
    expression = (TinAstExpression*)tin_ast_make_subscriptexpr(parser->state, line, previous, index);
    if(tin_astparser_match(parser, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(parser, expression, canassign);
    }
    else if(canassign && tin_astparser_match(parser, TINTOK_ASSIGN))
    {
        return (TinAstExpression*)tin_ast_make_assignexpr(parser->state, parser->previous.line, expression, tin_astparser_parseexpression(parser, true));
    }
    return expression;
}

static TinAstExpression* tin_astparser_rulethis(TinAstParser* parser, bool canassign)
{
    TinAstExpression* expression;
    expression = (TinAstExpression*)tin_ast_make_thisexpr(parser->state, parser->previous.line);
    if(tin_astparser_match(parser, TINTOK_BRACKETOPEN))
    {
        return tin_astparser_rulesubscript(parser, expression, canassign);
    }
    return expression;
}

static TinAstExpression* tin_astparser_rulesuper(TinAstParser* parser, bool canassign)
{
    (void)canassign;
    bool ignoring;
    size_t line;
    TinAstExpression* expression;
    line = parser->previous.line;
    if(!(tin_astparser_match(parser, TINTOK_DOT) || tin_astparser_match(parser, TINTOK_SMALLARROW)))
    {
        expression = (TinAstExpression*)tin_ast_make_superexpr(
        parser->state, line, tin_string_copy(parser->state, "constructor", 11), false);
        tin_astparser_consume(parser, TINTOK_PARENOPEN, "'(' after 'super'");
        return tin_astparser_rulecall(parser, expression, false);
    }
    ignoring = parser->previous.type == TINTOK_SMALLARROW;
    tin_astparser_consume(parser, TINTOK_IDENT, ignoring ? "super method name after '->'" : "super method name after '.'");
    expression = (TinAstExpression*)tin_ast_make_superexpr(
    parser->state, line, tin_string_copy(parser->state, parser->previous.start, parser->previous.length), ignoring);
    if(tin_astparser_match(parser, TINTOK_PARENOPEN))
    {
        return tin_astparser_rulecall(parser, expression, false);
    }
    return expression;
}

static TinAstExpression *tin_astparser_rulenothing(TinAstParser *parser, bool canassign)
{
    (void)parser;
    (void)canassign;
    return NULL;
}

static TinAstExpression* tin_astparser_rulereference(TinAstParser* parser, bool canassign)
{
    size_t line;
    TinAstRefExpr* expression;
    (void)canassign;
    line = parser->previous.line;
    tin_astparser_ignorenewlines(parser, true);
    expression = tin_ast_make_referenceexpr(parser->state, line, tin_astparser_parseprecedence(parser, TINPREC_CALL, false, true));
    if(tin_astparser_match(parser, TINTOK_ASSIGN))
    {
        return (TinAstExpression*)tin_ast_make_assignexpr(parser->state, line, (TinAstExpression*)expression, tin_astparser_parseexpression(parser, true));
    }
    return (TinAstExpression*)expression;
}



static TinAstExpression* tin_astparser_parsestatement(TinAstParser* parser)
{
    TinAstExpression* expression;
    tin_astparser_ignorenewlines(parser, true);
    if(setjmp(prs_jmpbuffer))
    {
        return NULL;
    }
    if(tin_astparser_match(parser, TINTOK_KWVAR) || tin_astparser_match(parser, TINTOK_KWCONST))
    {
        return tin_astparser_parsevar_declaration(parser, true);
    }
    else if(tin_astparser_match(parser, TINTOK_KWIF))
    {
        return tin_astparser_parseif(parser);
    }
    else if(tin_astparser_match(parser, TINTOK_KWFOR))
    {
        return tin_astparser_parsefor(parser);
    }
    else if(tin_astparser_match(parser, TINTOK_KWWHILE))
    {
        return tin_astparser_parsewhile(parser);
    }
    else if(tin_astparser_match(parser, TINTOK_KWCONTINUE))
    {
        return (TinAstExpression*)tin_ast_make_continueexpr(parser->state, parser->previous.line);
    }
    else if(tin_astparser_match(parser, TINTOK_KWBREAK))
    {
        return (TinAstExpression*)tin_ast_make_breakexpr(parser->state, parser->previous.line);
    }
    else if(tin_astparser_match(parser, TINTOK_KWFUNCTION) || tin_astparser_match(parser, TINTOK_KWEXPORT))
    {
        return tin_astparser_rulefunction(parser, false);
    }
    else if(tin_astparser_match(parser, TINTOK_KWRETURN))
    {
        return tin_astparser_parsereturn(parser);
    }
    else if(tin_astparser_match(parser, TINTOK_BRACEOPEN))
    {
        return tin_astparser_parseblock(parser);
    }
    expression = tin_astparser_parseexpression(parser, true);
    return expression == NULL ? NULL : (TinAstExpression*)tin_ast_make_exprstmt(parser->state, parser->previous.line, expression);
}

static TinAstExpression* tin_astparser_parseexpression(TinAstParser* parser, bool ignsemi)
{
    tin_astparser_ignorenewlines(parser, ignsemi);
    return tin_astparser_parseprecedence(parser, TINPREC_ASSIGNMENT, true, ignsemi);
}

static TinAstExpression* tin_astparser_parsevar_declaration(TinAstParser* parser, bool ignsemi)
{
    bool constant;
    size_t line;
    size_t length;
    const char* name;
    TinAstExpression* init;
    constant = parser->previous.type == TINTOK_KWCONST;
    line = parser->previous.line;
    tin_astparser_consume(parser, TINTOK_IDENT, "variable name");
    name = parser->previous.start;
    length = parser->previous.length;
    init = NULL;
    if(tin_astparser_match(parser, TINTOK_ASSIGN))
    {
        init = tin_astparser_parseexpression(parser, ignsemi);
    }
    return (TinAstExpression*)tin_ast_make_assignvarexpr(parser->state, line, name, length, init, constant);
}

static TinAstExpression* tin_astparser_parseif(TinAstParser* parser)
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
    line = parser->previous.line;
    invert = tin_astparser_match(parser, TINTOK_BANG);
    hadparen = tin_astparser_match(parser, TINTOK_PARENOPEN);
    condition = tin_astparser_parseexpression(parser, true);
    if(hadparen)
    {
        tin_astparser_consume(parser, TINTOK_PARENCLOSE, "')'");
    }
    if(invert)
    {
        condition = (TinAstExpression*)tin_ast_make_unaryexpr(parser->state, condition->line, condition, TINTOK_BANG);
    }
    tin_astparser_ignorenewlines(parser, true);
    ifbranch = tin_astparser_parsestatement(parser);
    elseifconds = NULL;
    elseifbranches = NULL;
    elsebranch = NULL;
    tin_astparser_ignorenewlines(parser, true);
    while(tin_astparser_match(parser, TINTOK_KWELSE))
    {
        // else if
        tin_astparser_ignorenewlines(parser, true);
        if(tin_astparser_match(parser, TINTOK_KWIF))
        {
            if(elseifconds == NULL)
            {
                elseifconds = tin_ast_allocexprlist(parser->state);
                elseifbranches = tin_ast_allocate_stmtlist(parser->state);
            }
            invert = tin_astparser_match(parser, TINTOK_BANG);
            hadparen = tin_astparser_match(parser, TINTOK_PARENOPEN);
            tin_astparser_ignorenewlines(parser, true);
            e = tin_astparser_parseexpression(parser, true);
            if(hadparen)
            {
                tin_astparser_consume(parser, TINTOK_PARENCLOSE, "')'");
            }
            tin_astparser_ignorenewlines(parser, true);
            if(invert)
            {
                e = (TinAstExpression*)tin_ast_make_unaryexpr(parser->state, condition->line, e, TINTOK_BANG);
            }
            tin_exprlist_push(parser->state, elseifconds, e);
            tin_exprlist_push(parser->state, elseifbranches, tin_astparser_parsestatement(parser));
            continue;
        }
        // else
        if(elsebranch != NULL)
        {
            tin_astparser_raiseerror(parser, "if-statement can have only one else-branch");
        }
        tin_astparser_ignorenewlines(parser, true);
        elsebranch = tin_astparser_parsestatement(parser);
    }
    return (TinAstExpression*)tin_ast_make_ifexpr(parser->state, line, condition, ifbranch, elsebranch, elseifconds, elseifbranches);
}

static TinAstExpression* tin_astparser_parsefor(TinAstParser* parser)
{
    bool cstyle;
    bool hadparen;
    size_t line;
    TinAstExpression* condition;
    TinAstExpression* increment;
    TinAstExpression* var;
    TinAstExpression* init;
    line= parser->previous.line;
    hadparen = tin_astparser_match(parser, TINTOK_PARENOPEN);
    var = NULL;
    init = NULL;
    if(!tin_astparser_check(parser, TINTOK_SEMICOLON))
    {
        if(tin_astparser_match(parser, TINTOK_KWVAR))
        {
            var = tin_astparser_parsevar_declaration(parser, false);
        }
        else
        {
            init = tin_astparser_parseexpression(parser, false);
        }
    }
    cstyle = !tin_astparser_match(parser, TINTOK_KWIN);
    condition= NULL;
    increment = NULL;
    if(cstyle)
    {
        tin_astparser_consume(parser, TINTOK_SEMICOLON, "';'");
        condition = tin_astparser_check(parser, TINTOK_SEMICOLON) ? NULL : tin_astparser_parseexpression(parser, false);
        tin_astparser_consume(parser, TINTOK_SEMICOLON, "';'");
        increment = tin_astparser_check(parser, TINTOK_PARENCLOSE) ? NULL : tin_astparser_parseexpression(parser, false);
    }
    else
    {
        condition = tin_astparser_parseexpression(parser, true);
        if(var == NULL)
        {
            tin_astparser_raiseerror(parser, "for-loops using in-iteration must declare a new variable");
        }
    }
    if(hadparen)
    {
        tin_astparser_consume(parser, TINTOK_PARENCLOSE, "')'");
    }
    tin_astparser_ignorenewlines(parser, true);
    return (TinAstExpression*)tin_ast_make_forexpr(parser->state, line, init, var, condition, increment,
                                                   tin_astparser_parsestatement(parser), cstyle);
}

static TinAstExpression* tin_astparser_parsewhile(TinAstParser* parser)
{
    bool hadparen;
    size_t line;
    TinAstExpression* body;
    line = parser->previous.line;
    hadparen = tin_astparser_match(parser, TINTOK_PARENOPEN);
    TinAstExpression* condition = tin_astparser_parseexpression(parser, true);
    if(hadparen)
    {
        tin_astparser_consume(parser, TINTOK_PARENCLOSE, "')'");
    }
    tin_astparser_ignorenewlines(parser, true);
    body = tin_astparser_parsestatement(parser);
    return (TinAstExpression*)tin_ast_make_whileexpr(parser->state, line, condition, body);
}

static TinAstExpression* tin_astparser_rulefunction(TinAstParser* parser, bool canassign)
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
    isexport = parser->previous.type == TINTOK_KWEXPORT;
    fnamestr = "<anonymous>";
    fnamelen = strlen(fnamestr);
    if(isexport)
    {
        tin_astparser_consume(parser, TINTOK_KWFUNCTION, "'function' after 'export'");
    }
    line = parser->previous.line;
    if(tin_astparser_check(parser, TINTOK_IDENT))
    {
        tin_astparser_consume(parser, TINTOK_IDENT, "function name");
        fnamestr = parser->previous.start;
        fnamelen = parser->previous.length;
    }
    if(tin_astparser_match(parser, TINTOK_DOT) || islambda)
    //if(tin_astparser_match(parser, TINTOK_DOT))
    {
        to = NULL;
        if(tin_astparser_check(parser, TINTOK_IDENT))
        {
            tin_astparser_consume(parser, TINTOK_IDENT, "function name");
        }
        lambda = tin_ast_make_lambdaexpr(parser->state, line);
        //if(islambda)
        /*
        {
            to = tin_ast_make_setexpr(
                parser->state,
                line,
                (TinAstExpression*)tin_ast_make_varexpr(parser->state, line, fnamestr, fnamelen),
                parser->previous.start,
                parser->previous.length,
                (TinAstExpression*)lambda
            );
        }
        */
        tin_astparser_consume(parser, TINTOK_PARENOPEN, "'(' after function name");
        tin_astparser_initcompiler(parser, &compiler);
        tin_astparser_beginscope(parser);
        tin_astparser_parseparameters(parser, &lambda->parameters);
        if(lambda->parameters.count > 255)
        {
            tin_astparser_raiseerror(parser, "function cannot have more than 255 arguments, got %i", (int)lambda->parameters.count);
        }
        tin_astparser_consume(parser, TINTOK_PARENCLOSE, "')' after function arguments");
        tin_astparser_ignorenewlines(parser, true);
        lambda->body = tin_astparser_parsestatement(parser);
        tin_astparser_endscope(parser);
        tin_astparser_endcompiler(parser, &compiler);
        if(islambda)
        {
            return (TinAstExpression*)lambda;
        }
        return (TinAstExpression*)tin_ast_make_exprstmt(parser->state, line, (TinAstExpression*)to);
    }
    function = tin_ast_make_funcexpr(parser->state, line, fnamestr, fnamelen);
    function->exported = isexport;
    tin_astparser_consume(parser, TINTOK_PARENOPEN, "'(' after function name");
    tin_astparser_initcompiler(parser, &compiler);
    tin_astparser_beginscope(parser);
    tin_astparser_parseparameters(parser, &function->parameters);
    if(function->parameters.count > 255)
    {
        tin_astparser_raiseerror(parser, "function cannot have more than 255 arguments, got %i", (int)function->parameters.count);
    }
    tin_astparser_consume(parser, TINTOK_PARENCLOSE, "')' after function arguments");
    function->body = tin_astparser_parsestatement(parser);
    tin_astparser_endscope(parser);
    tin_astparser_endcompiler(parser, &compiler);
    return (TinAstExpression*)function;
}

static TinAstExpression* tin_astparser_parsereturn(TinAstParser* parser)
{
    size_t line;
    TinAstExpression* expression;
    line = parser->previous.line;
    expression = NULL;
    if(!tin_astparser_check(parser, TINTOK_NEWLINE) && !tin_astparser_check(parser, TINTOK_BRACECLOSE))
    {
        expression = tin_astparser_parseexpression(parser, true);
    }
    return (TinAstExpression*)tin_ast_make_returnexpr(parser->state, line, expression);
}

static TinAstExpression* tin_astparser_parsefield(TinAstParser* parser, TinString* name, bool isstatic)
{
    size_t line;
    TinAstExpression* getter;
    TinAstExpression* setter;
    line = parser->previous.line;
    getter = NULL;
    setter = NULL;
    if(tin_astparser_match(parser, TINTOK_ARROW))
    {
        getter = tin_astparser_parsestatement(parser);
    }
    else
    {
        tin_astparser_match(parser, TINTOK_BRACEOPEN);// Will be TINTOK_BRACEOPEN, otherwise this method won't be called
        tin_astparser_ignorenewlines(parser, true);
        if(tin_astparser_match(parser, TINTOK_KWGET))
        {
            tin_astparser_match(parser, TINTOK_ARROW);// Ignore it if it's present
            getter = tin_astparser_parsestatement(parser);
        }
        tin_astparser_ignorenewlines(parser, true);
        if(tin_astparser_match(parser, TINTOK_KWSET))
        {
            tin_astparser_match(parser, TINTOK_ARROW);// Ignore it if it's present
            setter = tin_astparser_parsestatement(parser);
        }
        if(getter == NULL && setter == NULL)
        {
            tin_astparser_raiseerror(parser, "expected declaration of either getter or setter, got none");
        }
        tin_astparser_ignorenewlines(parser, true);
        tin_astparser_consume(parser, TINTOK_BRACECLOSE, "'}' after field declaration");
    }
    return (TinAstExpression*)tin_ast_make_fieldexpr(parser->state, line, name, getter, setter, isstatic);
}

static TinAstExpression* tin_astparser_parsemethod(TinAstParser* parser, bool isstatic)
{
    size_t i;
    TinAstCompiler compiler;
    TinAstMethodExpr* method;
    TinString* name;
    if(tin_astparser_match(parser, TINTOK_KWSTATIC))
    {
        isstatic = true;
    }
    name = NULL;
    if(tin_astparser_match(parser, TINTOK_KWOPERATOR))
    {
        if(isstatic)
        {
            tin_astparser_raiseerror(parser, "operator methods cannot be static or defined in static classes");
        }
        i = 0;
        while(operators[i] != TINTOK_EOF)
        {
            if(tin_astparser_match(parser, operators[i]))
            {
                break;
            }
            i++;
        }
        if(parser->previous.type == TINTOK_BRACKETOPEN)
        {
            tin_astparser_consume(parser, TINTOK_BRACKETCLOSE, "']' after '[' in op method declaration");
            name = tin_string_copy(parser->state, "[]", 2);
        }
        else
        {
            name = tin_string_copy(parser->state, parser->previous.start, parser->previous.length);
        }
    }
    else
    {
        tin_astparser_consume(parser, TINTOK_IDENT, "method name");
        name = tin_string_copy(parser->state, parser->previous.start, parser->previous.length);
        if(tin_astparser_check(parser, TINTOK_BRACEOPEN) || tin_astparser_check(parser, TINTOK_ARROW))
        {
            return tin_astparser_parsefield(parser, name, isstatic);
        }
    }
    method = tin_ast_make_methodexpr(parser->state, parser->previous.line, name, isstatic);
    tin_astparser_initcompiler(parser, &compiler);
    tin_astparser_beginscope(parser);
    tin_astparser_consume(parser, TINTOK_PARENOPEN, "'(' after method name");
    tin_astparser_parseparameters(parser, &method->parameters);
    if(method->parameters.count > 255)
    {
        tin_astparser_raiseerror(parser, "function cannot have more than 255 arguments, got %i", (int)method->parameters.count);
    }
    tin_astparser_consume(parser, TINTOK_PARENCLOSE, "')' after method arguments");
    method->body = tin_astparser_parsestatement(parser);
    tin_astparser_endscope(parser);
    tin_astparser_endcompiler(parser, &compiler);
    return (TinAstExpression*)method;
}

static TinAstExpression* tin_astparser_parseclass(TinAstParser* parser)
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
    line = parser->previous.line;
    isstatic = parser->previous.type == TINTOK_KWSTATIC;
    if(isstatic)
    {
        tin_astparser_consume(parser, TINTOK_KWCLASS, "'class' after 'static'");
    }
    tin_astparser_consume(parser, TINTOK_IDENT, "class name after 'class'");
    name = tin_string_copy(parser->state, parser->previous.start, parser->previous.length);
    super = NULL;
    if(tin_astparser_match(parser, TINTOK_COLON))
    {
        tin_astparser_consume(parser, TINTOK_IDENT, "super class name after ':'");
        super = tin_string_copy(parser->state, parser->previous.start, parser->previous.length);
        if(super == name)
        {
            tin_astparser_raiseerror(parser, "class cannot inherit itself");
        }
    }
    klass = tin_ast_make_classexpr(parser->state, line, name, super);
    tin_astparser_ignorenewlines(parser, true);
    tin_astparser_consume(parser, TINTOK_BRACEOPEN, "'{' before class body");
    tin_astparser_ignorenewlines(parser, true);
    finishedparsingfields = false;
    while(!tin_astparser_check(parser, TINTOK_BRACECLOSE))
    {
        fieldisstatic = false;
        if(tin_astparser_match(parser, TINTOK_KWSTATIC))
        {
            fieldisstatic = true;
            if(tin_astparser_match(parser, TINTOK_KWVAR))
            {
                if(finishedparsingfields)
                {
                    tin_astparser_raiseerror(parser, "all static fields must be defined before the methods");
                }
                var = tin_astparser_parsevar_declaration(parser, true);
                if(var != NULL)
                {
                    tin_exprlist_push(parser->state, &klass->fields, var);
                }
                tin_astparser_ignorenewlines(parser, true);
                continue;
            }
            else
            {
                finishedparsingfields = true;
            }
        }
        method = tin_astparser_parsemethod(parser, isstatic || fieldisstatic);
        if(method != NULL)
        {
            tin_exprlist_push(parser->state, &klass->fields, method);
        }
        tin_astparser_ignorenewlines(parser, true);
    }
    tin_astparser_consume(parser, TINTOK_BRACECLOSE, "'}' after class body");
    return (TinAstExpression*)klass;
}

static void tin_astparser_sync(TinAstParser* parser)
{
    parser->panic_mode = false;
    while(parser->current.type != TINTOK_EOF)
    {
        if(parser->previous.type == TINTOK_NEWLINE)
        {
            longjmp(prs_jmpbuffer, 1);
            return;
        }
        switch(parser->current.type)
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
                tin_astparser_advance(parser);
            }
        }
    }
}

static TinAstExpression* tin_astparser_parsedeclaration(TinAstParser* parser)
{
    TinAstExpression* statement;
    statement = NULL;
    if(tin_astparser_match(parser, TINTOK_KWCLASS) || tin_astparser_match(parser, TINTOK_KWSTATIC))
    {
        statement = tin_astparser_parseclass(parser);
    }
    else
    {
        statement = tin_astparser_parsestatement(parser);
    }
    return statement;
}

bool tin_astparser_parsesource(TinAstParser* parser, const char* filename, const char* source, TinAstExprList* statements)
{
    TinAstCompiler compiler;
    TinAstExpression* statement;
    parser->haderror = false;
    parser->panic_mode = false;
    tin_astlex_init(parser->state, parser->state->scanner, filename, source);
    tin_astparser_initcompiler(parser, &compiler);
    tin_astparser_advance(parser);
    tin_astparser_ignorenewlines(parser, true);
    if(!prs_is_at_end(parser))
    {
        do
        {
            statement = tin_astparser_parsedeclaration(parser);
            if(statement != NULL)
            {
                tin_exprlist_push(parser->state, statements, statement);
            }
            if(!tin_astparser_matchnewline(parser))
            {
                if(tin_astparser_match(parser, TINTOK_EOF))
                {
                    break;
                }
            }
        } while(!prs_is_at_end(parser));
    }
    return parser->haderror || parser->state->scanner->haderror;
}
