
#pragma once
#include "tin.h"

#if defined(__GNUC__)
    #define TIN_VM_INLINE static inline
#else
    #define TIN_VM_INLINE static
#endif


enum TinOpCode
{
#define OPCODE(name, effect) name,
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
    TINTOK_NEWLINE,

    // Single-character tokens.
    TINTOK_PARENOPEN,
    TINTOK_PARENCLOSE,
    TINTOK_BRACEOPEN,
    TINTOK_BRACECLOSE,
    TINTOK_BRACKETOPEN,
    TINTOK_BRACKETCLOSE,
    TINTOK_COMMA,
    TINTOK_SEMICOLON,
    TINTOK_COLON,

    // One or two character tokens.
    TINTOK_ASSIGNEQUAL,
    TINTOK_BAR,
    TINTOK_DOUBLEBAR,
    TINTOK_AMPERSANDEQUAL,
    TINTOK_AMPERSAND,
    TINTOK_DOUBLEAMPERSAND,
    TINTOK_BANG,
    TINTOK_BANGEQUAL,
    TINTOK_ASSIGN,
    TINTOK_EQUAL,
    TINTOK_GREATERTHAN,
    TINTOK_GREATEREQUAL,
    TINTOK_SHIFTRIGHT,
    TINTOK_LESSTHAN,
    TINTOK_LESSEQUAL,
    TINTOK_SHIFTLEFT,
    TINTOK_PLUS,
    TINTOK_PLUSEQUAL,
    TINTOK_DOUBLEPLUS,
    TINTOK_MINUS,
    TINTOK_MINUSEQUAL,
    TINTOK_DOUBLEMINUS,
    TINTOK_STAR,
    TINTOK_STAREQUAL,
    TINTOK_DOUBLESTAR,
    TINTOK_SLASH,
    TINTOK_SLASHEQUAL,
    TINTOK_QUESTION,
    TINTOK_DOUBLEQUESTION,
    TINTOK_PERCENT,
    TINTOK_PERCENTEQUAL,
    TINTOK_ARROW,
    TINTOK_SMALLARROW,
    TINTOK_TILDE,
    TINTOK_CARET,
    TINTOK_CARETEQUAL,
    TINTOK_DOT,
    TINTOK_DOUBLEDOT,
    TINTOK_TRIPLEDOT,
    TINTOK_SHARP,
    TINTOK_SHARPEQUAL,

    // Literals.
    TINTOK_IDENT,
    TINTOK_STRING,
    TINTOK_STRINTERPOL,
    TINTOK_NUMBER,

    // Keywords.
    TINTOK_KWCLASS,
    TINTOK_KWELSE,
    TINTOK_KWFALSE,
    TINTOK_KWFOR,
    TINTOK_KWFUNCTION,
    TINTOK_KWIF,
    TINTOK_KWNULL,
    TINTOK_KWRETURN,
    TINTOK_KWSUPER,
    TINTOK_KWTHIS,
    TINTOK_KWTRUE,
    TINTOK_KWVAR,
    TINTOK_KWWHILE,
    TINTOK_KWCONTINUE,
    TINTOK_KWBREAK,
    TINTOK_KWNEW,
    TINTOK_KWEXPORT,
    TINTOK_KWIS,
    TINTOK_KWSTATIC,
    TINTOK_KWOPERATOR,
    TINTOK_KWGET,
    TINTOK_KWSET,
    TINTOK_KWIN,
    TINTOK_KWCONST,
    TINTOK_KWREF,

    TINTOK_ERROR,
    TINTOK_EOF
};

enum TinAstFuncType
{
    TINFUNC_REGULAR,
    TINFUNC_SCRIPT,
    TINFUNC_METHOD,
    TINFUNC_STATICMETHOD,
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
    TinAstExprList keys;
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
    TinState* state;
    TinVM* vm;
    TinFiber* fiber;
    TinValue* slots;
    TinValue* privates;
    TinUpvalue** upvalues;
    uint8_t* ip;
    TinCallFrame* frame;
    TinChunk* currentchunk;
    bool wasallowed;
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

