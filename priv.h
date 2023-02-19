
#include "lit.h"

enum LitOpCode
{
#define OPCODE(name, effect) OP_##name,
#include "opcodes.inc"
#undef OPCODE
};

enum LitAstExprType
{
    LITEXPR_LITERAL,
    LITEXPR_BINARY,
    LITEXPR_UNARY,
    LITEXPR_VAREXPR,
    LITEXPR_ASSIGN,
    LITEXPR_CALL,
    LITEXPR_SET,
    LITEXPR_GET,
    LITEXPR_LAMBDA,
    LITEXPR_ARRAY,
    LITEXPR_OBJECT,
    LITEXPR_SUBSCRIPT,
    LITEXPR_THIS,
    LITEXPR_SUPER,
    LITEXPR_RANGE,
    LITEXPR_TERNARY,
    LITEXPR_INTERPOLATION,
    LITEXPR_REFERENCE,

    LITEXPR_EXPRESSION,
    LITEXPR_BLOCK,
    LITEXPR_IFSTMT,
    LITEXPR_WHILE,
    LITEXPR_FOR,
    LITEXPR_VARSTMT,
    LITEXPR_CONTINUE,
    LITEXPR_BREAK,
    LITEXPR_FUNCTION,
    LITEXPR_RETURN,
    LITEXPR_METHOD,
    LITEXPR_CLASS,
    LITEXPR_FIELD
};

enum LitAstPrecedence
{
    LITPREC_NONE,
    LITPREC_ASSIGNMENT,// =
    LITPREC_OR,// ||
    LITPREC_AND,// &&
    LITPREC_BOR,// | ^
    LITPREC_BAND,// &
    LITPREC_SHIFT,// << >>
    LITPREC_EQUALITY,// == !=
    LITPREC_COMPARISON,// < > <= >=
    LITPREC_COMPOUND,// += -= *= /= ++ --
    LITPREC_TERM,// + -
    LITPREC_FACTOR,// * /
    LITPREC_IS,// is
    LITPREC_RANGE,// ..
    LITPREC_UNARY,// ! - ~
    LITPREC_NULL,// ??
    LITPREC_CALL,// . ()
    LITPREC_PRIMARY
};

enum LitAstTokType
{
    LITTOK_NEW_LINE,

    // Single-character tokens.
    LITTOK_LEFT_PAREN,
    LITTOK_RIGHT_PAREN,
    LITTOK_LEFT_BRACE,
    LITTOK_RIGHT_BRACE,
    LITTOK_LEFT_BRACKET,
    LITTOK_RIGHT_BRACKET,
    LITTOK_COMMA,
    LITTOK_SEMICOLON,
    LITTOK_COLON,

    // One or two character tokens.
    LITTOK_BAR_EQUAL,
    LITTOK_BAR,
    LITTOK_BAR_BAR,
    LITTOK_AMPERSAND_EQUAL,
    LITTOK_AMPERSAND,
    LITTOK_AMPERSAND_AMPERSAND,
    LITTOK_BANG,
    LITTOK_BANG_EQUAL,
    LITTOK_EQUAL,
    LITTOK_EQUAL_EQUAL,
    LITTOK_GREATER,
    LITTOK_GREATER_EQUAL,
    LITTOK_GREATER_GREATER,
    LITTOK_LESS,
    LITTOK_LESS_EQUAL,
    LITTOK_LESS_LESS,
    LITTOK_PLUS,
    LITTOK_PLUS_EQUAL,
    LITTOK_PLUS_PLUS,
    LITTOK_MINUS,
    LITTOK_MINUS_EQUAL,
    LITTOK_MINUS_MINUS,
    LITTOK_STAR,
    LITTOK_STAR_EQUAL,
    LITTOK_STAR_STAR,
    LITTOK_SLASH,
    LITTOK_SLASH_EQUAL,
    LITTOK_QUESTION,
    LITTOK_QUESTION_QUESTION,
    LITTOK_PERCENT,
    LITTOK_PERCENT_EQUAL,
    LITTOK_ARROW,
    LITTOK_SMALL_ARROW,
    LITTOK_TILDE,
    LITTOK_CARET,
    LITTOK_CARET_EQUAL,
    LITTOK_DOT,
    LITTOK_DOT_DOT,
    LITTOK_DOT_DOT_DOT,
    LITTOK_SHARP,
    LITTOK_SHARP_EQUAL,

    // Literals.
    LITTOK_IDENTIFIER,
    LITTOK_STRING,
    LITTOK_INTERPOLATION,
    LITTOK_NUMBER,

    // Keywords.
    LITTOK_CLASS,
    LITTOK_ELSE,
    LITTOK_FALSE,
    LITTOK_FOR,
    LITTOK_FUNCTION,
    LITTOK_IF,
    LITTOK_NULL,
    LITTOK_RETURN,
    LITTOK_SUPER,
    LITTOK_THIS,
    LITTOK_TRUE,
    LITTOK_VAR,
    LITTOK_WHILE,
    LITTOK_CONTINUE,
    LITTOK_BREAK,
    LITTOK_NEW,
    LITTOK_EXPORT,
    LITTOK_IS,
    LITTOK_STATIC,
    LITTOK_OPERATOR,
    LITTOK_GET,
    LITTOK_SET,
    LITTOK_IN,
    LITTOK_CONST,
    LITTOK_REF,

    LITTOK_ERROR,
    LITTOK_EOF
};


enum LitAstFuncType
{
    LITFUNC_REGULAR,
    LITFUNC_SCRIPT,
    LITFUNC_METHOD,
    LITFUNC_STATIC_METHOD,
    LITFUNC_CONSTRUCTOR
};

struct LitAstExpression
{
    LitAstExprType type;
    size_t line;
};

struct LitAstPrivate
{
    bool initialized;
    bool constant;
};

struct LitAstPrivList
{
    size_t capacity;
    size_t count;
    LitAstPrivate* values;
};

struct LitAstEmitter
{
    LitState* state;
    LitChunk* chunk;
    LitAstCompiler* compiler;
    size_t last_line;
    size_t loop_start;
    LitAstPrivList privates;
    LitUintList breaks;
    LitUintList continues;
    LitModule* module;
    LitString* class_name;
    bool class_has_super;
    bool previous_was_expression_statement;
    int emit_reference;
};

struct LitAstParseRule
{
    LitAstParsePrefixFn prefix;
    LitAstParseInfixFn infix;
    LitAstPrecedence precedence;
};

/*
 * Expressions
 */
struct LitAstExprList
{
    size_t capacity;
    size_t count;
    LitAstExpression** values;
};

struct LitAstLiteralExpr
{
    LitAstExpression exobj;
    LitValue value;
};

struct LitAstBinaryExpr
{
    LitAstExpression exobj;
    LitAstExpression* left;
    LitAstExpression* right;
    LitAstTokType op;
    bool ignore_left;
};

struct LitAstUnaryExpr
{
    LitAstExpression exobj;
    LitAstExpression* right;
    LitAstTokType op;
};

struct LitAstVarExpr
{
    LitAstExpression exobj;
    const char* name;
    size_t length;
};

struct LitAstAssignExpr
{
    LitAstExpression exobj;
    LitAstExpression* to;
    LitAstExpression* value;
};

struct LitAstCallExpr
{
    LitAstExpression exobj;
    LitAstExpression* callee;
    LitAstExprList args;
    LitAstExpression* init;
};

struct LitAstGetExpr
{
    LitAstExpression exobj;
    LitAstExpression* where;
    const char* name;
    size_t length;
    int jump;
    bool ignore_emit;
    bool ignore_result;
};

struct LitAstSetExpr
{
    LitAstExpression exobj;
    LitAstExpression* where;
    const char* name;
    size_t length;
    LitAstExpression* value;
};

struct LitAstParameter
{
    const char* name;
    size_t length;
    LitAstExpression* default_value;
};

struct LitAstParamList
{
    size_t capacity;
    size_t count;
    LitAstParameter* values;
};



struct LitAstArrayExpr
{
    LitAstExpression exobj;
    LitAstExprList values;
};

struct LitAstObjectExpr
{
    LitAstExpression exobj;
    LitValList keys;
    LitAstExprList values;
};

struct LitAstIndexExpr
{
    LitAstExpression exobj;
    LitAstExpression* array;
    LitAstExpression* index;
};

struct LitAstThisExpr
{
    LitAstExpression exobj;
};

struct LitAstSuperExpr
{
    LitAstExpression exobj;
    LitString* method;
    bool ignore_emit;
    bool ignore_result;
};

struct LitAstRangeExpr
{
    LitAstExpression exobj;
    LitAstExpression* from;
    LitAstExpression* to;
};

struct LitAstTernaryExpr
{
    LitAstExpression exobj;
    LitAstExpression* condition;
    LitAstExpression* if_branch;
    LitAstExpression* else_branch;
};

struct LitAstStrInterExpr
{
    LitAstExpression exobj;
    LitAstExprList expressions;
};

struct LitAstRefExpr
{
    LitAstExpression exobj;
    LitAstExpression* to;
};

/*
 * Statements
 */

struct LitAstExprExpr
{
    LitAstExpression exobj;
    LitAstExpression* expression;
    bool pop;
};

struct LitAstBlockExpr
{
    LitAstExpression exobj;
    LitAstExprList statements;
};

struct LitAstAssignVarExpr
{
    LitAstExpression exobj;
    const char* name;
    size_t length;
    bool constant;
    LitAstExpression* init;
};

struct LitAstIfExpr
{
    LitAstExpression exobj;
    LitAstExpression* condition;
    LitAstExpression* if_branch;
    LitAstExpression* else_branch;
    LitAstExprList* elseif_conditions;
    LitAstExprList* elseif_branches;
};

struct LitAstWhileExpr
{
    LitAstExpression exobj;
    LitAstExpression* condition;
    LitAstExpression* body;
};

struct LitAstForExpr
{
    LitAstExpression exobj;
    LitAstExpression* init;
    LitAstExpression* var;
    LitAstExpression* condition;
    LitAstExpression* increment;
    LitAstExpression* body;
    bool c_style;
};

struct LitAstContinueExpr
{
    LitAstExpression exobj;
};

struct LitAstBreakExpr
{
    LitAstExpression exobj;
};

struct LitAstFunctionExpr
{
    LitAstExpression exobj;
    const char* name;
    size_t length;
    LitAstParamList parameters;
    LitAstExpression* body;
    bool exported;
};

struct LitAstReturnExpr
{
    LitAstExpression exobj;
    LitAstExpression* expression;
};

struct LitAstMethodExpr
{
    LitAstExpression exobj;
    LitString* name;
    LitAstParamList parameters;
    LitAstExpression* body;
    bool is_static;
};

struct LitAstClassExpr
{
    LitAstExpression exobj;
    LitString* name;
    LitString* parent;
    LitAstExprList fields;
};

struct LitAstFieldExpr
{
    LitAstExpression exobj;
    LitString* name;
    LitAstExpression* getter;
    LitAstExpression* setter;
    bool is_static;
};

struct LitVariable
{
    const char* name;
    size_t length;
    int depth;
    bool constant;
    bool used;
    LitValue constant_value;
    LitAstExpression** declaration;
};


struct LitExecState
{
    LitValue* slots;
    LitValue* privates;
    LitUpvalue** upvalues;
    uint8_t* ip;
    LitCallFrame* frame;
    LitChunk* current_chunk;

};


struct LitAstToken
{
    const char* start;
    LitAstTokType type;
    size_t length;
    size_t line;
    LitValue value;
};

struct LitAstLocal
{
    const char* name;
    size_t length;
    int depth;
    bool captured;
    bool constant;
};

struct LitAstLocList
{
    size_t capacity;
    size_t count;
    LitAstLocal* values;
};

struct LitAstCompUpvalue
{
    uint8_t index;
    bool isLocal;
};

struct LitAstCompiler
{
    LitAstLocList locals;
    int scope_depth;
    LitFunction* function;
    LitAstFuncType type;
    LitAstCompUpvalue upvalues[UINT8_COUNT];
    LitAstCompiler* enclosing;
    bool skip_return;
    size_t loop_depth;
    int slots;
    int max_slots;
};

struct LitAstParser
{
    LitState* state;
    bool had_error;
    bool panic_mode;
    LitAstToken previous;
    LitAstToken current;
    LitAstCompiler* compiler;
    uint8_t exprrootcnt;
    uint8_t stmtrootcnt;
};

struct LitEmulatedFile
{
    const char* source;
    size_t length;
    size_t position;
};

struct LitAstScanner
{
    size_t line;
    const char* start;
    const char* current;
    const char* file_name;
    LitState* state;
    size_t braces[LIT_MAX_INTERPOLATION_NESTING];
    size_t num_braces;
    bool had_error;
};

struct LitAstOptimizer
{
    LitState* state;
    LitVarList variables;
    int depth;
    bool mark_used;
};

static inline void lit_vm_push(LitVM* vm, LitValue value)
{
    *(vm->fiber->stack_top) = value;
    vm->fiber->stack_top++;
}

static inline LitValue lit_vm_pop(LitVM* vm)
{
    LitValue rt;
    rt = *(vm->fiber->stack_top);
    vm->fiber->stack_top--;
    return rt;
}

