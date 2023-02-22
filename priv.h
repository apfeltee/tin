
#include "tin.h"

enum TinOpCode
{
#define OPCODE(name, effect) OP_##name,
#include "opcodes.inc"
#undef OPCODE
};

enum TinAstExprType
{
    TINEXPR_LITERAL,
    TINEXPR_BINARY,
    TINEXPR_UNARY,
    TINEXPR_VAREXPR,
    TINEXPR_ASSIGN,
    TINEXPR_CALL,
    TINEXPR_SET,
    TINEXPR_GET,
    TINEXPR_LAMBDA,
    TINEXPR_ARRAY,
    TINEXPR_OBJECT,
    TINEXPR_SUBSCRIPT,
    TINEXPR_THIS,
    TINEXPR_SUPER,
    TINEXPR_RANGE,
    TINEXPR_TERNARY,
    TINEXPR_INTERPOLATION,
    TINEXPR_REFERENCE,

    TINEXPR_EXPRESSION,
    TINEXPR_BLOCK,
    TINEXPR_IFSTMT,
    TINEXPR_WHILE,
    TINEXPR_FOR,
    TINEXPR_VARSTMT,
    TINEXPR_CONTINUE,
    TINEXPR_BREAK,
    TINEXPR_FUNCTION,
    TINEXPR_RETURN,
    TINEXPR_METHOD,
    TINEXPR_CLASS,
    TINEXPR_FIELD
};

enum TinAstPrecedence
{
    TINPREC_NONE,
    TINPREC_ASSIGNMENT,// =
    TINPREC_OR,// ||
    TINPREC_AND,// &&
    TINPREC_BOR,// | ^
    TINPREC_BAND,// &
    TINPREC_SHIFT,// << >>
    TINPREC_EQUALITY,// == !=
    TINPREC_COMPARISON,// < > <= >=
    TINPREC_COMPOUND,// += -= *= /= ++ --
    TINPREC_TERM,// + -
    TINPREC_FACTOR,// * /
    TINPREC_IS,// is
    TINPREC_RANGE,// ..
    TINPREC_UNARY,// ! - ~
    TINPREC_NULL,// ??
    TINPREC_CALL,// . ()
    TINPREC_PRIMARY
};

enum TinAstTokType
{
    TINTOK_NEW_LINE,

    // Single-character tokens.
    TINTOK_LEFT_PAREN,
    TINTOK_RIGHT_PAREN,
    TINTOK_LEFT_BRACE,
    TINTOK_RIGHT_BRACE,
    TINTOK_LEFT_BRACKET,
    TINTOK_RIGHT_BRACKET,
    TINTOK_COMMA,
    TINTOK_SEMICOLON,
    TINTOK_COLON,

    // One or two character tokens.
    TINTOK_BAR_EQUAL,
    TINTOK_BAR,
    TINTOK_BAR_BAR,
    TINTOK_AMPERSAND_EQUAL,
    TINTOK_AMPERSAND,
    TINTOK_AMPERSAND_AMPERSAND,
    TINTOK_BANG,
    TINTOK_BANG_EQUAL,
    TINTOK_EQUAL,
    TINTOK_EQUAL_EQUAL,
    TINTOK_GREATER,
    TINTOK_GREATER_EQUAL,
    TINTOK_GREATER_GREATER,
    TINTOK_LESS,
    TINTOK_LESS_EQUAL,
    TINTOK_LESS_LESS,
    TINTOK_PLUS,
    TINTOK_PLUS_EQUAL,
    TINTOK_PLUS_PLUS,
    TINTOK_MINUS,
    TINTOK_MINUS_EQUAL,
    TINTOK_MINUS_MINUS,
    TINTOK_STAR,
    TINTOK_STAR_EQUAL,
    TINTOK_STAR_STAR,
    TINTOK_SLASH,
    TINTOK_SLASH_EQUAL,
    TINTOK_QUESTION,
    TINTOK_QUESTION_QUESTION,
    TINTOK_PERCENT,
    TINTOK_PERCENT_EQUAL,
    TINTOK_ARROW,
    TINTOK_SMALL_ARROW,
    TINTOK_TILDE,
    TINTOK_CARET,
    TINTOK_CARET_EQUAL,
    TINTOK_DOT,
    TINTOK_DOT_DOT,
    TINTOK_DOT_DOT_DOT,
    TINTOK_SHARP,
    TINTOK_SHARP_EQUAL,

    // Tinerals.
    TINTOK_IDENTIFIER,
    TINTOK_STRING,
    TINTOK_INTERPOLATION,
    TINTOK_NUMBER,

    // Keywords.
    TINTOK_CLASS,
    TINTOK_ELSE,
    TINTOK_FALSE,
    TINTOK_FOR,
    TINTOK_FUNCTION,
    TINTOK_IF,
    TINTOK_NULL,
    TINTOK_RETURN,
    TINTOK_SUPER,
    TINTOK_THIS,
    TINTOK_TRUE,
    TINTOK_VAR,
    TINTOK_WHILE,
    TINTOK_CONTINUE,
    TINTOK_BREAK,
    TINTOK_NEW,
    TINTOK_EXPORT,
    TINTOK_IS,
    TINTOK_STATIC,
    TINTOK_OPERATOR,
    TINTOK_GET,
    TINTOK_SET,
    TINTOK_IN,
    TINTOK_CONST,
    TINTOK_REF,

    TINTOK_ERROR,
    TINTOK_EOF
};

enum TinAstFuncType
{
    TINFUNC_REGULAR,
    TINFUNC_SCRIPT,
    TINFUNC_METHOD,
    TINFUNC_STATIC_METHOD,
    TINFUNC_CONSTRUCTOR
};

struct TinAstExpression
{
    TinAstExprType type;
    size_t line;
};

struct TinAstPrivate
{
    bool initialized;
    bool constant;
};

struct TinAstPrivList
{
    size_t capacity;
    size_t count;
    TinAstPrivate* values;
};

struct TinAstEmitter
{
    TinState* state;
    TinChunk* chunk;
    TinAstCompiler* compiler;
    size_t lastline;
    size_t loopstart;
    TinAstPrivList privates;
    TinUintList breaks;
    TinUintList continues;
    TinModule* module;
    TinString* classname;
    bool classisinheriting;
    bool prevwasexprstmt;
    int emitref;
};

struct TinAstParseRule
{
    TinAstParsePrefixFn prefix;
    TinAstParseInfixFn infix;
    TinAstPrecedence precedence;
};

/*
 * Expressions
 */
struct TinAstExprList
{
    size_t capacity;
    size_t count;
    TinAstExpression** values;
};

struct TinAstLiteralExpr
{
    TinAstExpression exobj;
    TinValue value;
};

struct TinAstBinaryExpr
{
    TinAstExpression exobj;
    TinAstExpression* left;
    TinAstExpression* right;
    TinAstTokType op;
    bool ignore_left;
};

struct TinAstUnaryExpr
{
    TinAstExpression exobj;
    TinAstExpression* right;
    TinAstTokType op;
};

struct TinAstVarExpr
{
    TinAstExpression exobj;
    const char* name;
    size_t length;
};

struct TinAstAssignExpr
{
    TinAstExpression exobj;
    TinAstExpression* to;
    TinAstExpression* value;
};

struct TinAstCallExpr
{
    TinAstExpression exobj;
    TinAstExpression* callee;
    TinAstExprList args;
    TinAstExpression* init;
};

struct TinAstGetExpr
{
    TinAstExpression exobj;
    TinAstExpression* where;
    const char* name;
    size_t length;
    int jump;
    bool ignemit;
    bool ignresult;
};

struct TinAstSetExpr
{
    TinAstExpression exobj;
    TinAstExpression* where;
    const char* name;
    size_t length;
    TinAstExpression* value;
};

struct TinAstParameter
{
    const char* name;
    size_t length;
    TinAstExpression* defaultexpr;
};

struct TinAstParamList
{
    size_t capacity;
    size_t count;
    TinAstParameter* values;
};

struct TinAstArrayExpr
{
    TinAstExpression exobj;
    TinAstExprList values;
};

struct TinAstObjectExpr
{
    TinAstExpression exobj;
    TinValList keys;
    TinAstExprList values;
};

struct TinAstIndexExpr
{
    TinAstExpression exobj;
    TinAstExpression* array;
    TinAstExpression* index;
};

struct TinAstThisExpr
{
    TinAstExpression exobj;
};

struct TinAstSuperExpr
{
    TinAstExpression exobj;
    TinString* method;
    bool ignemit;
    bool ignresult;
};

struct TinAstRangeExpr
{
    TinAstExpression exobj;
    TinAstExpression* from;
    TinAstExpression* to;
};

struct TinAstTernaryExpr
{
    TinAstExpression exobj;
    TinAstExpression* condition;
    TinAstExpression* ifbranch;
    TinAstExpression* elsebranch;
};

struct TinAstStrInterExpr
{
    TinAstExpression exobj;
    TinAstExprList expressions;
};

struct TinAstRefExpr
{
    TinAstExpression exobj;
    TinAstExpression* to;
};

/*
 * Statements
 */

struct TinAstExprExpr
{
    TinAstExpression exobj;
    TinAstExpression* expression;
    bool pop;
};

struct TinAstBlockExpr
{
    TinAstExpression exobj;
    TinAstExprList statements;
};

struct TinAstAssignVarExpr
{
    TinAstExpression exobj;
    const char* name;
    size_t length;
    bool constant;
    TinAstExpression* init;
};

struct TinAstIfExpr
{
    TinAstExpression exobj;
    TinAstExpression* condition;
    TinAstExpression* ifbranch;
    TinAstExpression* elsebranch;
    TinAstExprList* elseifconds;
    TinAstExprList* elseifbranches;
};

struct TinAstWhileExpr
{
    TinAstExpression exobj;
    TinAstExpression* condition;
    TinAstExpression* body;
};

struct TinAstForExpr
{
    TinAstExpression exobj;
    TinAstExpression* init;
    TinAstExpression* var;
    TinAstExpression* condition;
    TinAstExpression* increment;
    TinAstExpression* body;
    bool cstyle;
};

struct TinAstContinueExpr
{
    TinAstExpression exobj;
};

struct TinAstBreakExpr
{
    TinAstExpression exobj;
};

struct TinAstFunctionExpr
{
    TinAstExpression exobj;
    const char* name;
    size_t length;
    TinAstParamList parameters;
    TinAstExpression* body;
    bool exported;
};

struct TinAstReturnExpr
{
    TinAstExpression exobj;
    TinAstExpression* expression;
};

struct TinAstMethodExpr
{
    TinAstExpression exobj;
    TinString* name;
    TinAstParamList parameters;
    TinAstExpression* body;
    bool isstatic;
};

struct TinAstClassExpr
{
    TinAstExpression exobj;
    TinString* name;
    TinString* parent;
    TinAstExprList fields;
};

struct TinAstFieldExpr
{
    TinAstExpression exobj;
    TinString* name;
    TinAstExpression* getter;
    TinAstExpression* setter;
    bool isstatic;
};

struct TinVariable
{
    const char* name;
    size_t length;
    int depth;
    bool constant;
    bool used;
    TinValue constvalue;
    TinAstExpression** declaration;
};


struct TinExecState
{
    TinValue* slots;
    TinValue* privates;
    TinUpvalue** upvalues;
    uint8_t* ip;
    TinCallFrame* frame;
    TinChunk* currentchunk;
};

struct TinAstToken
{
    const char* start;
    TinAstTokType type;
    size_t length;
    size_t line;
    TinValue value;
};

struct TinAstLocal
{
    const char* name;
    size_t length;
    int depth;
    bool captured;
    bool constant;
};

struct TinAstLocList
{
    size_t capacity;
    size_t count;
    TinAstLocal* values;
};

struct TinAstCompUpvalue
{
    uint8_t index;
    bool isLocal;
};

struct TinAstCompiler
{
    TinAstLocList locals;
    int scope_depth;
    TinFunction* function;
    TinAstFuncType type;
    TinAstCompUpvalue upvalues[UINT8_COUNT];
    TinAstCompiler* enclosing;
    bool skipreturn;
    size_t loopdepth;
    int slots;
    int maxslots;
};

struct TinAstParser
{
    TinState* state;
    bool haderror;
    bool panic_mode;
    TinAstToken previous;
    TinAstToken current;
    TinAstCompiler* compiler;
    uint8_t exprrootcnt;
    uint8_t stmtrootcnt;
};

struct TinEmulatedFile
{
    const char* source;
    size_t length;
    size_t position;
};

struct TinAstScanner
{
    size_t line;
    const char* start;
    const char* current;
    const char* filename;
    TinState* state;
    size_t braces[TIN_MAX_INTERPOLATION_NESTING];
    size_t numbraces;
    bool haderror;
};

struct TinAstOptimizer
{
    TinState* state;
    TinVarList variables;
    int depth;
    bool mark_used;
};

static inline void tin_vm_push(TinVM* vm, TinValue value)
{
    *(vm->fiber->stack_top) = value;
    vm->fiber->stack_top++;
}

static inline TinValue tin_vm_pop(TinVM* vm)
{
    TinValue rt;
    rt = *(vm->fiber->stack_top);
    vm->fiber->stack_top--;
    return rt;
}

