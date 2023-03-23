
#pragma once
#include "tin.h"

#if defined(__GNUC__)
    #define TIN_VM_INLINE static inline
#else
    #define TIN_VM_INLINE static
#endif


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
    size_t srclength;
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

