
#include "lit.h"

enum LitOpCode
{
#define OPCODE(name, effect) OP_##name,
#include "opcodes.inc"
#undef OPCODE
};

enum LitExprType
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


enum LitError
{
    // Preprocessor errors
    LITERROR_UNCLOSED_MACRO,
    LITERROR_UNKNOWN_MACRO,

    // Scanner errors
    LITERROR_UNEXPECTED_CHAR,
    LITERROR_UNTERMINATED_STRING,
    LITERROR_INVALID_ESCAPE_CHAR,
    LITERROR_INTERPOLATION_NESTING_TOO_DEEP,
    LITERROR_NUMBER_IS_TOO_BIG,
    LITERROR_CHAR_EXPECTATION_UNMET,

    // Parser errors
    LITERROR_EXPECTATION_UNMET,
    LITERROR_INVALID_ASSIGMENT_TARGET,
    LITERROR_TOO_MANY_FUNCTION_ARGS,
    LITERROR_MULTIPLE_ELSE_BRANCHES,
    LITERROR_VAR_MISSING_IN_FORIN,
    LITERROR_NO_GETTER_AND_SETTER,
    LITERROR_STATIC_OPERATOR,
    LITERROR_SELF_INHERITED_CLASS,
    LITERROR_STATIC_FIELDS_AFTER_METHODS,
    LITERROR_MISSING_STATEMENT,
    LITERROR_EXPECTED_EXPRESSION,
    LITERROR_DEFAULT_ARG_CENTRED,

    // Emitter errors
    LITERROR_TOO_MANY_CONSTANTS,
    LITERROR_TOO_MANY_PRIVATES,
    LITERROR_VAR_REDEFINED,
    LITERROR_TOO_MANY_LOCALS,
    LITERROR_TOO_MANY_UPVALUES,
    LITERROR_VARIABLE_USED_IN_INIT,
    LITERROR_JUMP_TOO_BIG,
    LITERROR_NO_SUPER,
    LITERROR_THIS_MISSUSE,
    LITERROR_SUPER_MISSUSE,
    LITERROR_UNKNOWN_EXPRESSION,
    LITERROR_UNKNOWN_STATEMENT,
    LITERROR_LOOP_JUMP_MISSUSE,
    LITERROR_RETURN_FROM_CONSTRUCTOR,
    LITERROR_STATIC_CONSTRUCTOR,
    LITERROR_CONSTANT_MODIFIED,
    LITERROR_INVALID_REFERENCE_TARGET,

    LITERROR_TOTAL
};

enum LitPrecedence
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

enum LitTokType
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


enum LitFuncType
{
    LITFUNC_REGULAR,
    LITFUNC_SCRIPT,
    LITFUNC_METHOD,
    LITFUNC_STATIC_METHOD,
    LITFUNC_CONSTRUCTOR
};

struct LitAstExpression
{
    LitExprType type;
    size_t line;
};

struct LitPrivate
{
    bool initialized;
    bool constant;
};

struct LitPrivList
{
    size_t capacity;
    size_t count;
    LitPrivate* values;
};

struct LitEmitter
{
    LitState* state;
    LitChunk* chunk;
    LitCompiler* compiler;
    size_t last_line;
    size_t loop_start;
    LitPrivList privates;
    LitUintList breaks;
    LitUintList continues;
    LitModule* module;
    LitString* class_name;
    bool class_has_super;
    bool previous_was_expression_statement;
    int emit_reference;
};

struct LitParseRule
{
    LitPrefixParseFn prefix;
    LitInfixParseFn infix;
    LitPrecedence precedence;
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
    LitTokType op;
    bool ignore_left;
};

struct LitAstUnaryExpr
{
    LitAstExpression exobj;
    LitAstExpression* right;
    LitTokType op;
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


struct LitAstLambdaExpr
{
    LitAstExpression exobj;
    LitAstParamList parameters;
    LitAstExpression* body;
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


struct LitToken
{
    const char* start;
    LitTokType type;
    size_t length;
    size_t line;
    LitValue value;
};

struct LitLocal
{
    const char* name;
    size_t length;
    int depth;
    bool captured;
    bool constant;
};

struct LitLocList
{
    size_t capacity;
    size_t count;
    LitLocal* values;
};

struct LitCompilerUpvalue
{
    uint8_t index;
    bool isLocal;
};

struct LitCompiler
{
    LitLocList locals;
    int scope_depth;
    LitFunction* function;
    LitFuncType type;
    LitCompilerUpvalue upvalues[UINT8_COUNT];
    LitCompiler* enclosing;
    bool skip_return;
    size_t loop_depth;
    int slots;
    int max_slots;
};

struct LitParser
{
    LitState* state;
    bool had_error;
    bool panic_mode;
    LitToken previous;
    LitToken current;
    LitCompiler* compiler;
    uint8_t expression_root_count;
    uint8_t statement_root_count;
};

struct LitEmulatedFile
{
    const char* source;
    size_t length;
    size_t position;
};

struct LitScanner
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

struct LitOptimizer
{
    LitState* state;
    LitVarList variables;
    int depth;
    bool mark_used;
};


static inline bool lit_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static inline bool lit_is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}


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

