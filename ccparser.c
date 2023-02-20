
#include <stdlib.h>
#include <setjmp.h>
#include "priv.h"

static jmp_buf prs_jmpbuffer;
static LitAstParseRule rules[LITTOK_EOF + 1];


static LitAstTokType operators[]=
{
    LITTOK_PLUS, LITTOK_MINUS, LITTOK_STAR, LITTOK_PERCENT, LITTOK_SLASH,
    LITTOK_SHARP, LITTOK_BANG, LITTOK_LESS, LITTOK_LESS_EQUAL, LITTOK_GREATER,
    LITTOK_GREATER_EQUAL, LITTOK_EQUAL_EQUAL, LITTOK_LEFT_BRACKET, LITTOK_EOF
};


static bool didsetuprules;
static void lit_astparser_setuprules();
static void lit_astparser_sync(LitAstParser* parser);

static LitAstExpression *lit_astparser_parseblock(LitAstParser *parser);
static LitAstExpression *lit_astparser_parseprecedence(LitAstParser *parser, LitAstPrecedence precedence, bool err, bool ignsemi);
static LitAstExpression *lit_astparser_parselambda(LitAstParser *parser, LitAstFunctionExpr *lambda);
static void lit_astparser_parseparameters(LitAstParser *parser, LitAstParamList *parameters);
static LitAstExpression *lit_astparser_parseexpression(LitAstParser *parser, bool ignsemi);
static LitAstExpression *lit_astparser_parsevar_declaration(LitAstParser *parser, bool ignsemi);
static LitAstExpression *lit_astparser_parseif(LitAstParser *parser);
static LitAstExpression *lit_astparser_parsefor(LitAstParser *parser);
static LitAstExpression *lit_astparser_parsewhile(LitAstParser *parser);
static LitAstExpression *lit_astparser_parsereturn(LitAstParser *parser);
static LitAstExpression *lit_astparser_parsefield(LitAstParser *parser, LitString *name, bool is_static);
static LitAstExpression *lit_astparser_parsemethod(LitAstParser *parser, bool is_static);
static LitAstExpression *lit_astparser_parseclass(LitAstParser *parser);
static LitAstExpression *lit_astparser_parsestatement(LitAstParser *parser);
static LitAstExpression *lit_astparser_parsedeclaration(LitAstParser *parser);

static LitAstExpression *lit_astparser_rulenumber(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_rulegroupingorlambda(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_rulecall(LitAstParser *parser, LitAstExpression *prev, bool canassign);
static LitAstExpression *lit_astparser_ruleunary(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_rulebinary(LitAstParser *parser, LitAstExpression *prev, bool canassign);
static LitAstExpression *lit_astparser_ruleand(LitAstParser *parser, LitAstExpression *prev, bool canassign);
static LitAstExpression *lit_astparser_ruleor(LitAstParser *parser, LitAstExpression *prev, bool canassign);
static LitAstExpression *lit_astparser_rulenull_filter(LitAstParser *parser, LitAstExpression *prev, bool canassign);
static LitAstExpression *lit_astparser_rulecompound(LitAstParser *parser, LitAstExpression *prev, bool canassign);
static LitAstExpression *lit_astparser_ruleliteral(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_rulestring(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_ruleinterpolation(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_ruleobject(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_rulevarexprbase(LitAstParser *parser, bool canassign, bool isnew);
static LitAstExpression *lit_astparser_rulevarexpr(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_rulenewexpr(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_ruledot(LitAstParser *parser, LitAstExpression *previous, bool canassign);
static LitAstExpression *lit_astparser_rulerange(LitAstParser *parser, LitAstExpression *previous, bool canassign);
static LitAstExpression *lit_astparser_ruleternary(LitAstParser *parser, LitAstExpression *previous, bool canassign);
static LitAstExpression *lit_astparser_rulearray(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_rulesubscript(LitAstParser *parser, LitAstExpression *previous, bool canassign);
static LitAstExpression *lit_astparser_rulethis(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_rulesuper(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_rulereference(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_rulenothing(LitAstParser *parser, bool canassign);
static LitAstExpression *lit_astparser_rulefunction(LitAstParser *parser, bool canassign);


static void lit_astparser_setuprules()
{
    rules[LITTOK_LEFT_PAREN] = (LitAstParseRule){ lit_astparser_rulegroupingorlambda, lit_astparser_rulecall, LITPREC_CALL };
    rules[LITTOK_PLUS] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_TERM };
    rules[LITTOK_MINUS] = (LitAstParseRule){ lit_astparser_ruleunary, lit_astparser_rulebinary, LITPREC_TERM };
    rules[LITTOK_BANG] = (LitAstParseRule){ lit_astparser_ruleunary, lit_astparser_rulebinary, LITPREC_TERM };
    rules[LITTOK_STAR] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_FACTOR };
    rules[LITTOK_STAR_STAR] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_FACTOR };
    rules[LITTOK_SLASH] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_FACTOR };
    rules[LITTOK_SHARP] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_FACTOR };
    rules[LITTOK_STAR] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_FACTOR };
    rules[LITTOK_STAR] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_FACTOR };
    rules[LITTOK_BAR] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_BOR };
    rules[LITTOK_AMPERSAND] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_BAND };
    rules[LITTOK_TILDE] = (LitAstParseRule){ lit_astparser_ruleunary, NULL, LITPREC_UNARY };
    rules[LITTOK_CARET] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_BOR };
    rules[LITTOK_LESS_LESS] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_SHIFT };
    rules[LITTOK_GREATER_GREATER] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_SHIFT };
    rules[LITTOK_PERCENT] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_FACTOR };
    rules[LITTOK_IS] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_IS };
    rules[LITTOK_NUMBER] = (LitAstParseRule){ lit_astparser_rulenumber, NULL, LITPREC_NONE };
    rules[LITTOK_TRUE] = (LitAstParseRule){ lit_astparser_ruleliteral, NULL, LITPREC_NONE };
    rules[LITTOK_FALSE] = (LitAstParseRule){ lit_astparser_ruleliteral, NULL, LITPREC_NONE };
    rules[LITTOK_NULL] = (LitAstParseRule){ lit_astparser_ruleliteral, NULL, LITPREC_NONE };
    rules[LITTOK_BANG_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_EQUALITY };
    rules[LITTOK_EQUAL_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_EQUALITY };
    rules[LITTOK_GREATER] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_COMPARISON };
    rules[LITTOK_GREATER_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_COMPARISON };
    rules[LITTOK_LESS] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_COMPARISON };
    rules[LITTOK_LESS_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulebinary, LITPREC_COMPARISON };
    rules[LITTOK_STRING] = (LitAstParseRule){ lit_astparser_rulestring, NULL, LITPREC_NONE };
    rules[LITTOK_INTERPOLATION] = (LitAstParseRule){ lit_astparser_ruleinterpolation, NULL, LITPREC_NONE };
    rules[LITTOK_IDENTIFIER] = (LitAstParseRule){ lit_astparser_rulevarexpr, NULL, LITPREC_NONE };
    rules[LITTOK_NEW] = (LitAstParseRule){ lit_astparser_rulenewexpr, NULL, LITPREC_NONE };
    rules[LITTOK_PLUS_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulecompound, LITPREC_COMPOUND };
    rules[LITTOK_MINUS_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulecompound, LITPREC_COMPOUND };
    rules[LITTOK_STAR_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulecompound, LITPREC_COMPOUND };
    rules[LITTOK_SLASH_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulecompound, LITPREC_COMPOUND };
    rules[LITTOK_SHARP_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulecompound, LITPREC_COMPOUND };
    rules[LITTOK_PERCENT_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulecompound, LITPREC_COMPOUND };
    rules[LITTOK_CARET_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulecompound, LITPREC_COMPOUND };
    rules[LITTOK_BAR_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulecompound, LITPREC_COMPOUND };
    rules[LITTOK_AMPERSAND_EQUAL] = (LitAstParseRule){ NULL, lit_astparser_rulecompound, LITPREC_COMPOUND };
    rules[LITTOK_PLUS_PLUS] = (LitAstParseRule){ NULL, lit_astparser_rulecompound, LITPREC_COMPOUND };
    rules[LITTOK_MINUS_MINUS] = (LitAstParseRule){ NULL, lit_astparser_rulecompound, LITPREC_COMPOUND };
    rules[LITTOK_AMPERSAND_AMPERSAND] = (LitAstParseRule){ NULL, lit_astparser_ruleand, LITPREC_AND };
    rules[LITTOK_BAR_BAR] = (LitAstParseRule){ NULL, lit_astparser_ruleor, LITPREC_AND };
    rules[LITTOK_QUESTION_QUESTION] = (LitAstParseRule){ NULL, lit_astparser_rulenull_filter, LITPREC_NULL };
    rules[LITTOK_DOT] = (LitAstParseRule){ NULL, lit_astparser_ruledot, LITPREC_CALL };
    rules[LITTOK_SMALL_ARROW] = (LitAstParseRule){ NULL, lit_astparser_ruledot, LITPREC_CALL };
    rules[LITTOK_DOT_DOT] = (LitAstParseRule){ NULL, lit_astparser_rulerange, LITPREC_RANGE };
    rules[LITTOK_DOT_DOT_DOT] = (LitAstParseRule){ lit_astparser_rulevarexpr, NULL, LITPREC_ASSIGNMENT };
    rules[LITTOK_LEFT_BRACKET] = (LitAstParseRule){ lit_astparser_rulearray, lit_astparser_rulesubscript, LITPREC_NONE };
    rules[LITTOK_LEFT_BRACE] = (LitAstParseRule){ lit_astparser_ruleobject, NULL, LITPREC_NONE };
    rules[LITTOK_THIS] = (LitAstParseRule){ lit_astparser_rulethis, NULL, LITPREC_NONE };
    rules[LITTOK_SUPER] = (LitAstParseRule){ lit_astparser_rulesuper, NULL, LITPREC_NONE };
    rules[LITTOK_QUESTION] = (LitAstParseRule){ NULL, lit_astparser_ruleternary, LITPREC_EQUALITY };
    rules[LITTOK_REF] = (LitAstParseRule){ lit_astparser_rulereference, NULL, LITPREC_NONE };
    rules[LITTOK_FUNCTION] = (LitAstParseRule){lit_astparser_rulefunction, NULL, LITPREC_NONE};
    rules[LITTOK_SEMICOLON] = (LitAstParseRule){lit_astparser_rulenothing, NULL, LITPREC_NONE};
}


const char* lit_astparser_token2name(int t)
{
    switch(t)
    {
        case LITTOK_NEW_LINE: return "LITTOK_NEW_LINE";
        case LITTOK_LEFT_PAREN: return "LITTOK_LEFT_PAREN";
        case LITTOK_RIGHT_PAREN: return "LITTOK_RIGHT_PAREN";
        case LITTOK_LEFT_BRACE: return "LITTOK_LEFT_BRACE";
        case LITTOK_RIGHT_BRACE: return "LITTOK_RIGHT_BRACE";
        case LITTOK_LEFT_BRACKET: return "LITTOK_LEFT_BRACKET";
        case LITTOK_RIGHT_BRACKET: return "LITTOK_RIGHT_BRACKET";
        case LITTOK_COMMA: return "LITTOK_COMMA";
        case LITTOK_SEMICOLON: return "LITTOK_SEMICOLON";
        case LITTOK_COLON: return "LITTOK_COLON";
        case LITTOK_BAR_EQUAL: return "LITTOK_BAR_EQUAL";
        case LITTOK_BAR: return "LITTOK_BAR";
        case LITTOK_BAR_BAR: return "LITTOK_BAR_BAR";
        case LITTOK_AMPERSAND_EQUAL: return "LITTOK_AMPERSAND_EQUAL";
        case LITTOK_AMPERSAND: return "LITTOK_AMPERSAND";
        case LITTOK_AMPERSAND_AMPERSAND: return "LITTOK_AMPERSAND_AMPERSAND";
        case LITTOK_BANG: return "LITTOK_BANG";
        case LITTOK_BANG_EQUAL: return "LITTOK_BANG_EQUAL";
        case LITTOK_EQUAL: return "LITTOK_EQUAL";
        case LITTOK_EQUAL_EQUAL: return "LITTOK_EQUAL_EQUAL";
        case LITTOK_GREATER: return "LITTOK_GREATER";
        case LITTOK_GREATER_EQUAL: return "LITTOK_GREATER_EQUAL";
        case LITTOK_GREATER_GREATER: return "LITTOK_GREATER_GREATER";
        case LITTOK_LESS: return "LITTOK_LESS";
        case LITTOK_LESS_EQUAL: return "LITTOK_LESS_EQUAL";
        case LITTOK_LESS_LESS: return "LITTOK_LESS_LESS";
        case LITTOK_PLUS: return "LITTOK_PLUS";
        case LITTOK_PLUS_EQUAL: return "LITTOK_PLUS_EQUAL";
        case LITTOK_PLUS_PLUS: return "LITTOK_PLUS_PLUS";
        case LITTOK_MINUS: return "LITTOK_MINUS";
        case LITTOK_MINUS_EQUAL: return "LITTOK_MINUS_EQUAL";
        case LITTOK_MINUS_MINUS: return "LITTOK_MINUS_MINUS";
        case LITTOK_STAR: return "LITTOK_STAR";
        case LITTOK_STAR_EQUAL: return "LITTOK_STAR_EQUAL";
        case LITTOK_STAR_STAR: return "LITTOK_STAR_STAR";
        case LITTOK_SLASH: return "LITTOK_SLASH";
        case LITTOK_SLASH_EQUAL: return "LITTOK_SLASH_EQUAL";
        case LITTOK_QUESTION: return "LITTOK_QUESTION";
        case LITTOK_QUESTION_QUESTION: return "LITTOK_QUESTION_QUESTION";
        case LITTOK_PERCENT: return "LITTOK_PERCENT";
        case LITTOK_PERCENT_EQUAL: return "LITTOK_PERCENT_EQUAL";
        case LITTOK_ARROW: return "LITTOK_ARROW";
        case LITTOK_SMALL_ARROW: return "LITTOK_SMALL_ARROW";
        case LITTOK_TILDE: return "LITTOK_TILDE";
        case LITTOK_CARET: return "LITTOK_CARET";
        case LITTOK_CARET_EQUAL: return "LITTOK_CARET_EQUAL";
        case LITTOK_DOT: return "LITTOK_DOT";
        case LITTOK_DOT_DOT: return "LITTOK_DOT_DOT";
        case LITTOK_DOT_DOT_DOT: return "LITTOK_DOT_DOT_DOT";
        case LITTOK_SHARP: return "LITTOK_SHARP";
        case LITTOK_SHARP_EQUAL: return "LITTOK_SHARP_EQUAL";
        case LITTOK_IDENTIFIER: return "LITTOK_IDENTIFIER";
        case LITTOK_STRING: return "LITTOK_STRING";
        case LITTOK_INTERPOLATION: return "LITTOK_INTERPOLATION";
        case LITTOK_NUMBER: return "LITTOK_NUMBER";
        case LITTOK_CLASS: return "LITTOK_CLASS";
        case LITTOK_ELSE: return "LITTOK_ELSE";
        case LITTOK_FALSE: return "LITTOK_FALSE";
        case LITTOK_FOR: return "LITTOK_FOR";
        case LITTOK_FUNCTION: return "LITTOK_FUNCTION";
        case LITTOK_IF: return "LITTOK_IF";
        case LITTOK_NULL: return "LITTOK_NULL";
        case LITTOK_RETURN: return "LITTOK_RETURN";
        case LITTOK_SUPER: return "LITTOK_SUPER";
        case LITTOK_THIS: return "LITTOK_THIS";
        case LITTOK_TRUE: return "LITTOK_TRUE";
        case LITTOK_VAR: return "LITTOK_VAR";
        case LITTOK_WHILE: return "LITTOK_WHILE";
        case LITTOK_CONTINUE: return "LITTOK_CONTINUE";
        case LITTOK_BREAK: return "LITTOK_BREAK";
        case LITTOK_NEW: return "LITTOK_NEW";
        case LITTOK_EXPORT: return "LITTOK_EXPORT";
        case LITTOK_IS: return "LITTOK_IS";
        case LITTOK_STATIC: return "LITTOK_STATIC";
        case LITTOK_OPERATOR: return "LITTOK_OPERATOR";
        case LITTOK_GET: return "LITTOK_GET";
        case LITTOK_SET: return "LITTOK_SET";
        case LITTOK_IN: return "LITTOK_IN";
        case LITTOK_CONST: return "LITTOK_CONST";
        case LITTOK_REF: return "LITTOK_REF";
        case LITTOK_ERROR: return "LITTOK_ERROR";
        case LITTOK_EOF: return "LITTOK_EOF";
        default:
            break;
    }
    return "?unknown?";
}


static void lit_astparser_initcompiler(LitAstParser* parser, LitAstCompiler* compiler)
{
    compiler->scope_depth = 0;
    compiler->function = NULL;
    compiler->enclosing = (struct LitAstCompiler*)parser->compiler;

    parser->compiler = compiler;
}

static void lit_astparser_endcompiler(LitAstParser* parser, LitAstCompiler* compiler)
{
    parser->compiler = (LitAstCompiler*)compiler->enclosing;
}

static void lit_astparser_beginscope(LitAstParser* parser)
{
    parser->compiler->scope_depth++;
}

static void lit_astparser_endscope(LitAstParser* parser)
{
    parser->compiler->scope_depth--;
}

static LitAstParseRule* lit_astparser_getrule(LitAstTokType type)
{
    return &rules[type];
}

static inline bool prs_is_at_end(LitAstParser* parser)
{
    return parser->current.type == LITTOK_EOF;
}

void lit_astparser_init(LitState* state, LitAstParser* parser)
{
    if(!didsetuprules)
    {
        didsetuprules = true;
        lit_astparser_setuprules();
    }
    parser->state = state;
    parser->had_error = false;
    parser->panic_mode = false;
}

void lit_astparser_destroy(LitAstParser* parser)
{
    (void)parser;
}

static void lit_astparser_raisestring(LitAstParser* parser, LitAstToken* token, const char* message)
{
    (void)token;
    if(parser->panic_mode)
    {
        return;
    }
    lit_state_raiseerror(parser->state, COMPILE_ERROR, message);
    parser->had_error = true;
    lit_astparser_sync(parser);
}

static void lit_astparser_raiseat(LitAstParser* parser, LitAstToken* token, const char* fmt, va_list args)
{
    lit_astparser_raisestring(parser, token, lit_vformat_error(parser->state, token->line, fmt, args)->chars);
}

static void lit_astparser_raiseatcurrent(LitAstParser* parser, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    lit_astparser_raiseat(parser, &parser->current, fmt, args);
    va_end(args);
}

static void lit_astparser_raiseerror(LitAstParser* parser, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    lit_astparser_raiseat(parser, &parser->previous, fmt, args);
    va_end(args);
}

static void lit_astparser_advance(LitAstParser* parser)
{
    parser->previous = parser->current;

    while(true)
    {
        parser->current = lit_astlex_scantoken(parser->state->scanner);
        if(parser->current.type != LITTOK_ERROR)
        {
            break;
        }
        lit_astparser_raisestring(parser, &parser->current, parser->current.start);
    }
}

static bool lit_astparser_check(LitAstParser* parser, LitAstTokType type)
{
    return parser->current.type == type;
}

static bool lit_astparser_match(LitAstParser* parser, LitAstTokType type)
{
    if(parser->current.type == type)
    {
        lit_astparser_advance(parser);
        return true;
    }
    return false;
}

static bool lit_astparser_matchnewline(LitAstParser* parser)
{
    while(true)
    {
        if(!lit_astparser_match(parser, LITTOK_NEW_LINE))
        {
            return false;
        }
    }
    return true;
}

static void lit_astparser_ignorenewlines(LitAstParser* parser, bool checksemi)
{
    (void)checksemi;
    lit_astparser_matchnewline(parser);
}

static void lit_astparser_consume(LitAstParser* parser, LitAstTokType type, const char* onerror)
{
    bool line;
    size_t olen;
    const char* fmt;
    const char* otext;
    if(parser->current.type == type)
    {
        lit_astparser_advance(parser);
        return;
    }
    //fprintf(stderr, "in lit_astparser_consume: failed?\n");
    line = parser->previous.type == LITTOK_NEW_LINE;
    olen = (line ? 8 : parser->previous.length);
    otext = (line ? "new line" : parser->previous.start);
    fmt = lit_format_error(parser->state, parser->current.line, "expected %s, got '%.*s'", onerror, olen, otext)->chars;
    lit_astparser_raisestring(parser, &parser->current,fmt);
}

static LitAstExpression* lit_astparser_parseblock(LitAstParser* parser)
{
    LitAstBlockExpr* statement;
    lit_astparser_beginscope(parser);
    statement = lit_ast_make_blockexpr(parser->state, parser->previous.line);
    while(true)
    {
        lit_astparser_ignorenewlines(parser, true);
        if(lit_astparser_check(parser, LITTOK_RIGHT_BRACE) || lit_astparser_check(parser, LITTOK_EOF))
        {
            break;
        }
        lit_astparser_ignorenewlines(parser, true);
        lit_exprlist_push(parser->state, &statement->statements, lit_astparser_parsestatement(parser));
        lit_astparser_ignorenewlines(parser, true);
    }
    lit_astparser_ignorenewlines(parser, true);
    lit_astparser_consume(parser, LITTOK_RIGHT_BRACE, "'}'");
    lit_astparser_ignorenewlines(parser, true);
    lit_astparser_endscope(parser);
    return (LitAstExpression*)statement;
}

static LitAstExpression* lit_astparser_parseprecedence(LitAstParser* parser, LitAstPrecedence precedence, bool err, bool ignsemi)
{
    bool new_line;
    bool prevnewline;
    bool parserprevnewline;
    bool canassign;
    LitAstExpression* expr;
    LitAstParsePrefixFn prefix_rule;
    LitAstParseInfixFn infix_rule;
    LitAstToken previous;
    (void)new_line;
    prevnewline = false;
    previous = parser->previous;
    lit_astparser_advance(parser);
    prefix_rule = lit_astparser_getrule(parser->previous.type)->prefix;
    if(prefix_rule == NULL)
    {
        //if(parser->previous.type != parser->current.type)
        {
            // todo: file start
            new_line = previous.start != NULL && *previous.start == '\n';
            parserprevnewline = parser->previous.start != NULL && *parser->previous.start == '\n';
            lit_astparser_raiseerror(parser, "expected expression after '%.*s', got '%.*s'",
                (prevnewline ? 8 : previous.length),
                (prevnewline ? "new line" : previous.start),
                (parserprevnewline ? 8 : parser->previous.length),
                (parserprevnewline ? "new line" : parser->previous.start)
            );
            return NULL;
        }
    }
    canassign = precedence <= LITPREC_ASSIGNMENT;
    expr = prefix_rule(parser, canassign);
    lit_astparser_ignorenewlines(parser, ignsemi);
    while(precedence <= lit_astparser_getrule(parser->current.type)->precedence)
    {
        lit_astparser_advance(parser);
        infix_rule = lit_astparser_getrule(parser->previous.type)->infix;
        expr = infix_rule(parser, expr, canassign);
    }
    if(err && canassign && lit_astparser_match(parser, LITTOK_EQUAL))
    {
        lit_astparser_raiseerror(parser, "invalid assigment target");
    }
    return expr;
}

static LitAstExpression* lit_astparser_rulenumber(LitAstParser* parser, bool canassign)
{
    (void)canassign;
    return (LitAstExpression*)lit_ast_make_literalexpr(parser->state, parser->previous.line, parser->previous.value);
}

static LitAstExpression* lit_astparser_parselambda(LitAstParser* parser, LitAstFunctionExpr* lambda)
{
    lambda->body = lit_astparser_parsestatement(parser);
    return (LitAstExpression*)lambda;
}

static void lit_astparser_parseparameters(LitAstParser* parser, LitAstParamList* parameters)
{
    bool haddefault;
    size_t arglength;
    const char* argname;
    LitAstExpression* default_value;
    haddefault = false;
    while(!lit_astparser_check(parser, LITTOK_RIGHT_PAREN))
    {
        // Vararg ...
        if(lit_astparser_match(parser, LITTOK_DOT_DOT_DOT))
        {
            lit_paramlist_push(parser->state, parameters, (LitAstParameter){ "...", 3, NULL });
            return;
        }
        lit_astparser_consume(parser, LITTOK_IDENTIFIER, "argument name");
        argname = parser->previous.start;
        arglength = parser->previous.length;
        default_value = NULL;
        if(lit_astparser_match(parser, LITTOK_EQUAL))
        {
            haddefault = true;
            default_value = lit_astparser_parseexpression(parser, true);
        }
        else if(haddefault)
        {
            lit_astparser_raiseerror(parser, "default arguments must always be in the end of the argument list.");
        }
        lit_paramlist_push(parser->state, parameters, (LitAstParameter){ argname, arglength, default_value });
        if(!lit_astparser_match(parser, LITTOK_COMMA))
        {
            break;
        }
    }
}

/*
* this is extremely not working at all.
*/
static LitAstExpression* lit_astparser_rulegroupingorlambda(LitAstParser* parser, bool canassign)
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
    LitAstExpression* expression;
    LitAstExpression* default_value;
    LitAstScanner* scanner;
    (void)canassign;
    (void)hadarrow;
    (void)had_array;
    hadarrow = false;
    if(lit_astparser_match(parser, LITTOK_RIGHT_PAREN))
    {
        lit_astparser_consume(parser, LITTOK_ARROW, "=> after lambda arguments");
        return lit_astparser_parselambda(parser, lit_ast_make_lambdaexpr(parser->state, parser->previous.line));
    }
    start = parser->previous.start;
    line = parser->previous.line;
    if(lit_astparser_match(parser, LITTOK_IDENTIFIER) || lit_astparser_match(parser, LITTOK_DOT_DOT_DOT))
    {
        LitState* state = parser->state;
        firstargstart = parser->previous.start;
        firstarglength = parser->previous.length;
        if(lit_astparser_match(parser, LITTOK_COMMA) || (lit_astparser_match(parser, LITTOK_RIGHT_PAREN) && lit_astparser_match(parser, LITTOK_ARROW)))
        {
            had_array = parser->previous.type == LITTOK_ARROW;
            hadvararg= parser->previous.type == LITTOK_DOT_DOT_DOT;
            // This is a lambda
            LitAstFunctionExpr* lambda = lit_ast_make_lambdaexpr(state, line);
            LitAstExpression* defvalue = NULL;
            haddefault = lit_astparser_match(parser, LITTOK_EQUAL);
            if(haddefault)
            {
                defvalue = lit_astparser_parseexpression(parser, true);
            }
            lit_paramlist_push(state, &lambda->parameters, (LitAstParameter){ firstargstart, firstarglength, defvalue });
            if(!hadvararg && parser->previous.type == LITTOK_COMMA)
            {
                do
                {
                    stop = false;
                    if(lit_astparser_match(parser, LITTOK_DOT_DOT_DOT))
                    {
                        stop = true;
                    }
                    else
                    {
                        lit_astparser_consume(parser, LITTOK_IDENTIFIER, "argument name");
                    }

                    argname = parser->previous.start;
                    arglength = parser->previous.length;
                    default_value = NULL;
                    if(lit_astparser_match(parser, LITTOK_EQUAL))
                    {
                        default_value = lit_astparser_parseexpression(parser, true);
                        haddefault = true;
                    }
                    else if(haddefault)
                    {
                        lit_astparser_raiseerror(parser, "default arguments must always be in the end of the argument list.");
                    }
                    lit_paramlist_push(state, &lambda->parameters, (LitAstParameter){ argname, arglength, default_value });
                    if(stop)
                    {
                        break;
                    }
                } while(lit_astparser_match(parser, LITTOK_COMMA));
            }
            #if 0
            if(!hadarrow)
            {
                lit_astparser_consume(parser, LITTOK_RIGHT_PAREN, "')' after lambda parameters");
                lit_astparser_consume(parser, LITTOK_ARROW, "=> after lambda parameters");
            }
            #endif
            return lit_astparser_parselambda(parser, lambda);
        }
        else
        {
            // Ouch, this was a grouping with a single identifier
            scanner = state->scanner;
            scanner->current = start;
            scanner->line = line;
            parser->current = lit_astlex_scantoken(scanner);
            lit_astparser_advance(parser);
        }
    }
    expression = lit_astparser_parseexpression(parser, true);
    lit_astparser_consume(parser, LITTOK_RIGHT_PAREN, "')' after grouping expression");
    return expression;
}

static LitAstExpression* lit_astparser_rulecall(LitAstParser* parser, LitAstExpression* prev, bool canassign)
{
    (void)canassign;
    LitAstExpression* e;
    LitAstVarExpr* ee;
    LitAstCallExpr* expression;
    expression = lit_ast_make_callexpr(parser->state, parser->previous.line, prev);
    while(!lit_astparser_check(parser, LITTOK_RIGHT_PAREN))
    {
        e = lit_astparser_parseexpression(parser, true);
        lit_exprlist_push(parser->state, &expression->args, e);
        if(!lit_astparser_match(parser, LITTOK_COMMA))
        {
            break;
        }
        if(e->type == LITEXPR_VAREXPR)
        {
            ee = (LitAstVarExpr*)e;
            // Vararg ...
            if(ee->length == 3 && memcmp(ee->name, "...", 3) == 0)
            {
                break;
            }
        }
    }
    if(expression->args.count > 255)
    {
        lit_astparser_raiseerror(parser, "function cannot have more than 255 arguments, got %i", (int)expression->args.count);
    }
    lit_astparser_consume(parser, LITTOK_RIGHT_PAREN, "')' after arguments");
    return (LitAstExpression*)expression;
}

static LitAstExpression* lit_astparser_ruleunary(LitAstParser* parser, bool canassign)
{
    (void)canassign;
    size_t line;
    LitAstExpression* expression;
    LitAstTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    expression = lit_astparser_parseprecedence(parser, LITPREC_UNARY, true, true);
    return (LitAstExpression*)lit_ast_make_unaryexpr(parser->state, line, expression, op);
}

static LitAstExpression* lit_astparser_rulebinary(LitAstParser* parser, LitAstExpression* prev, bool canassign)
{
    (void)canassign;
    bool invert;
    size_t line;
    LitAstParseRule* rule;
    LitAstExpression* expression;
    LitAstTokType op;
    invert = parser->previous.type == LITTOK_BANG;
    if(invert)
    {
        lit_astparser_consume(parser, LITTOK_IS, "'is' after '!'");
    }
    op = parser->previous.type;
    line = parser->previous.line;
    rule = lit_astparser_getrule(op);
    expression = lit_astparser_parseprecedence(parser, (LitAstPrecedence)(rule->precedence + 1), true, true);
    expression = (LitAstExpression*)lit_ast_make_binaryexpr(parser->state, line, prev, expression, op);
    if(invert)
    {
        expression = (LitAstExpression*)lit_ast_make_unaryexpr(parser->state, line, expression, LITTOK_BANG);
    }
    return expression;
}

static LitAstExpression* lit_astparser_ruleand(LitAstParser* parser, LitAstExpression* prev, bool canassign)
{
    (void)canassign;
    size_t line;
    LitAstTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    return (LitAstExpression*)lit_ast_make_binaryexpr(parser->state, line, prev, lit_astparser_parseprecedence(parser, LITPREC_AND, true, true), op);
}

static LitAstExpression* lit_astparser_ruleor(LitAstParser* parser, LitAstExpression* prev, bool canassign)
{
    (void)canassign;
    size_t line;
    LitAstTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    return (LitAstExpression*)lit_ast_make_binaryexpr(parser->state, line, prev, lit_astparser_parseprecedence(parser, LITPREC_OR, true, true), op);
}

static LitAstExpression* lit_astparser_rulenull_filter(LitAstParser* parser, LitAstExpression* prev, bool canassign)
{
    (void)canassign;
    size_t line;
    LitAstTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    return (LitAstExpression*)lit_ast_make_binaryexpr(parser->state, line, prev, lit_astparser_parseprecedence(parser, LITPREC_NULL, true, true), op);
}

static LitAstTokType lit_astparser_convertcompoundop(LitAstTokType op)
{
    switch(op)
    {
        case LITTOK_PLUS_EQUAL:
            {
                return LITTOK_PLUS;
            }
            break;
        case LITTOK_MINUS_EQUAL:
            {
                return LITTOK_MINUS;
            }
            break;
        case LITTOK_STAR_EQUAL:
            {
                return LITTOK_STAR;
            }
            break;
        case LITTOK_SLASH_EQUAL:
            {
                return LITTOK_SLASH;
            }
            break;
        case LITTOK_SHARP_EQUAL:
            {
                return LITTOK_SHARP;
            }
            break;
        case LITTOK_PERCENT_EQUAL:
            {
                return LITTOK_PERCENT;
            }
            break;
        case LITTOK_CARET_EQUAL:
            {
                return LITTOK_CARET;
            }
            break;
        case LITTOK_BAR_EQUAL:
            {
                return LITTOK_BAR;
            }
            break;
        case LITTOK_AMPERSAND_EQUAL:
            {
                return LITTOK_AMPERSAND;
            }
            break;
        case LITTOK_PLUS_PLUS:
            {
                return LITTOK_PLUS;
            }
            break;
        case LITTOK_MINUS_MINUS:
            {
                return LITTOK_MINUS;
            }
            break;
        default:
            {
                UNREACHABLE
            }
            break;
    }
    return (LitAstTokType)-1;
}

static LitAstExpression* lit_astparser_rulecompound(LitAstParser* parser, LitAstExpression* prev, bool canassign)
{
    (void)canassign;
    size_t line;
    LitAstBinaryExpr* binary;
    LitAstExpression* expression;
    LitAstParseRule* rule;
    LitAstTokType op;
    op = parser->previous.type;
    line = parser->previous.line;
    rule = lit_astparser_getrule(op);
    if(op == LITTOK_PLUS_PLUS || op == LITTOK_MINUS_MINUS)
    {
        expression = (LitAstExpression*)lit_ast_make_literalexpr(parser->state, line, lit_value_makefixednumber(parser->state, 1));
    }
    else
    {
        expression = lit_astparser_parseprecedence(parser, (LitAstPrecedence)(rule->precedence + 1), true, true);
    }
    binary = lit_ast_make_binaryexpr(parser->state, line, prev, expression, lit_astparser_convertcompoundop(op));
    binary->ignore_left = true;// To make sure we don't free it twice
    return (LitAstExpression*)lit_ast_make_assignexpr(parser->state, line, prev, (LitAstExpression*)binary);
}

static LitAstExpression* lit_astparser_ruleliteral(LitAstParser* parser, bool canassign)
{
    (void)canassign;
    size_t line;
    line = parser->previous.line;
    switch(parser->previous.type)
    {
        case LITTOK_TRUE:
            {
                return (LitAstExpression*)lit_ast_make_literalexpr(parser->state, line, TRUE_VALUE);
            }
            break;
        case LITTOK_FALSE:
            {
                return (LitAstExpression*)lit_ast_make_literalexpr(parser->state, line, FALSE_VALUE);
            }
            break;
        case LITTOK_NULL:
            {
                return (LitAstExpression*)lit_ast_make_literalexpr(parser->state, line, NULL_VALUE);
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

static LitAstExpression* lit_astparser_rulestring(LitAstParser* parser, bool canassign)
{
    (void)canassign;
    LitAstExpression* expression;
    expression = (LitAstExpression*)lit_ast_make_literalexpr(parser->state, parser->previous.line, parser->previous.value);
    if(lit_astparser_match(parser, LITTOK_LEFT_BRACKET))
    {
        return lit_astparser_rulesubscript(parser, expression, canassign);
    }
    return expression;
}

static LitAstExpression* lit_astparser_ruleinterpolation(LitAstParser* parser, bool canassign)
{
    LitAstStrInterExpr* expression;
    (void)canassign;
    expression = lit_ast_make_strinterpolexpr(parser->state, parser->previous.line);
    do
    {
        if(lit_string_getlength(lit_value_asstring(parser->previous.value)) > 0)
        {
            lit_exprlist_push(
            parser->state, &expression->expressions,
            (LitAstExpression*)lit_ast_make_literalexpr(parser->state, parser->previous.line, parser->previous.value));
        }
        lit_exprlist_push(parser->state, &expression->expressions, lit_astparser_parseexpression(parser, true));
    } while(lit_astparser_match(parser, LITTOK_INTERPOLATION));
    lit_astparser_consume(parser, LITTOK_STRING, "end of interpolation");
    if(lit_string_getlength(lit_value_asstring(parser->previous.value)) > 0)
    {
        lit_exprlist_push(
        parser->state, &expression->expressions,
        (LitAstExpression*)lit_ast_make_literalexpr(parser->state, parser->previous.line, parser->previous.value));
    }
    if(lit_astparser_match(parser, LITTOK_LEFT_BRACKET))
    {
        return lit_astparser_rulesubscript(parser, (LitAstExpression*)expression, canassign);
    }
    return (LitAstExpression*)expression;
}

static LitAstExpression* lit_astparser_ruleobject(LitAstParser* parser, bool canassign)
{
    (void)canassign;
    LitAstObjectExpr* object;
    object = lit_ast_make_objectexpr(parser->state, parser->previous.line);
    lit_astparser_ignorenewlines(parser, true);
    while(!lit_astparser_check(parser, LITTOK_RIGHT_BRACE))
    {
        lit_astparser_ignorenewlines(parser, true);
        lit_astparser_consume(parser, LITTOK_IDENTIFIER, "key string after '{'");
        lit_vallist_push(parser->state, &object->keys, lit_value_fromobject(lit_string_copy(parser->state, parser->previous.start, parser->previous.length)));
        lit_astparser_ignorenewlines(parser, true);
        lit_astparser_consume(parser, LITTOK_EQUAL, "'=' after key string");
        lit_astparser_ignorenewlines(parser, true);
        lit_exprlist_push(parser->state, &object->values, lit_astparser_parseexpression(parser, true));
        if(!lit_astparser_match(parser, LITTOK_COMMA))
        {
            break;
        }
    }
    lit_astparser_ignorenewlines(parser, true);
    lit_astparser_consume(parser, LITTOK_RIGHT_BRACE, "'}' after object");
    return (LitAstExpression*)object;
}

static LitAstExpression* lit_astparser_rulevarexprbase(LitAstParser* parser, bool canassign, bool isnew)
{
    (void)canassign;
    bool hadargs;
    LitAstCallExpr* callex;
    LitAstExpression* expression;
    expression = (LitAstExpression*)lit_ast_make_varexpr(parser->state, parser->previous.line, parser->previous.start, parser->previous.length);
    if(isnew)
    {
        hadargs = lit_astparser_check(parser, LITTOK_LEFT_PAREN);
        callex = NULL;
        if(hadargs)
        {
            lit_astparser_advance(parser);
            callex = (LitAstCallExpr*)lit_astparser_rulecall(parser, expression, false);
        }
        if(lit_astparser_match(parser, LITTOK_LEFT_BRACE))
        {
            if(callex == NULL)
            {
                callex = lit_ast_make_callexpr(parser->state, expression->line, expression);
            }
            callex->init = lit_astparser_ruleobject(parser, false);
        }
        else if(!hadargs)
        {
            lit_astparser_raiseatcurrent(parser, "expected %s, got '%.*s'", "argument list for instance creation",
                             parser->previous.length, parser->previous.start);
        }
        return (LitAstExpression*)callex;
    }
    if(lit_astparser_match(parser, LITTOK_LEFT_BRACKET))
    {
        return lit_astparser_rulesubscript(parser, expression, canassign);
    }
    if(canassign && lit_astparser_match(parser, LITTOK_EQUAL))
    {
        return (LitAstExpression*)lit_ast_make_assignexpr(parser->state, parser->previous.line, expression,
                                                            lit_astparser_parseexpression(parser, true));
    }
    return expression;
}

static LitAstExpression* lit_astparser_rulevarexpr(LitAstParser* parser, bool canassign)
{
    return lit_astparser_rulevarexprbase(parser, canassign, false);
}

static LitAstExpression* lit_astparser_rulenewexpr(LitAstParser* parser, bool canassign)
{
    (void)canassign;
    lit_astparser_consume(parser, LITTOK_IDENTIFIER, "class name after 'new'");
    return lit_astparser_rulevarexprbase(parser, false, true);
}

static LitAstExpression* lit_astparser_ruledot(LitAstParser* parser, LitAstExpression* previous, bool canassign)
{
    (void)canassign;
    bool ignored;
    size_t line;
    size_t length;
    const char* name;
    LitAstExpression* expression;
    line = parser->previous.line;
    ignored = parser->previous.type == LITTOK_SMALL_ARROW;
    if(!(lit_astparser_match(parser, LITTOK_CLASS) || lit_astparser_match(parser, LITTOK_SUPER)))
    {// class and super are allowed field names
        lit_astparser_consume(parser, LITTOK_IDENTIFIER, ignored ? "propety name after '->'" : "property name after '.'");
    }
    name = parser->previous.start;
    length = parser->previous.length;
    if(!ignored && canassign && lit_astparser_match(parser, LITTOK_EQUAL))
    {
        return (LitAstExpression*)lit_ast_make_setexpr(parser->state, line, previous, name, length, lit_astparser_parseexpression(parser, true));
    }
    expression = (LitAstExpression*)lit_ast_make_getexpr(parser->state, line, previous, name, length, false, ignored);
    if(!ignored && lit_astparser_match(parser, LITTOK_LEFT_BRACKET))
    {
        return lit_astparser_rulesubscript(parser, expression, canassign);
    }
    return expression;
}

static LitAstExpression* lit_astparser_rulerange(LitAstParser* parser, LitAstExpression* previous, bool canassign)
{
    (void)canassign;
    size_t line;
    line = parser->previous.line;
    return (LitAstExpression*)lit_ast_make_rangeexpr(parser->state, line, previous, lit_astparser_parseexpression(parser, true));
}

static LitAstExpression* lit_astparser_ruleternary(LitAstParser* parser, LitAstExpression* previous, bool canassign)
{
    (void)canassign;
    bool ignored;
    size_t line;
    LitAstExpression* ifbranch;
    LitAstExpression* elsebranch;
    line = parser->previous.line;
    if(lit_astparser_match(parser, LITTOK_DOT) || lit_astparser_match(parser, LITTOK_SMALL_ARROW))
    {
        ignored = parser->previous.type == LITTOK_SMALL_ARROW;
        lit_astparser_consume(parser, LITTOK_IDENTIFIER, ignored ? "property name after '->'" : "property name after '.'");
        return (LitAstExpression*)lit_ast_make_getexpr(parser->state, line, previous, parser->previous.start,
                                                         parser->previous.length, true, ignored);
    }
    ifbranch = lit_astparser_parseexpression(parser, true);
    lit_astparser_consume(parser, LITTOK_COLON, "':' after expression");
    elsebranch = lit_astparser_parseexpression(parser, true);
    return (LitAstExpression*)lit_ast_make_ternaryexpr(parser->state, line, previous, ifbranch, elsebranch);
}

static LitAstExpression* lit_astparser_rulearray(LitAstParser* parser, bool canassign)
{
    (void)canassign;
    LitAstExpression* expr;
    LitAstArrayExpr* array;
    array = lit_ast_make_arrayexpr(parser->state, parser->previous.line);
    lit_astparser_ignorenewlines(parser, true);
    while(!lit_astparser_check(parser, LITTOK_RIGHT_BRACKET))
    {
        expr = NULL;
        lit_astparser_ignorenewlines(parser, true);
        #if 1
            expr = lit_astparser_parseexpression(parser, true);
        #else
            if(lit_astparser_check(parser, LITTOK_COMMA))
            {
                //lit_astparser_rulenull_filter(LitAstParser *parser, LitAstExpression *prev, _Bool canassign)
                expr = lit_astparser_rulenull_filter(parser, NULL, false);
            }
            else
            {
                expr = lit_astparser_parseexpression(parser, true);
            }
        #endif
        lit_exprlist_push(parser->state, &array->values, expr);
        if(!lit_astparser_match(parser, LITTOK_COMMA))
        {
            break;
        }
        lit_astparser_ignorenewlines(parser, true);
    }
    lit_astparser_ignorenewlines(parser, true);
    lit_astparser_consume(parser, LITTOK_RIGHT_BRACKET, "']' after array");
    if(lit_astparser_match(parser, LITTOK_LEFT_BRACKET))
    {
        return lit_astparser_rulesubscript(parser, (LitAstExpression*)array, canassign);
    }
    return (LitAstExpression*)array;
}

static LitAstExpression* lit_astparser_rulesubscript(LitAstParser* parser, LitAstExpression* previous, bool canassign)
{
    size_t line;
    LitAstExpression* index;
    LitAstExpression* expression;
    line = parser->previous.line;
    index = lit_astparser_parseexpression(parser, true);
    lit_astparser_consume(parser, LITTOK_RIGHT_BRACKET, "']' after subscript");
    expression = (LitAstExpression*)lit_ast_make_subscriptexpr(parser->state, line, previous, index);
    if(lit_astparser_match(parser, LITTOK_LEFT_BRACKET))
    {
        return lit_astparser_rulesubscript(parser, expression, canassign);
    }
    else if(canassign && lit_astparser_match(parser, LITTOK_EQUAL))
    {
        return (LitAstExpression*)lit_ast_make_assignexpr(parser->state, parser->previous.line, expression, lit_astparser_parseexpression(parser, true));
    }
    return expression;
}

static LitAstExpression* lit_astparser_rulethis(LitAstParser* parser, bool canassign)
{
    LitAstExpression* expression;
    expression = (LitAstExpression*)lit_ast_make_thisexpr(parser->state, parser->previous.line);
    if(lit_astparser_match(parser, LITTOK_LEFT_BRACKET))
    {
        return lit_astparser_rulesubscript(parser, expression, canassign);
    }
    return expression;
}

static LitAstExpression* lit_astparser_rulesuper(LitAstParser* parser, bool canassign)
{
    (void)canassign;
    bool ignoring;
    size_t line;
    LitAstExpression* expression;
    line = parser->previous.line;
    if(!(lit_astparser_match(parser, LITTOK_DOT) || lit_astparser_match(parser, LITTOK_SMALL_ARROW)))
    {
        expression = (LitAstExpression*)lit_ast_make_superexpr(
        parser->state, line, lit_string_copy(parser->state, "constructor", 11), false);
        lit_astparser_consume(parser, LITTOK_LEFT_PAREN, "'(' after 'super'");
        return lit_astparser_rulecall(parser, expression, false);
    }
    ignoring = parser->previous.type == LITTOK_SMALL_ARROW;
    lit_astparser_consume(parser, LITTOK_IDENTIFIER, ignoring ? "super method name after '->'" : "super method name after '.'");
    expression = (LitAstExpression*)lit_ast_make_superexpr(
    parser->state, line, lit_string_copy(parser->state, parser->previous.start, parser->previous.length), ignoring);
    if(lit_astparser_match(parser, LITTOK_LEFT_PAREN))
    {
        return lit_astparser_rulecall(parser, expression, false);
    }
    return expression;
}

static LitAstExpression *lit_astparser_rulenothing(LitAstParser *parser, bool canassign)
{
    (void)parser;
    (void)canassign;
    return NULL;
}

static LitAstExpression* lit_astparser_rulereference(LitAstParser* parser, bool canassign)
{
    size_t line;
    LitAstRefExpr* expression;
    (void)canassign;
    line = parser->previous.line;
    lit_astparser_ignorenewlines(parser, true);
    expression = lit_ast_make_referenceexpr(parser->state, line, lit_astparser_parseprecedence(parser, LITPREC_CALL, false, true));
    if(lit_astparser_match(parser, LITTOK_EQUAL))
    {
        return (LitAstExpression*)lit_ast_make_assignexpr(parser->state, line, (LitAstExpression*)expression, lit_astparser_parseexpression(parser, true));
    }
    return (LitAstExpression*)expression;
}



static LitAstExpression* lit_astparser_parsestatement(LitAstParser* parser)
{
    LitAstExpression* expression;
    lit_astparser_ignorenewlines(parser, true);
    if(setjmp(prs_jmpbuffer))
    {
        return NULL;
    }
    if(lit_astparser_match(parser, LITTOK_VAR) || lit_astparser_match(parser, LITTOK_CONST))
    {
        return lit_astparser_parsevar_declaration(parser, true);
    }
    else if(lit_astparser_match(parser, LITTOK_IF))
    {
        return lit_astparser_parseif(parser);
    }
    else if(lit_astparser_match(parser, LITTOK_FOR))
    {
        return lit_astparser_parsefor(parser);
    }
    else if(lit_astparser_match(parser, LITTOK_WHILE))
    {
        return lit_astparser_parsewhile(parser);
    }
    else if(lit_astparser_match(parser, LITTOK_CONTINUE))
    {
        return (LitAstExpression*)lit_ast_make_continueexpr(parser->state, parser->previous.line);
    }
    else if(lit_astparser_match(parser, LITTOK_BREAK))
    {
        return (LitAstExpression*)lit_ast_make_breakexpr(parser->state, parser->previous.line);
    }
    else if(lit_astparser_match(parser, LITTOK_FUNCTION) || lit_astparser_match(parser, LITTOK_EXPORT))
    {
        return lit_astparser_rulefunction(parser, false);
    }
    else if(lit_astparser_match(parser, LITTOK_RETURN))
    {
        return lit_astparser_parsereturn(parser);
    }
    else if(lit_astparser_match(parser, LITTOK_LEFT_BRACE))
    {
        return lit_astparser_parseblock(parser);
    }
    expression = lit_astparser_parseexpression(parser, true);
    return expression == NULL ? NULL : (LitAstExpression*)lit_ast_make_exprstmt(parser->state, parser->previous.line, expression);
}

static LitAstExpression* lit_astparser_parseexpression(LitAstParser* parser, bool ignsemi)
{
    lit_astparser_ignorenewlines(parser, ignsemi);
    return lit_astparser_parseprecedence(parser, LITPREC_ASSIGNMENT, true, ignsemi);
}

static LitAstExpression* lit_astparser_parsevar_declaration(LitAstParser* parser, bool ignsemi)
{
    bool constant;
    size_t line;
    size_t length;
    const char* name;
    LitAstExpression* init;
    constant = parser->previous.type == LITTOK_CONST;
    line = parser->previous.line;
    lit_astparser_consume(parser, LITTOK_IDENTIFIER, "variable name");
    name = parser->previous.start;
    length = parser->previous.length;
    init = NULL;
    if(lit_astparser_match(parser, LITTOK_EQUAL))
    {
        init = lit_astparser_parseexpression(parser, ignsemi);
    }
    return (LitAstExpression*)lit_ast_make_assignvarexpr(parser->state, line, name, length, init, constant);
}

static LitAstExpression* lit_astparser_parseif(LitAstParser* parser)
{
    size_t line;
    bool invert;
    bool hadparen;
    LitAstExpression* condition;
    LitAstExpression* ifbranch;
    LitAstExprList* elseifconds;
    LitAstExprList* elseifbranches;
    LitAstExpression* elsebranch;
    LitAstExpression* e;
    line = parser->previous.line;
    invert = lit_astparser_match(parser, LITTOK_BANG);
    hadparen = lit_astparser_match(parser, LITTOK_LEFT_PAREN);
    condition = lit_astparser_parseexpression(parser, true);
    if(hadparen)
    {
        lit_astparser_consume(parser, LITTOK_RIGHT_PAREN, "')'");
    }
    if(invert)
    {
        condition = (LitAstExpression*)lit_ast_make_unaryexpr(parser->state, condition->line, condition, LITTOK_BANG);
    }
    lit_astparser_ignorenewlines(parser, true);
    ifbranch = lit_astparser_parsestatement(parser);
    elseifconds = NULL;
    elseifbranches = NULL;
    elsebranch = NULL;
    lit_astparser_ignorenewlines(parser, true);
    while(lit_astparser_match(parser, LITTOK_ELSE))
    {
        // else if
        lit_astparser_ignorenewlines(parser, true);
        if(lit_astparser_match(parser, LITTOK_IF))
        {
            if(elseifconds == NULL)
            {
                elseifconds = lit_ast_allocexprlist(parser->state);
                elseifbranches = lit_ast_allocate_stmtlist(parser->state);
            }
            invert = lit_astparser_match(parser, LITTOK_BANG);
            hadparen = lit_astparser_match(parser, LITTOK_LEFT_PAREN);
            lit_astparser_ignorenewlines(parser, true);
            e = lit_astparser_parseexpression(parser, true);
            if(hadparen)
            {
                lit_astparser_consume(parser, LITTOK_RIGHT_PAREN, "')'");
            }
            lit_astparser_ignorenewlines(parser, true);
            if(invert)
            {
                e = (LitAstExpression*)lit_ast_make_unaryexpr(parser->state, condition->line, e, LITTOK_BANG);
            }
            lit_exprlist_push(parser->state, elseifconds, e);
            lit_exprlist_push(parser->state, elseifbranches, lit_astparser_parsestatement(parser));
            continue;
        }
        // else
        if(elsebranch != NULL)
        {
            lit_astparser_raiseerror(parser, "if-statement can have only one else-branch");
        }
        lit_astparser_ignorenewlines(parser, true);
        elsebranch = lit_astparser_parsestatement(parser);
    }
    return (LitAstExpression*)lit_ast_make_ifexpr(parser->state, line, condition, ifbranch, elsebranch, elseifconds, elseifbranches);
}

static LitAstExpression* lit_astparser_parsefor(LitAstParser* parser)
{
    bool c_style;
    bool hadparen;
    size_t line;
    LitAstExpression* condition;
    LitAstExpression* increment;
    LitAstExpression* var;
    LitAstExpression* init;
    line= parser->previous.line;
    hadparen = lit_astparser_match(parser, LITTOK_LEFT_PAREN);
    var = NULL;
    init = NULL;
    if(!lit_astparser_check(parser, LITTOK_SEMICOLON))
    {
        if(lit_astparser_match(parser, LITTOK_VAR))
        {
            var = lit_astparser_parsevar_declaration(parser, false);
        }
        else
        {
            init = lit_astparser_parseexpression(parser, false);
        }
    }
    c_style = !lit_astparser_match(parser, LITTOK_IN);
    condition= NULL;
    increment = NULL;
    if(c_style)
    {
        lit_astparser_consume(parser, LITTOK_SEMICOLON, "';'");
        condition = lit_astparser_check(parser, LITTOK_SEMICOLON) ? NULL : lit_astparser_parseexpression(parser, false);
        lit_astparser_consume(parser, LITTOK_SEMICOLON, "';'");
        increment = lit_astparser_check(parser, LITTOK_RIGHT_PAREN) ? NULL : lit_astparser_parseexpression(parser, false);
    }
    else
    {
        condition = lit_astparser_parseexpression(parser, true);
        if(var == NULL)
        {
            lit_astparser_raiseerror(parser, "for-loops using in-iteration must declare a new variable");
        }
    }
    if(hadparen)
    {
        lit_astparser_consume(parser, LITTOK_RIGHT_PAREN, "')'");
    }
    lit_astparser_ignorenewlines(parser, true);
    return (LitAstExpression*)lit_ast_make_forexpr(parser->state, line, init, var, condition, increment,
                                                   lit_astparser_parsestatement(parser), c_style);
}

static LitAstExpression* lit_astparser_parsewhile(LitAstParser* parser)
{
    bool hadparen;
    size_t line;
    LitAstExpression* body;
    line = parser->previous.line;
    hadparen = lit_astparser_match(parser, LITTOK_LEFT_PAREN);
    LitAstExpression* condition = lit_astparser_parseexpression(parser, true);
    if(hadparen)
    {
        lit_astparser_consume(parser, LITTOK_RIGHT_PAREN, "')'");
    }
    lit_astparser_ignorenewlines(parser, true);
    body = lit_astparser_parsestatement(parser);
    return (LitAstExpression*)lit_ast_make_whileexpr(parser->state, line, condition, body);
}

static LitAstExpression* lit_astparser_rulefunction(LitAstParser* parser, bool canassign)
{
    bool isexport;
    bool islambda;
    size_t line;
    size_t fnamelen;
    const char* fnamestr;
    LitAstCompiler compiler;
    LitAstFunctionExpr* function;
    LitAstFunctionExpr* lambda;
    LitAstSetExpr* to;
    islambda = canassign;
    isexport = parser->previous.type == LITTOK_EXPORT;
    fnamestr = "<anonymous>";
    fnamelen = strlen(fnamestr);
    if(isexport)
    {
        lit_astparser_consume(parser, LITTOK_FUNCTION, "'function' after 'export'");
    }
    line = parser->previous.line;
    if(lit_astparser_check(parser, LITTOK_IDENTIFIER))
    {
        lit_astparser_consume(parser, LITTOK_IDENTIFIER, "function name");
        fnamestr = parser->previous.start;
        fnamelen = parser->previous.length;
    }
    if(lit_astparser_match(parser, LITTOK_DOT) || islambda)
    //if(lit_astparser_match(parser, LITTOK_DOT))
    {
        to = NULL;
        if(lit_astparser_check(parser, LITTOK_IDENTIFIER))
        {
            lit_astparser_consume(parser, LITTOK_IDENTIFIER, "function name");
        }
        lambda = lit_ast_make_lambdaexpr(parser->state, line);
        //if(islambda)
        /*
        {
            to = lit_ast_make_setexpr(
                parser->state,
                line,
                (LitAstExpression*)lit_ast_make_varexpr(parser->state, line, fnamestr, fnamelen),
                parser->previous.start,
                parser->previous.length,
                (LitAstExpression*)lambda
            );
        }
        */
        lit_astparser_consume(parser, LITTOK_LEFT_PAREN, "'(' after function name");
        lit_astparser_initcompiler(parser, &compiler);
        lit_astparser_beginscope(parser);
        lit_astparser_parseparameters(parser, &lambda->parameters);
        if(lambda->parameters.count > 255)
        {
            lit_astparser_raiseerror(parser, "function cannot have more than 255 arguments, got %i", (int)lambda->parameters.count);
        }
        lit_astparser_consume(parser, LITTOK_RIGHT_PAREN, "')' after function arguments");
        lit_astparser_ignorenewlines(parser, true);
        lambda->body = lit_astparser_parsestatement(parser);
        lit_astparser_endscope(parser);
        lit_astparser_endcompiler(parser, &compiler);
        if(islambda)
        {
            return (LitAstExpression*)lambda;
        }
        return (LitAstExpression*)lit_ast_make_exprstmt(parser->state, line, (LitAstExpression*)to);
    }
    function = lit_ast_make_funcexpr(parser->state, line, fnamestr, fnamelen);
    function->exported = isexport;
    lit_astparser_consume(parser, LITTOK_LEFT_PAREN, "'(' after function name");
    lit_astparser_initcompiler(parser, &compiler);
    lit_astparser_beginscope(parser);
    lit_astparser_parseparameters(parser, &function->parameters);
    if(function->parameters.count > 255)
    {
        lit_astparser_raiseerror(parser, "function cannot have more than 255 arguments, got %i", (int)function->parameters.count);
    }
    lit_astparser_consume(parser, LITTOK_RIGHT_PAREN, "')' after function arguments");
    function->body = lit_astparser_parsestatement(parser);
    lit_astparser_endscope(parser);
    lit_astparser_endcompiler(parser, &compiler);
    return (LitAstExpression*)function;
}

static LitAstExpression* lit_astparser_parsereturn(LitAstParser* parser)
{
    size_t line;
    LitAstExpression* expression;
    line = parser->previous.line;
    expression = NULL;
    if(!lit_astparser_check(parser, LITTOK_NEW_LINE) && !lit_astparser_check(parser, LITTOK_RIGHT_BRACE))
    {
        expression = lit_astparser_parseexpression(parser, true);
    }
    return (LitAstExpression*)lit_ast_make_returnexpr(parser->state, line, expression);
}

static LitAstExpression* lit_astparser_parsefield(LitAstParser* parser, LitString* name, bool is_static)
{
    size_t line;
    LitAstExpression* getter;
    LitAstExpression* setter;
    line = parser->previous.line;
    getter = NULL;
    setter = NULL;
    if(lit_astparser_match(parser, LITTOK_ARROW))
    {
        getter = lit_astparser_parsestatement(parser);
    }
    else
    {
        lit_astparser_match(parser, LITTOK_LEFT_BRACE);// Will be LITTOK_LEFT_BRACE, otherwise this method won't be called
        lit_astparser_ignorenewlines(parser, true);
        if(lit_astparser_match(parser, LITTOK_GET))
        {
            lit_astparser_match(parser, LITTOK_ARROW);// Ignore it if it's present
            getter = lit_astparser_parsestatement(parser);
        }
        lit_astparser_ignorenewlines(parser, true);
        if(lit_astparser_match(parser, LITTOK_SET))
        {
            lit_astparser_match(parser, LITTOK_ARROW);// Ignore it if it's present
            setter = lit_astparser_parsestatement(parser);
        }
        if(getter == NULL && setter == NULL)
        {
            lit_astparser_raiseerror(parser, "expected declaration of either getter or setter, got none");
        }
        lit_astparser_ignorenewlines(parser, true);
        lit_astparser_consume(parser, LITTOK_RIGHT_BRACE, "'}' after field declaration");
    }
    return (LitAstExpression*)lit_ast_make_fieldexpr(parser->state, line, name, getter, setter, is_static);
}

static LitAstExpression* lit_astparser_parsemethod(LitAstParser* parser, bool is_static)
{
    size_t i;
    LitAstCompiler compiler;
    LitAstMethodExpr* method;
    LitString* name;
    if(lit_astparser_match(parser, LITTOK_STATIC))
    {
        is_static = true;
    }
    name = NULL;
    if(lit_astparser_match(parser, LITTOK_OPERATOR))
    {
        if(is_static)
        {
            lit_astparser_raiseerror(parser, "operator methods cannot be static or defined in static classes");
        }
        i = 0;
        while(operators[i] != LITTOK_EOF)
        {
            if(lit_astparser_match(parser, operators[i]))
            {
                break;
            }
            i++;
        }
        if(parser->previous.type == LITTOK_LEFT_BRACKET)
        {
            lit_astparser_consume(parser, LITTOK_RIGHT_BRACKET, "']' after '[' in op method declaration");
            name = lit_string_copy(parser->state, "[]", 2);
        }
        else
        {
            name = lit_string_copy(parser->state, parser->previous.start, parser->previous.length);
        }
    }
    else
    {
        lit_astparser_consume(parser, LITTOK_IDENTIFIER, "method name");
        name = lit_string_copy(parser->state, parser->previous.start, parser->previous.length);
        if(lit_astparser_check(parser, LITTOK_LEFT_BRACE) || lit_astparser_check(parser, LITTOK_ARROW))
        {
            return lit_astparser_parsefield(parser, name, is_static);
        }
    }
    method = lit_ast_make_methodexpr(parser->state, parser->previous.line, name, is_static);
    lit_astparser_initcompiler(parser, &compiler);
    lit_astparser_beginscope(parser);
    lit_astparser_consume(parser, LITTOK_LEFT_PAREN, "'(' after method name");
    lit_astparser_parseparameters(parser, &method->parameters);
    if(method->parameters.count > 255)
    {
        lit_astparser_raiseerror(parser, "function cannot have more than 255 arguments, got %i", (int)method->parameters.count);
    }
    lit_astparser_consume(parser, LITTOK_RIGHT_PAREN, "')' after method arguments");
    method->body = lit_astparser_parsestatement(parser);
    lit_astparser_endscope(parser);
    lit_astparser_endcompiler(parser, &compiler);
    return (LitAstExpression*)method;
}

static LitAstExpression* lit_astparser_parseclass(LitAstParser* parser)
{
    bool finishedparsingfields;
    bool fieldisstatic;
    size_t line;
    bool is_static;
    LitString* name;
    LitString* super;
    LitAstClassExpr* klass;
    LitAstExpression* var;
    LitAstExpression* method;
    if(setjmp(prs_jmpbuffer))
    {
        return NULL;
    }
    line = parser->previous.line;
    is_static = parser->previous.type == LITTOK_STATIC;
    if(is_static)
    {
        lit_astparser_consume(parser, LITTOK_CLASS, "'class' after 'static'");
    }
    lit_astparser_consume(parser, LITTOK_IDENTIFIER, "class name after 'class'");
    name = lit_string_copy(parser->state, parser->previous.start, parser->previous.length);
    super = NULL;
    if(lit_astparser_match(parser, LITTOK_COLON))
    {
        lit_astparser_consume(parser, LITTOK_IDENTIFIER, "super class name after ':'");
        super = lit_string_copy(parser->state, parser->previous.start, parser->previous.length);
        if(super == name)
        {
            lit_astparser_raiseerror(parser, "class cannot inherit itself");
        }
    }
    klass = lit_ast_make_classexpr(parser->state, line, name, super);
    lit_astparser_ignorenewlines(parser, true);
    lit_astparser_consume(parser, LITTOK_LEFT_BRACE, "'{' before class body");
    lit_astparser_ignorenewlines(parser, true);
    finishedparsingfields = false;
    while(!lit_astparser_check(parser, LITTOK_RIGHT_BRACE))
    {
        fieldisstatic = false;
        if(lit_astparser_match(parser, LITTOK_STATIC))
        {
            fieldisstatic = true;
            if(lit_astparser_match(parser, LITTOK_VAR))
            {
                if(finishedparsingfields)
                {
                    lit_astparser_raiseerror(parser, "all static fields must be defined before the methods");
                }
                var = lit_astparser_parsevar_declaration(parser, true);
                if(var != NULL)
                {
                    lit_exprlist_push(parser->state, &klass->fields, var);
                }
                lit_astparser_ignorenewlines(parser, true);
                continue;
            }
            else
            {
                finishedparsingfields = true;
            }
        }
        method = lit_astparser_parsemethod(parser, is_static || fieldisstatic);
        if(method != NULL)
        {
            lit_exprlist_push(parser->state, &klass->fields, method);
        }
        lit_astparser_ignorenewlines(parser, true);
    }
    lit_astparser_consume(parser, LITTOK_RIGHT_BRACE, "'}' after class body");
    return (LitAstExpression*)klass;
}

static void lit_astparser_sync(LitAstParser* parser)
{
    parser->panic_mode = false;
    while(parser->current.type != LITTOK_EOF)
    {
        if(parser->previous.type == LITTOK_NEW_LINE)
        {
            longjmp(prs_jmpbuffer, 1);
            return;
        }
        switch(parser->current.type)
        {
            case LITTOK_CLASS:
            case LITTOK_FUNCTION:
            case LITTOK_EXPORT:
            case LITTOK_VAR:
            case LITTOK_CONST:
            case LITTOK_FOR:
            case LITTOK_STATIC:
            case LITTOK_IF:
            case LITTOK_WHILE:
            case LITTOK_RETURN:
            {
                longjmp(prs_jmpbuffer, 1);
                return;
            }
            default:
            {
                lit_astparser_advance(parser);
            }
        }
    }
}

static LitAstExpression* lit_astparser_parsedeclaration(LitAstParser* parser)
{
    LitAstExpression* statement;
    statement = NULL;
    if(lit_astparser_match(parser, LITTOK_CLASS) || lit_astparser_match(parser, LITTOK_STATIC))
    {
        statement = lit_astparser_parseclass(parser);
    }
    else
    {
        statement = lit_astparser_parsestatement(parser);
    }
    return statement;
}

bool lit_astparser_parsesource(LitAstParser* parser, const char* file_name, const char* source, LitAstExprList* statements)
{
    LitAstCompiler compiler;
    LitAstExpression* statement;
    parser->had_error = false;
    parser->panic_mode = false;
    lit_astlex_init(parser->state, parser->state->scanner, file_name, source);
    lit_astparser_initcompiler(parser, &compiler);
    lit_astparser_advance(parser);
    lit_astparser_ignorenewlines(parser, true);
    if(!prs_is_at_end(parser))
    {
        do
        {
            statement = lit_astparser_parsedeclaration(parser);
            if(statement != NULL)
            {
                lit_exprlist_push(parser->state, statements, statement);
            }
            if(!lit_astparser_matchnewline(parser))
            {
                if(lit_astparser_match(parser, LITTOK_EOF))
                {
                    break;
                }
            }
        } while(!prs_is_at_end(parser));
    }
    return parser->had_error || parser->state->scanner->had_error;
}
