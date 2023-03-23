
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <wchar.h>

#define TIN_REPOSITORY "https://github.com/egordorichev/lit"

#define TIN_VERSION_MAJOR 0
#define TIN_VERSION_MINOR 1
#define TIN_VERSION_STRING "0.1"
#define TIN_BYTECODE_VERSION 0

#define TESTING
// #define DEBUG

#ifdef DEBUG
    #define TIN_TRACE_EXECUTION
    #define TIN_TRACE_STACK
    #define TIN_CHECK_STACK_SIZE
// #define TIN_TRACE_CHUNK
// #define TIN_MINIMIZE_CONTAINERS
// #define TIN_LOG_GC
// #define TIN_LOG_ALLOCATION
// #define TIN_LOG_MARKING
// #define TIN_LOG_BLACKING
// #define TIN_STRESS_TEST_GC
#endif

#ifdef TESTING
    // So that we can actually test the map contents with a single-line expression
    #define SINGLE_LINE_MAPS
    #define SINGLE_LINE_MAPS_ENABLED true

// Make sure that we did not break anything
// #define TIN_STRESS_TEST_GC
#else
    #define SINGLE_LINE_MAPS_ENABLED false
#endif

#define TIN_MAX_INTERPOLATION_NESTING 4

#define TIN_GC_HEAP_GROW_FACTOR 2
#define TIN_CALL_FRAMES_MAX (1024*8)
#define TIN_INITIAL_CALL_FRAMES 128
#define TIN_CONTAINER_OUTPUT_MAX 10


#if defined(__ANDROID__) || defined(_ANDROID_)
    #define TIN_OS_ANDROID
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    #define TIN_OS_WINDOWS
#elif __APPLE__
    #define TIN_OS_MAC
    #define TIN_OS_UNIX_LIKE
#elif __linux__
    #define TIN_OS_LINUX
    #define TIN_OS_UNIX_LIKE
#else
    #define TIN_OS_UNKNOWN
#endif

#ifdef TIN_OS_UNIX_LIKE
    #define TIN_USE_LIBREADLINE
#endif

#ifdef TIN_USE_LIBREADLINE
#else
    #define TIN_REPL_INPUT_MAX 1024
#endif

#define UNREACHABLE assert(false);
#define UINT8_COUNT UINT8_MAX + 1
#define UINT16_COUNT UINT16_MAX + 1

#define TABLE_MAX_LOAD 0.75
// Do not change these, or old bytecode files will break!
#define TIN_BYTECODE_MAGIC_NUMBER 6932
#define TIN_BYTECODE_END_NUMBER 2942
#define TIN_STRING_KEY 48



#define TIN_TESTS_DIRECTORY "tests"

#define TIN_GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity)*2)

#define TIN_GROW_ARRAY(state, previous, typesz, oldcount, count) \
    tin_gcmem_memrealloc(state, previous, typesz * (oldcount), typesz * (count))

#define TIN_FREE_ARRAY(state, typesz, pointer, oldcount) \
    tin_gcmem_memrealloc(state, pointer, typesz * (oldcount), 0)

#define TIN_ALLOCATE(state, typesz, count) \
    tin_gcmem_memrealloc(state, NULL, 0, typesz * (count))

#define TIN_FREE(state, typesz, pointer) \
    tin_gcmem_memrealloc(state, pointer, typesz, 0)


#define TIN_GET_FIELD(id) tin_state_getfield(vm->state, &tin_value_asinstance(instance)->fields, id)
#define TIN_GET_MAP_FIELD(id) tin_state_getmapfield(vm->state, &tin_value_asinstance(instance)->fields, id)
#define TIN_SET_FIELD(id, value) tin_state_setfield(vm->state, &tin_value_asinstance(instance)->fields, id, value)
#define TIN_SET_MAP_FIELD(id, value) tin_state_setmapfield(vm->state, &tin_value_asinstance(instance)->fields, id, value)

#define TIN_ENSURE_ARGS(state, count)                                                   \
    if(argc != count)                                                       \
    {                                                                            \
        tin_vm_raiseerror(vm, "expected %i argument, got %i", count, argc); \
        return tin_value_makenull(state);                                                       \
    }

#define TIN_ENSURE_MIN_ARGS(state, count)                                                       \
    if(argc < count)                                                                \
    {                                                                                    \
        tin_vm_raiseerror(state->vm, "expected minimum %i argument, got %i", count, argc); \
        return tin_value_makenull(state);                                                               \
    }

#define TIN_ENSURE_MAX_ARGS(state, count)                                                       \
    if(argc > count)                                                                \
    {                                                                                    \
        tin_vm_raiseerror(state->vm, "expected maximum %i argument, got %i", count, argc); \
        return tin_value_makenull(state);                                                               \
    }


#if !defined(TIN_DISABLE_COLOR) && !defined(TIN_ENABLE_COLOR) && !(defined(TIN_OS_WINDOWS) || defined(EMSCRIPTEN))
    #define TIN_ENABLE_COLOR
#endif

#ifdef TIN_ENABLE_COLOR
    #define COLOR_RESET "\x1B[0m"
    #define COLOR_RED "\x1B[31m"
    #define COLOR_GREEN "\x1B[32m"
    #define COLOR_YELLOW "\x1B[33m"
    #define COLOR_BLUE "\x1B[34m"
    #define COLOR_MAGENTA "\x1B[35m"
    #define COLOR_CYAN "\x1B[36m"
    #define COLOR_WHITE "\x1B[37m"
#else
    #define COLOR_RESET ""
    #define COLOR_RED ""
    #define COLOR_GREEN ""
    #define COLOR_YELLOW ""
    #define COLOR_BLUE ""
    #define COLOR_MAGENTA ""
    #define COLOR_CYAN ""
    #define COLOR_WHITE ""
#endif


#define PUSH(value) (*fiber->stack_top++ = value)

#if defined(__cplusplus)
    #define TIN_MAKESTATUS(code, value) TinInterpretResult{code, value}
#else
    #define TIN_MAKESTATUS(code, value) (TinInterpretResult){code, value}
#endif

#define RETURN_OK(r) return TIN_MAKESTATUS(TINSTATE_OK, r)
#define RETURN_RUNTIME_ERROR(state) return TIN_MAKESTATUS(TINSTATE_RUNTIMEERROR, tin_value_makenull(state))
#define INTERPRET_RUNTIME_FAIL(state) (TIN_MAKESTATUS(TINSTATE_INVALID, tin_value_makenull(state)))

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


enum TinOpCode
{
    OP_POP,
    OP_RETURN,
    OP_CONSTVALUE,
    OP_CONSTLONG,
    OP_VALTRUE,
    OP_VALFALSE,
    OP_VALNULL,
    OP_VALARRAY,
    OP_VALOBJECT,
    OP_RANGE,
    OP_NEGATE,
    OP_NOT,
    OP_MATHADD,
    OP_MATHSUB,
    OP_MATHMULT,
    OP_MATHPOWER,
    OP_MATHDIV,
    OP_MATHFLOORDIV,
    OP_MATHMOD,
    OP_BINAND,
    OP_BINOR,
    OP_BINXOR,
    OP_LEFTSHIFT,
    OP_RIGHTSHIFT,
    OP_BINNOT,
    OP_EQUAL,
    OP_GREATERTHAN,
    OP_GREATEREQUAL,
    OP_LESSTHAN,
    OP_LESSEQUAL,
    OP_GLOBALSET,
    OP_GLOBALGET,
    OP_LOCALSET,
    OP_LOCALGET,
    OP_LOCALLONGSET,
    OP_LOCALLONGGET,
    OP_PRIVATESET,
    OP_PRIVATEGET,
    OP_PRIVATELONGSET,
    OP_PRIVATELONGGET,
    OP_UPVALSET,
    OP_UPVALGET,

    OP_JUMPIFFALSE,
    OP_JUMPIFNULL,
    OP_JUMPIFNULLPOP,
    OP_JUMPALWAYS,
    OP_JUMPBACK,
    OP_AND,
    OP_OR,
    OP_NULLOR,

    OP_MAKECLOSURE,
    OP_UPVALCLOSE,

    OP_MAKECLASS,
    OP_FIELDGET,
    OP_FIELDSET,

    // [array] [index] -> [value]
    OP_GETINDEX,
    // [array] [index] [value] -> [value]
    OP_SETINDEX,
    // [array] [value] -> [array]
    OP_ARRAYPUSHVALUE,
    // [map] [slot] [value] -> [map]
    OP_OBJECTPUSHFIELD,

    // [class] [method] -> [class]
    OP_MAKEMETHOD,
    // [class] [method] -> [class]
    OP_FIELDSTATIC,
    OP_FIELDDEFINE,
    OP_CLASSINHERIT,
    // [instance] [class] -> [bool]
    OP_ISCLASS,
    OP_GETSUPERMETHOD,

    // Varying stack effect
    OP_CALLFUNCTION,
    OP_INVOKEMETHOD,
    OP_INVOKESUPER,
    OP_INVOKEIGNORING,
    OP_INVOKESUPERIGNORING,
    OP_POPLOCALS,
    OP_VARARG,

    OP_REFGLOBAL,
    OP_REFPRIVATE,
    OP_REFLOCAL,
    OP_REFUPVAL,
    OP_REFFIELD,

    OP_REFSET,
};


enum TinObjType
{
    TINTYPE_UNDEFINED,
    TINTYPE_NULL,
    TINTYPE_STRING,
    TINTYPE_FUNCTION,
    TINTYPE_NATIVEFUNCTION,
    TINTYPE_NATIVEPRIMITIVE,
    TINTYPE_NATIVEMETHOD,
    TINTYPE_PRIMITIVEMETHOD,
    TINTYPE_FIBER,
    TINTYPE_MODULE,
    TINTYPE_CLOSURE,
    TINTYPE_UPVALUE,
    TINTYPE_CLASS,
    TINTYPE_INSTANCE,
    TINTYPE_BOUNDMETHOD,
    TINTYPE_ARRAY,
    TINTYPE_MAP,
    TINTYPE_USERDATA,
    TINTYPE_RANGE,
    TINTYPE_FIELD,
    TINTYPE_REFERENCE,
    TINTYPE_NUMBER,
    TINTYPE_BOOL,
};

enum TinValType
{
    TINVAL_NULL,
    TINVAL_BOOL,
    TINVAL_NUMBER,
    TINVAL_OBJECT,
};

enum TinStatus
{
    TINSTATE_OK,
    TINSTATE_COMPILEERROR,
    TINSTATE_RUNTIMEERROR,
    TINSTATE_INVALID
};

enum TinErrType
{
    COMPILE_ERROR,
    RUNTIME_ERROR
};

enum TinAstOptLevel
{
    TINOPTLEVEL_NONE,
    TINOPTLEVEL_REPL,
    TINOPTLEVEL_DEBUG,
    TINOPTLEVEL_RELEASE,
    TINOPTLEVEL_EXTREME,
    TINOPTLEVEL_TOTAL
};

enum TinAstOptType
{
    TINOPTSTATE_CONSTANTFOLDING,
    TINOPTSTATE_LITERALFOLDING,
    TINOPTSTATE_UNUSEDVAR,
    TINOPTSTATE_UNREACHABLECODE,
    TINOPTSTATE_EMPTYBODY,
    TINOPTSTATE_LINEINFO,
    TINOPTSTATE_PRIVATENAMES,
    TINOPTSTATE_CFOR,
    TINOPTSTATE_TOTAL
};



typedef enum /**/ TinValType TinValType;
typedef enum /**/ TinObjType TinObjType;
typedef struct /**/ TinObject TinObject;
typedef struct /**/ TinValue TinValue;

typedef enum /**/TinOpCode TinOpCode;
typedef enum /**/TinAstExprType TinAstExprType;
typedef enum /**/TinAstOptLevel TinAstOptLevel;
typedef enum /**/TinAstOptType TinAstOptType;
typedef enum /**/TinAstPrecedence TinAstPrecedence;
typedef enum /**/TinAstTokType TinAstTokType;
typedef enum /**/TinStatus TinStatus;
typedef enum /**/TinErrType TinErrType;
typedef enum /**/TinAstFuncType TinAstFuncType;
typedef struct /**/TinAstScanner TinAstScanner;
typedef struct /**/TinExecState TinExecState;
typedef struct /**/TinVM TinVM;
typedef struct /**/TinAstParser TinAstParser;
typedef struct /**/TinAstEmitter TinAstEmitter;
typedef struct /**/TinAstOptimizer TinAstOptimizer;
typedef struct /**/TinState TinState;
typedef struct /**/TinInterpretResult TinInterpretResult;
typedef struct /**/TinMap TinMap;
typedef struct /**/TinNumber TinNumber;
typedef struct /**/TinString TinString;
typedef struct /**/TinModule TinModule;
typedef struct /**/TinFiber TinFiber;
typedef struct /**/TinUserdata TinUserdata;
typedef struct /**/TinChunk TinChunk;
typedef struct /**/TinTabEntry TinTabEntry;
typedef struct /**/TinTable TinTable;
typedef struct /**/TinFunction TinFunction;
typedef struct /**/TinUpvalue TinUpvalue;
typedef struct /**/TinClosure TinClosure;
typedef struct /**/TinNativeFunction TinNativeFunction;
typedef struct /**/TinNativePrimFunction TinNativePrimFunction;
typedef struct /**/TinNativeMethod TinNativeMethod;
typedef struct /**/TinPrimitiveMethod TinPrimitiveMethod;
typedef struct /**/TinCallFrame TinCallFrame;
typedef struct /**/TinClass TinClass;
typedef struct /**/TinInstance TinInstance;
typedef struct /**/TinBoundMethod TinBoundMethod;
typedef struct /**/TinArray TinArray;
typedef struct /**/TinRange TinRange;
typedef struct /**/TinField TinField;
typedef struct /**/TinReference TinReference;
typedef struct /**/TinAstToken TinAstToken;
typedef struct /**/TinAstExpression TinAstExpression;
typedef struct /**/TinAstCompUpvalue TinAstCompUpvalue;
typedef struct /**/TinAstCompiler TinAstCompiler;
typedef struct /**/TinAstParseRule TinAstParseRule;
typedef struct /**/TinEmulatedFile TinEmulatedFile;
typedef struct /**/TinVariable TinVariable;
typedef struct /**/TinWriter TinWriter;
typedef struct /**/TinAstLocal TinAstLocal;
typedef struct /**/TinConfig TinConfig;

/* ARRAYTYPES */
typedef struct /**/TinVarList TinVarList;
typedef struct /**/TinUintList TinUintList;
typedef struct /**/TinValList TinValList;
typedef struct /**/TinAstExprList TinAstExprList;
typedef struct /**/TinAstParamList TinAstParamList;
typedef struct /**/TinAstPrivList TinAstPrivList;
typedef struct /**/TinAstLocList TinAstLocList;
typedef struct /**/TinDataList TinDataList;
typedef struct /**/TinAstByteList TinAstByteList;

/* ast/compiler types */
typedef struct /**/TinAstLiteralExpr TinAstLiteralExpr;
typedef struct /**/TinAstBinaryExpr TinAstBinaryExpr;
typedef struct /**/TinAstUnaryExpr TinAstUnaryExpr;
typedef struct /**/TinAstVarExpr TinAstVarExpr;
typedef struct /**/TinAstAssignExpr TinAstAssignExpr;
typedef struct /**/TinAstCallExpr TinAstCallExpr;
typedef struct /**/TinAstGetExpr TinAstGetExpr;
typedef struct /**/TinAstSetExpr TinAstSetExpr;
typedef struct /**/TinAstParameter TinAstParameter;
typedef struct /**/TinAstArrayExpr TinAstArrayExpr;
typedef struct /**/TinAstObjectExpr TinAstObjectExpr;
typedef struct /**/TinAstIndexExpr TinAstIndexExpr;
typedef struct /**/TinAstThisExpr TinAstThisExpr;
typedef struct /**/TinAstSuperExpr TinAstSuperExpr;
typedef struct /**/TinAstRangeExpr TinAstRangeExpr;
typedef struct /**/TinAstTernaryExpr TinAstTernaryExpr;
typedef struct /**/TinAstStrInterExpr TinAstStrInterExpr;
typedef struct /**/TinAstRefExpr TinAstRefExpr;
typedef struct /**/TinAstExprExpr TinAstExprExpr;
typedef struct /**/TinAstBlockExpr TinAstBlockExpr;
typedef struct /**/TinAstAssignVarExpr TinAstAssignVarExpr;
typedef struct /**/TinAstIfExpr TinAstIfExpr;
typedef struct /**/TinAstWhileExpr TinAstWhileExpr;
typedef struct /**/TinAstForExpr TinAstForExpr;
typedef struct /**/TinAstContinueExpr TinAstContinueExpr;
typedef struct /**/TinAstBreakExpr TinAstBreakExpr;
typedef struct /**/TinAstFunctionExpr TinAstFunctionExpr;
typedef struct /**/TinAstReturnExpr TinAstReturnExpr;
typedef struct /**/TinAstMethodExpr TinAstMethodExpr;
typedef struct /**/TinAstClassExpr TinAstClassExpr;
typedef struct /**/TinAstFieldExpr TinAstFieldExpr;
typedef struct /**/TinAstPrivate TinAstPrivate;

typedef struct TinAstWriterState TinAstWriterState;


/* forward decls to make prot.inc work */
typedef struct /**/TinDirReader TinDirReader;
typedef struct /**/TinDirItem TinDirItem;

typedef TinAstExpression* (*TinAstParsePrefixFn)(TinAstParser*, bool);
typedef TinAstExpression* (*TinAstParseInfixFn)(TinAstParser*, TinAstExpression*, bool);


typedef TinValue (*TinNativeFunctionFn)(TinVM*, size_t, TinValue*);
typedef bool (*TinNativePrimitiveFn)(TinVM*, size_t, TinValue*);
typedef TinValue (*TinNativeMethodFn)(TinVM*, TinValue, size_t arg_count, TinValue*);
typedef bool (*TinPrimitiveMethodFn)(TinVM*, TinValue, size_t, TinValue*);
typedef TinValue (*TinMapIndexFn)(TinVM*, TinMap*, TinString*, TinValue*);
typedef void (*TinCleanupFn)(TinState*, TinUserdata*, bool mark);
typedef void (*TinErrorFn)(TinState*, const char*);
typedef void (*TinPrintFn)(TinState*, const char*);

typedef void(*TinWriterByteFN)(TinWriter*, int);
typedef void(*TinWriterStringFN)(TinWriter*, const char*, size_t);
typedef void(*TinWriterFormatFN)(TinWriter*, const char*, va_list);

struct TinObject
{
    /* the type of this object */
    TinObjType type;
    TinObject* next;
    bool marked;
    bool mustfree;
};

struct TinValue
{
    TinValType type;
    bool isfixednumber;
    union
    {
        bool boolval;
        int64_t numfixedval;
        double numfloatval;
        TinObject* obj;
    };
};

struct TinValList
{
    size_t capacity;
    size_t count;
    TinValue* values;
};

struct TinDataList
{
    /* how many values *could* this list hold? */
    size_t capacity;

    /* actual amount of values in this list */
    size_t count;

    size_t rawelemsz;
    size_t elemsz;

    /* the actual values */
    intptr_t* values;
};

struct TinVarList
{
    size_t capacity;
    size_t count;
    TinVariable* values;
};

struct TinUintList
{
    TinDataList list;
};


/* TODO: using DataList messes with the string its supposed to collect. no clue why, though. */
struct TinAstByteList
{
    size_t capacity;
    size_t count;
    uint8_t* values;
};

struct TinChunk
{
    /* how many items this chunk holds */
    size_t count;
    size_t capacity;
    uint8_t* code;
    bool has_line_info;
    size_t line_count;
    size_t line_capacity;
    uint16_t* lines;
    TinValList constants;
};

struct TinWriter
{
    TinState* state;

    /* the main pointer, that either holds a pointer to a TinString, or a FILE */
    void* uptr;

    /* if true, then uptr is a TinString, otherwise it's a FILE */
    bool stringmode;

    /* if true, and !stringmode, then calls fflush() after each i/o operations */
    bool forceflush;

    /* the callback that emits a single character */
    TinWriterByteFN fnbyte;

    /* the callback that emits a string */
    TinWriterStringFN fnstring;

    /* the callback that emits a format string (printf-style) */
    TinWriterFormatFN fnformat;
};


struct TinTabEntry
{
    /* the key of this entry. can be NULL! */
    TinString* key;

    /* the associated value */
    TinValue value;
};

struct TinTable
{
    TinState* state;

    /* how many entries are in this table */
    int count;

    /* how many entries could be held */
    int capacity;

    /* the actual entries */
    TinTabEntry* entries;
};

struct TinNumber
{
    TinObject object;
    double num;
};

struct TinString
{
    TinObject object;
    /* the hash of this string - note that it is only unique to the context! */
    uint32_t hash;
    /* this is handled by sds - use tin_string_getlength to get the length! */
    char* data;
};

struct TinFunction
{
    TinObject object;
    TinChunk chunk;
    TinString* name;
    uint8_t arg_count;
    uint16_t upvalue_count;
    size_t maxslots;
    bool vararg;
    TinModule* module;
};

struct TinUpvalue
{
    TinObject object;
    TinValue* location;
    TinValue closed;
    TinUpvalue* next;
};

struct TinClosure
{
    TinObject object;
    TinFunction* function;
    TinUpvalue** upvalues;
    size_t upvalue_count;
};

struct TinNativeFunction
{
    TinObject object;
    /* the native callback for this function */
    TinNativeFunctionFn function;
    /* the name of this function */
    TinString* name;
};

struct TinNativePrimFunction
{
    TinObject object;
    TinNativePrimitiveFn function;
    TinString* name;
};

struct TinNativeMethod
{
    TinObject object;
    TinNativeMethodFn method;
    TinString* name;
};

struct TinPrimitiveMethod
{
    TinObject object;
    TinPrimitiveMethodFn method;
    TinString* name;
};

struct TinCallFrame
{
    TinFunction* function;
    TinClosure* closure;
    uint8_t* ip;
    TinValue* slots;
    bool result_ignored;
    bool return_to_c;
};

struct TinMap
{
    TinObject object;
    /* the table that holds the actual entries */
    TinTable values;
    /* the index function corresponding to operator[] */
    TinMapIndexFn index_fn;
};

struct TinModule
{
    TinObject object;
    TinValue return_value;
    TinString* name;
    TinValue* privates;
    TinMap* private_names;
    size_t private_count;
    TinFunction* main_function;
    TinFiber* main_fiber;
    bool ran;
};

struct TinFiber
{
    TinObject object;
    TinFiber* parent;
    TinValue* stack;
    TinValue* stack_top;
    size_t stack_capacity;
    TinCallFrame* frames;
    size_t frame_capacity;
    size_t frame_count;
    size_t arg_count;
    TinUpvalue* open_upvalues;
    TinModule* module;
    TinValue errorval;
    bool abort;
    bool catcher;
};

struct TinClass
{
    TinObject object;
    /* the name of this class */
    TinString* name;
    /* the constructor object */
    TinObject* init_method;
    /* runtime methods */
    TinTable methods;
    /* static fields, which include functions, and variables */
    TinTable static_fields;
    /*
    * the parent class - the topmost is always TinClass, followed by TinObject.
    * that is, eg for TinString: TinString <- TinObject <- TinClass
    */
    TinClass* super;
};

struct TinInstance
{
    TinObject object;
    /* the class that corresponds to this instance */
    TinClass* klass;
    TinTable fields;
};

struct TinBoundMethod
{
    TinObject object;
    TinValue receiver;
    TinValue method;
};

struct TinArray
{
    TinObject object;
    TinValList list;
};

struct TinUserdata
{
    TinObject object;
    void* data;
    size_t size;
    TinCleanupFn cleanup_fn;
    bool canfree;
};

struct TinRange
{
    TinObject object;
    double from;
    double to;
};

struct TinField
{
    TinObject object;
    TinObject* getter;
    TinObject* setter;
};

struct TinReference
{
    TinObject object;
    TinValue* slot;
};

struct TinInterpretResult
{
    /* the result of this interpret/tin_vm_callcallable attempt */
    TinStatus type;
    /* the value returned from this interpret/tin_vm_callcallable attempt */
    TinValue result;
};

struct TinConfig
{
    bool dumpbytecode;
    bool dumpast;
    bool runafterdump;
};

struct TinState
{
    TinConfig config;
    TinWriter stdoutwriter;
    /* how much was allocated in total? */
    int64_t gcbytescount;
    int64_t gcnext;
    bool gcallow;
    TinValList gclightobjects;
    TinErrorFn errorfn;
    TinPrintFn printfn;
    TinValue* gcroots;
    size_t gcrootcount;
    size_t gcrootcapacity;
    TinAstScanner* scanner;
    TinAstParser* parser;
    TinAstEmitter* emitter;
    TinAstOptimizer* optimizer;
    /*
    * recursive pointer to the current VM instance.
    * using 'state->vm->state' will in turn mean this instance, etc.
    */
    TinVM* vm;
    bool haderror;
    TinFunction* capifunction;
    TinFiber* capifiber;
    TinString* capiname;
    /* when using debug routines, this is the writer that output is called on */
    TinWriter debugwriter;
    // class class
    // Mental note:
    // When adding another class here, DO NOT forget to mark it in tin_mem.c or it will be GC-ed
    TinClass* primclassclass;
    TinClass* primobjectclass;
    TinClass* primnumberclass;
    TinClass* primstringclass;
    TinClass* primboolclass;
    TinClass* primfunctionclass;
    TinClass* primfiberclass;
    TinClass* primmoduleclass;
    TinClass* primarrayclass;
    TinClass* primmapclass;
    TinClass* primrangeclass;
    TinModule* last_module;
};

struct TinVM
{
    /* the current state */
    TinState* state;
    /* currently held objects */
    TinObject* gcobjects;
    /* currently cached strings */
    TinTable gcstrings;
    /* currently loaded/defined modules */
    TinMap* modules;
    /* currently defined globals */
    TinMap* globals;
    TinFiber* fiber;
    // For garbage collection
    size_t gcgraycount;
    size_t gcgraycapacity;
    TinObject** gcgraystack;
};


/*
#define tin_value_asnumber(v) \
    ( (tin_value_isnumber(v) ? (\
            ((v).isfixednumber) ? \
                ((v).numfixedval) : \
                ((v).numfloatval) \
            ) : ( \
                tin_value_isnull(v) ? 0 : ( \
                    tin_value_isbool(v) ? tin_value_asbool(v) : 0 \
                )\
            )))

*/
#define tin_value_asnumber(v) \
        ( \
            ((v).isfixednumber) ? ((v).numfixedval) : ((v).numfloatval) )


#define tin_value_fromobject(obj) tin_value_fromobject_actual((TinObject*)obj)

#define tin_value_istype(value, t) \
    (tin_value_isobject(value) && (tin_value_asobject(value) != NULL) && (tin_value_asobject(value)->type == t))


#define tin_value_isstring(value) \
    tin_value_istype(value, TINTYPE_STRING)

#define tin_value_isfunction(value) \
    tin_value_istype(value, TINTYPE_FUNCTION)

#define tin_value_isnatfunction(value) \
    tin_value_istype(value, TINTYPE_NATIVEFUNCTION)

#define tin_value_isnatprim(value) \
    tin_value_istype(value, TINTYPE_NATIVEPRIMITIVE)

#define tin_value_isnatmethod(value) \
    tin_value_istype(value, TINTYPE_NATIVEMETHOD)

#define tin_value_isprimmethod(value) \
    tin_value_istype(value, TINTYPE_PRIMITIVEMETHOD)

#define tin_value_ismodule(value) \
    tin_value_istype(value, TINTYPE_MODULE)

#define tin_value_isclosure(value) \
    tin_value_istype(value, TINTYPE_CLOSURE)

#define tin_value_isupvalue(value) \
    tin_value_istype(value, TINTYPE_UPVALUE)

#define tin_value_isclass(value) \
    tin_value_istype(value, TINTYPE_CLASS)

#define tin_value_isinstance(value) \
    tin_value_istype(value, TINTYPE_INSTANCE)

#define tin_value_isarray(value) \
    tin_value_istype(value, TINTYPE_ARRAY)

#include "protall.inc"

static inline bool tin_value_isnull(TinValue v)
{
    return (v.type == TINVAL_NULL);
}

static inline bool tin_value_isbool(TinValue v)
{
    return v.type == TINVAL_BOOL;
}

static inline bool tin_value_isnumber(TinValue v)
{
    return v.type == TINVAL_NUMBER;
}

static inline bool tin_value_isobject(TinValue v)
{
    return v.type == TINVAL_OBJECT;
}

static inline bool tin_value_isfalsey(TinValue v)
{
    return (
        (tin_value_isbool(v) && !v.boolval) ||
        tin_value_isnull(v) ||
        (tin_value_isnumber(v) && tin_value_asnumber(v) == 0)
    );
}

static inline bool tin_value_ismap(TinValue value)
{
    return tin_value_istype(value, TINTYPE_MAP);
}

static inline bool tin_value_isboundmethod(TinValue value)
{
    return tin_value_istype(value, TINTYPE_BOUNDMETHOD);
}

static inline bool tin_value_isuserdata(TinValue value)
{
    return tin_value_istype(value, TINTYPE_USERDATA);
}

static inline bool tin_value_isrange(TinValue value)
{
    return tin_value_istype(value, TINTYPE_RANGE);
}

static inline bool tin_value_isfield(TinValue value)
{
    return tin_value_istype(value, TINTYPE_FIELD);
}

static inline bool tin_value_isreference(TinValue value)
{
    return tin_value_istype(value, TINTYPE_REFERENCE);
}

static inline bool tin_value_asbool(TinValue v)
{
    return v.boolval;
}

static inline double tin_value_asfloatnumber(TinValue v)
{
    if(v.isfixednumber)
    {
        return v.numfixedval;
    }
    return v.numfloatval;
}

static inline int64_t tin_value_asfixednumber(TinValue v)
{
    if(!v.isfixednumber)
    {
        return v.numfloatval;
    }
    return v.numfixedval;
}

static inline TinString* tin_value_asstring(TinValue v)
{
    return (TinString*)tin_value_asobject(v);
}

static inline char* tin_value_ascstring(TinValue v)
{
    return (tin_value_asstring(v)->data);
}

static inline TinFunction* tin_value_asfunction(TinValue v)
{
    return (TinFunction*)tin_value_asobject(v);
}

static inline TinNativeFunction* tin_value_asnativefunction(TinValue v)
{
    return (TinNativeFunction*)tin_value_asobject(v);
}

static inline TinNativePrimFunction* tin_value_asnativeprimitive(TinValue v)
{
    return (TinNativePrimFunction*)tin_value_asobject(v);
}

static inline TinNativeMethod* tin_value_asnativemethod(TinValue v)
{
    return (TinNativeMethod*)tin_value_asobject(v);
}

static inline TinPrimitiveMethod* tin_value_asprimitivemethod(TinValue v)
{
    return (TinPrimitiveMethod*)tin_value_asobject(v);
}

static inline TinModule* tin_value_asmodule(TinValue v)
{
    return (TinModule*)tin_value_asobject(v);
}

static inline TinClosure* tin_value_asclosure(TinValue v)
{
    return (TinClosure*)tin_value_asobject(v);
}

static inline TinUpvalue* tin_value_asupvalue(TinValue v)
{
    return (TinUpvalue*)tin_value_asobject(v);
}

static inline TinClass* tin_value_asclass(TinValue v)
{
    return (TinClass*)tin_value_asobject(v);
}

static inline TinInstance* tin_value_asinstance(TinValue v)
{
    return (TinInstance*)tin_value_asobject(v);
}

static inline TinArray* tin_value_asarray(TinValue v)
{
    return (TinArray*)tin_value_asobject(v);
}

static inline TinMap* tin_value_asmap(TinValue v)
{
    return (TinMap*)tin_value_asobject(v);
}

static inline TinBoundMethod* tin_value_asboundmethod(TinValue v)
{
    return (TinBoundMethod*)tin_value_asobject(v);
}

static inline TinUserdata* tin_value_asuserdata(TinValue v)
{
    return (TinUserdata*)tin_value_asobject(v);
}

static inline TinRange* tin_value_asrange(TinValue v)
{
    return (TinRange*)tin_value_asobject(v);
}

static inline TinField* tin_value_asfield(TinValue v)
{
    return (TinField*)tin_value_asobject(v);
}

static inline TinFiber* tin_value_asfiber(TinValue v)
{
    return (TinFiber*)tin_value_asobject(v);
}

static inline TinReference* tin_value_asreference(TinValue v)
{
    return (TinReference*)tin_value_asobject(v);
}

static inline void tin_value_setnull(TinValue* tv)
{
    tv->type = TINVAL_NULL;
    tv->isfixednumber = false;
    tv->numfixedval = 0;
    tv->numfloatval = 0;
    tv->boolval = false;
}

static inline TinValue tin_value_makebool(TinState* state, bool b) 
{
    TinValue tv;
    (void)state;
    tin_value_setnull(&tv);
    tv.type = TINVAL_BOOL;
    tv.boolval = b;
    return tv;
}

static inline TinValue tin_value_makenull(TinState* state)
{
    TinValue tv;
    (void)state;
    tin_value_setnull(&tv);
    tv.type = TINVAL_NULL;
    return tv;
}

static inline TinValue tin_value_makestring(TinState* state, const char* text)
{
    return tin_value_fromobject(tin_string_copy((state), (text), strlen(text)));
}

static inline TinString* tin_string_copyconst(TinState* state, const char* text)
{
    return tin_string_copy(state, text, strlen(text));
}
