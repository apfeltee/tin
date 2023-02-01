
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


#define LIT_REPOSITORY "https://github.com/egordorichev/lit"

#define LIT_VERSION_MAJOR 0
#define LIT_VERSION_MINOR 1
#define LIT_VERSION_STRING "0.1"
#define LIT_BYTECODE_VERSION 0

#define TESTING
// #define DEBUG

#ifdef DEBUG
    #define LIT_TRACE_EXECUTION
    #define LIT_TRACE_STACK
    #define LIT_CHECK_STACK_SIZE
// #define LIT_TRACE_CHUNK
// #define LIT_MINIMIZE_CONTAINERS
// #define LIT_LOG_GC
// #define LIT_LOG_ALLOCATION
// #define LIT_LOG_MARKING
// #define LIT_LOG_BLACKING
// #define LIT_STRESS_TEST_GC
#endif

#ifdef TESTING
    // So that we can actually test the map contents with a single-line expression
    #define SINGLE_LINE_MAPS
    #define SINGLE_LINE_MAPS_ENABLED true

// Make sure that we did not break anything
// #define LIT_STRESS_TEST_GC
#else
    #define SINGLE_LINE_MAPS_ENABLED false
#endif

#define LIT_MAX_INTERPOLATION_NESTING 4

#define LIT_GC_HEAP_GROW_FACTOR 2
#define LIT_CALL_FRAMES_MAX (1024*8)
#define LIT_INITIAL_CALL_FRAMES 128
#define LIT_CONTAINER_OUTPUT_MAX 10


#if defined(__ANDROID__) || defined(_ANDROID_)
    #define LIT_OS_ANDROID
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    #define LIT_OS_WINDOWS
#elif __APPLE__
    #define LIT_OS_MAC
    #define LIT_OS_UNIX_LIKE
#elif __linux__
    #define LIT_OS_LINUX
    #define LIT_OS_UNIX_LIKE
#else
    #define LIT_OS_UNKNOWN
#endif

#ifdef LIT_OS_UNIX_LIKE
    #define LIT_USE_LIBREADLINE
#endif

#ifdef LIT_USE_LIBREADLINE
#else
    #define LIT_REPL_INPUT_MAX 1024
#endif

#define UNREACHABLE assert(false);
#define UINT8_COUNT UINT8_MAX + 1
#define UINT16_COUNT UINT16_MAX + 1

#define TABLE_MAX_LOAD 0.75
// Do not change these, or old bytecode files will break!
#define LIT_BYTECODE_MAGIC_NUMBER 6932
#define LIT_BYTECODE_END_NUMBER 2942
#define LIT_STRING_KEY 48



#define LIT_TESTS_DIRECTORY "tests"

#define LIT_GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity)*2)

#define LIT_GROW_ARRAY(state, previous, typesz, old_count, count) \
    lit_gcmem_memrealloc(state, previous, typesz * (old_count), typesz * (count))

#define LIT_FREE_ARRAY(state, typesz, pointer, old_count) \
    lit_gcmem_memrealloc(state, pointer, typesz * (old_count), 0)

#define LIT_ALLOCATE(state, typesz, count) \
    lit_gcmem_memrealloc(state, NULL, 0, typesz * (count))

#define LIT_FREE(state, typesz, pointer) \
    lit_gcmem_memrealloc(state, pointer, typesz, 0)


#define LIT_GET_FIELD(id) lit_state_getfield(vm->state, &lit_value_asinstance(instance)->fields, id)
#define LIT_GET_MAP_FIELD(id) lit_state_getmapfield(vm->state, &lit_value_asinstance(instance)->fields, id)
#define LIT_SET_FIELD(id, value) lit_state_setfield(vm->state, &lit_value_asinstance(instance)->fields, id, value)
#define LIT_SET_MAP_FIELD(id, value) lit_state_setmapfield(vm->state, &lit_value_asinstance(instance)->fields, id, value)

#define LIT_ENSURE_ARGS(state, count)                                                   \
    if(argc != count)                                                       \
    {                                                                            \
        lit_vm_raiseerror(vm, "expected %i argument, got %i", count, argc); \
        return lit_value_makenull(state);                                                       \
    }

#define LIT_ENSURE_MIN_ARGS(state, count)                                                       \
    if(argc < count)                                                                \
    {                                                                                    \
        lit_vm_raiseerror(state->vm, "expected minimum %i argument, got %i", count, argc); \
        return lit_value_makenull(state);                                                               \
    }

#define LIT_ENSURE_MAX_ARGS(state, count)                                                       \
    if(argc > count)                                                                \
    {                                                                                    \
        lit_vm_raiseerror(state->vm, "expected maximum %i argument, got %i", count, argc); \
        return lit_value_makenull(state);                                                               \
    }


#if !defined(LIT_DISABLE_COLOR) && !defined(LIT_ENABLE_COLOR) && !(defined(LIT_OS_WINDOWS) || defined(EMSCRIPTEN))
    #define LIT_ENABLE_COLOR
#endif

#ifdef LIT_ENABLE_COLOR
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
#define RETURN_OK(r) return (LitInterpretResult){ LITRESULT_OK, r };

#define RETURN_RUNTIME_ERROR() return (LitInterpretResult){ LITRESULT_RUNTIME_ERROR, NULL_VALUE };

#define INTERPRET_RUNTIME_FAIL ((LitInterpretResult){ LITRESULT_INVALID, NULL_VALUE })



#define NULL_VALUE ((LitValue){.type=LITVAL_NULL})
#define TRUE_VALUE ((LitValue){.type=LITVAL_BOOL, .boolval=true})
#define FALSE_VALUE ((LitValue){.type=LITVAL_BOOL, .boolval=false})

enum LitObjType
{
    LITTYPE_UNDEFINED,
    LITTYPE_NULL,
    LITTYPE_STRING,
    LITTYPE_FUNCTION,
    LITTYPE_NATIVE_FUNCTION,
    LITTYPE_NATIVE_PRIMITIVE,
    LITTYPE_NATIVE_METHOD,
    LITTYPE_PRIMITIVE_METHOD,
    LITTYPE_FIBER,
    LITTYPE_MODULE,
    LITTYPE_CLOSURE,
    LITTYPE_UPVALUE,
    LITTYPE_CLASS,
    LITTYPE_INSTANCE,
    LITTYPE_BOUND_METHOD,
    LITTYPE_ARRAY,
    LITTYPE_MAP,
    LITTYPE_USERDATA,
    LITTYPE_RANGE,
    LITTYPE_FIELD,
    LITTYPE_REFERENCE,
    LITTYPE_NUMBER,
    LITTYPE_BOOL,
};

enum LitValType
{
    LITVAL_NULL,
    LITVAL_BOOL,
    LITVAL_NUMBER,
    LITVAL_OBJECT,
};

enum LitResult
{
    LITRESULT_OK,
    LITRESULT_COMPILE_ERROR,
    LITRESULT_RUNTIME_ERROR,
    LITRESULT_INVALID
};

enum LitErrType
{
    COMPILE_ERROR,
    RUNTIME_ERROR
};

enum LitOptLevel
{
    LITOPTLEVEL_NONE,
    LITOPTLEVEL_REPL,
    LITOPTLEVEL_DEBUG,
    LITOPTLEVEL_RELEASE,
    LITOPTLEVEL_EXTREME,

    LITOPTLEVEL_TOTAL
};

enum LitOptimization
{
    LITOPTSTATE_CONSTANT_FOLDING,
    LITOPTSTATE_LITERAL_FOLDING,
    LITOPTSTATE_UNUSED_VAR,
    LITOPTSTATE_UNREACHABLE_CODE,
    LITOPTSTATE_EMPTY_BODY,
    LITOPTSTATE_LINE_INFO,
    LITOPTSTATE_PRIVATE_NAMES,
    LITOPTSTATE_C_FOR,

    LITOPTSTATE_TOTAL
};


typedef enum /**/ LitValType LitValType;
typedef enum /**/ LitObjType LitObjType;
typedef struct /**/ LitObject LitObject;
typedef struct /**/ LitValue LitValue;

typedef enum /**/LitOpCode LitOpCode;
typedef enum /**/LitExprType LitExprType;
typedef enum /**/LitOptLevel LitOptLevel;
typedef enum /**/LitOptimization LitOptimization;
typedef enum /**/LitError LitError;
typedef enum /**/LitPrecedence LitPrecedence;
typedef enum /**/LitTokType LitTokType;
typedef enum /**/LitResult LitResult;
typedef enum /**/LitErrType LitErrType;
typedef enum /**/LitFuncType LitFuncType;
typedef struct /**/LitScanner LitScanner;
typedef struct /**/LitPreprocessor LitPreprocessor;
typedef struct /**/LitExecState LitExecState;
typedef struct /**/LitVM LitVM;
typedef struct /**/LitParser LitParser;
typedef struct /**/LitEmitter LitEmitter;
typedef struct /**/LitOptimizer LitOptimizer;
typedef struct /**/LitState LitState;
typedef struct /**/LitInterpretResult LitInterpretResult;
typedef struct /**/LitMap LitMap;
typedef struct /**/LitNumber LitNumber;
typedef struct /**/LitString LitString;
typedef struct /**/LitModule LitModule;
typedef struct /**/LitFiber LitFiber;
typedef struct /**/LitUserdata LitUserdata;
typedef struct /**/LitChunk LitChunk;
typedef struct /**/LitTableEntry LitTableEntry;
typedef struct /**/LitTable LitTable;
typedef struct /**/LitFunction LitFunction;
typedef struct /**/LitUpvalue LitUpvalue;
typedef struct /**/LitClosure LitClosure;
typedef struct /**/LitNativeFunction LitNativeFunction;
typedef struct /**/LitNativePrimFunction LitNativePrimFunction;
typedef struct /**/LitNativeMethod LitNativeMethod;
typedef struct /**/LitPrimitiveMethod LitPrimitiveMethod;
typedef struct /**/LitCallFrame LitCallFrame;
typedef struct /**/LitClass LitClass;
typedef struct /**/LitInstance LitInstance;
typedef struct /**/LitBoundMethod LitBoundMethod;
typedef struct /**/LitArray LitArray;
typedef struct /**/LitRange LitRange;
typedef struct /**/LitField LitField;
typedef struct /**/LitReference LitReference;
typedef struct /**/LitToken LitToken;
typedef struct /**/LitAstExpression LitAstExpression;
typedef struct /**/LitCompilerUpvalue LitCompilerUpvalue;
typedef struct /**/LitCompiler LitCompiler;
typedef struct /**/LitParseRule LitParseRule;
typedef struct /**/LitEmulatedFile LitEmulatedFile;
typedef struct /**/LitVariable LitVariable;
typedef struct /**/LitWriter LitWriter;
typedef struct /**/LitLocal LitLocal;
typedef struct /**/LitConfig LitConfig;

/* ARRAYTYPES */
typedef struct /**/LitVarList LitVarList;
typedef struct /**/LitUintList LitUintList;
typedef struct /**/LitValList LitValList;
typedef struct /**/LitAstExprList LitAstExprList;
typedef struct /**/LitAstParamList LitAstParamList;
typedef struct /**/LitPrivList LitPrivList;
typedef struct /**/LitLocList LitLocList;
typedef struct /**/LitDataList LitDataList;
typedef struct /**/LitByteList LitByteList;

/* ast/compiler types */
typedef struct /**/LitAstLiteralExpr LitAstLiteralExpr;
typedef struct /**/LitAstBinaryExpr LitAstBinaryExpr;
typedef struct /**/LitAstUnaryExpr LitAstUnaryExpr;
typedef struct /**/LitAstVarExpr LitAstVarExpr;
typedef struct /**/LitAstAssignExpr LitAstAssignExpr;
typedef struct /**/LitAstCallExpr LitAstCallExpr;
typedef struct /**/LitAstGetExpr LitAstGetExpr;
typedef struct /**/LitAstSetExpr LitAstSetExpr;
typedef struct /**/LitAstParameter LitAstParameter;
typedef struct /**/LitAstLambdaExpr LitAstLambdaExpr;
typedef struct /**/LitAstArrayExpr LitAstArrayExpr;
typedef struct /**/LitAstObjectExpr LitAstObjectExpr;
typedef struct /**/LitAstIndexExpr LitAstIndexExpr;
typedef struct /**/LitAstThisExpr LitAstThisExpr;
typedef struct /**/LitAstSuperExpr LitAstSuperExpr;
typedef struct /**/LitAstRangeExpr LitAstRangeExpr;
typedef struct /**/LitAstTernaryExpr LitAstTernaryExpr;
typedef struct /**/LitAstStrInterExpr LitAstStrInterExpr;
typedef struct /**/LitAstRefExpr LitAstRefExpr;
typedef struct /**/LitAstExprExpr LitAstExprExpr;
typedef struct /**/LitAstBlockExpr LitAstBlockExpr;
typedef struct /**/LitAstAssignVarExpr LitAstAssignVarExpr;
typedef struct /**/LitAstIfExpr LitAstIfExpr;
typedef struct /**/LitAstWhileExpr LitAstWhileExpr;
typedef struct /**/LitAstForExpr LitAstForExpr;
typedef struct /**/LitAstContinueExpr LitAstContinueExpr;
typedef struct /**/LitAstBreakExpr LitAstBreakExpr;
typedef struct /**/LitAstFunctionExpr LitAstFunctionExpr;
typedef struct /**/LitAstReturnExpr LitAstReturnExpr;
typedef struct /**/LitAstMethodExpr LitAstMethodExpr;
typedef struct /**/LitAstClassExpr LitAstClassExpr;
typedef struct /**/LitAstFieldExpr LitAstFieldExpr;
typedef struct /**/LitPrivate LitPrivate;

/* forward decls to make prot.inc work */
typedef struct /**/LitDirReader LitDirReader;
typedef struct /**/LitDirItem LitDirItem;

typedef LitAstExpression* (*LitPrefixParseFn)(LitParser*, bool);
typedef LitAstExpression* (*LitInfixParseFn)(LitParser*, LitAstExpression*, bool);


typedef LitValue (*LitNativeFunctionFn)(LitVM*, size_t, LitValue*);
typedef bool (*LitNativePrimitiveFn)(LitVM*, size_t, LitValue*);
typedef LitValue (*LitNativeMethodFn)(LitVM*, LitValue, size_t arg_count, LitValue*);
typedef bool (*LitPrimitiveMethodFn)(LitVM*, LitValue, size_t, LitValue*);
typedef LitValue (*LitMapIndexFn)(LitVM*, LitMap*, LitString*, LitValue*);
typedef void (*LitCleanupFn)(LitState*, LitUserdata*, bool mark);
typedef void (*LitErrorFn)(LitState*, const char*);
typedef void (*LitPrintFn)(LitState*, const char*);

typedef void(*LitWriterByteFN)(LitWriter*, int);
typedef void(*LitWriterStringFN)(LitWriter*, const char*, size_t);
typedef void(*LitWriterFormatFN)(LitWriter*, const char*, va_list);

struct LitObject
{
    /* the type of this object */
    LitObjType type;
    LitObject* next;
    bool marked;
    bool mustfree;
};

struct LitValue
{
    LitValType type;
    bool isfixednumber;
    struct
    {
        bool boolval;
        int64_t numfixedval;
        double numfloatval;
        LitObject* obj;
    };
};

struct LitValList
{
    size_t capacity;
    size_t count;
    LitValue* values;
};

struct LitDataList
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

struct LitVarList
{
    size_t capacity;
    size_t count;
    LitVariable* values;
};

struct LitUintList
{
    LitDataList list;
};


/* TODO: using DataList messes with the string its supposed to collect. no clue why, though. */
struct LitByteList
{
    size_t capacity;
    size_t count;
    uint8_t* values;
};

struct LitChunk
{
    /* how many items this chunk holds */
    size_t count;
    size_t capacity;
    uint8_t* code;
    bool has_line_info;
    size_t line_count;
    size_t line_capacity;
    uint16_t* lines;
    LitValList constants;
};

struct LitWriter
{
    LitState* state;

    /* the main pointer, that either holds a pointer to a LitString, or a FILE */
    void* uptr;

    /* if true, then uptr is a LitString, otherwise it's a FILE */
    bool stringmode;

    /* if true, and !stringmode, then calls fflush() after each i/o operations */
    bool forceflush;

    /* the callback that emits a single character */
    LitWriterByteFN fnbyte;

    /* the callback that emits a string */
    LitWriterStringFN fnstring;

    /* the callback that emits a format string (printf-style) */
    LitWriterFormatFN fnformat;
};


struct LitTableEntry
{
    /* the key of this entry. can be NULL! */
    LitString* key;

    /* the associated value */
    LitValue value;
};

struct LitTable
{
    LitState* state;

    /* how many entries are in this table */
    int count;

    /* how many entries could be held */
    int capacity;

    /* the actual entries */
    LitTableEntry* entries;
};

struct LitNumber
{
    LitObject object;
    double num;
};

struct LitString
{
    LitObject object;
    /* the hash of this string - note that it is only unique to the context! */
    uint32_t hash;
    /* this is handled by sds - use lit_string_getlength to get the length! */
    char* chars;
};

struct LitFunction
{
    LitObject object;
    LitChunk chunk;
    LitString* name;
    uint8_t arg_count;
    uint16_t upvalue_count;
    size_t max_slots;
    bool vararg;
    LitModule* module;
};

struct LitUpvalue
{
    LitObject object;
    LitValue* location;
    LitValue closed;
    LitUpvalue* next;
};

struct LitClosure
{
    LitObject object;
    LitFunction* function;
    LitUpvalue** upvalues;
    size_t upvalue_count;
};

struct LitNativeFunction
{
    LitObject object;
    /* the native callback for this function */
    LitNativeFunctionFn function;
    /* the name of this function */
    LitString* name;
};

struct LitNativePrimFunction
{
    LitObject object;
    LitNativePrimitiveFn function;
    LitString* name;
};

struct LitNativeMethod
{
    LitObject object;
    LitNativeMethodFn method;
    LitString* name;
};

struct LitPrimitiveMethod
{
    LitObject object;
    LitPrimitiveMethodFn method;
    LitString* name;
};

struct LitCallFrame
{
    LitFunction* function;
    LitClosure* closure;
    uint8_t* ip;
    LitValue* slots;
    bool result_ignored;
    bool return_to_c;
};

struct LitMap
{
    LitObject object;
    /* the table that holds the actual entries */
    LitTable values;
    /* the index function corresponding to operator[] */
    LitMapIndexFn index_fn;
};

struct LitModule
{
    LitObject object;
    LitValue return_value;
    LitString* name;
    LitValue* privates;
    LitMap* private_names;
    size_t private_count;
    LitFunction* main_function;
    LitFiber* main_fiber;
    bool ran;
};

struct LitFiber
{
    LitObject object;
    LitFiber* parent;
    LitValue* stack;
    LitValue* stack_top;
    size_t stack_capacity;
    LitCallFrame* frames;
    size_t frame_capacity;
    size_t frame_count;
    size_t arg_count;
    LitUpvalue* open_upvalues;
    LitModule* module;
    LitValue lit_emitter_raiseerror;
    bool abort;
    bool catcher;
};

struct LitClass
{
    LitObject object;
    /* the name of this class */
    LitString* name;
    /* the constructor object */
    LitObject* init_method;
    /* runtime methods */
    LitTable methods;
    /* static fields, which include functions, and variables */
    LitTable static_fields;
    /*
    * the parent class - the topmost is always LitClass, followed by LitObject.
    * that is, eg for LitString: LitString <- LitObject <- LitClass
    */
    LitClass* super;
};

struct LitInstance
{
    LitObject object;
    /* the class that corresponds to this instance */
    LitClass* klass;
    LitTable fields;
};

struct LitBoundMethod
{
    LitObject object;
    LitValue receiver;
    LitValue method;
};

struct LitArray
{
    LitObject object;
    LitValList list;
};

struct LitUserdata
{
    LitObject object;
    void* data;
    size_t size;
    LitCleanupFn cleanup_fn;
    bool canfree;
};

struct LitRange
{
    LitObject object;
    double from;
    double to;
};

struct LitField
{
    LitObject object;
    LitObject* getter;
    LitObject* setter;
};

struct LitReference
{
    LitObject object;
    LitValue* slot;
};

struct LitInterpretResult
{
    /* the result of this interpret/lit_vm_callcallable attempt */
    LitResult type;
    /* the value returned from this interpret/lit_vm_callcallable attempt */
    LitValue result;
};

struct LitConfig
{
    bool dumpbytecode;
    bool dumpast;
    bool runafterdump;
};

struct LitState
{
    LitConfig config;
    LitWriter stdoutwriter;
    /* how much was allocated in total? */
    int64_t bytes_allocated;
    int64_t next_gc;
    bool allow_gc;
    LitValList lightobjects;
    LitErrorFn error_fn;
    LitPrintFn print_fn;
    LitValue* roots;
    size_t root_count;
    size_t root_capacity;
    LitPreprocessor* preprocessor;
    LitScanner* scanner;
    LitParser* parser;
    LitEmitter* emitter;
    LitOptimizer* optimizer;
    /*
    * recursive pointer to the current VM instance.
    * using 'state->vm->state' will in turn mean this instance, etc.
    */
    LitVM* vm;
    bool had_error;
    LitFunction* api_function;
    LitFiber* api_fiber;
    LitString* api_name;
    /* when using debug routines, this is the writer that output is called on */
    LitWriter debugwriter;
    // class class
    // Mental note:
    // When adding another class here, DO NOT forget to mark it in lit_mem.c or it will be GC-ed
    LitClass* classvalue_class;
    LitClass* objectvalue_class;
    LitClass* numbervalue_class;
    LitClass* stringvalue_class;
    LitClass* boolvalue_class;
    LitClass* functionvalue_class;
    LitClass* fibervalue_class;
    LitClass* modulevalue_class;
    LitClass* arrayvalue_class;
    LitClass* mapvalue_class;
    LitClass* rangevalue_class;
    LitModule* last_module;
};

struct LitVM
{
    /* the current state */
    LitState* state;
    /* currently held objects */
    LitObject* objects;
    /* currently cached strings */
    LitTable strings;
    /* currently loaded/defined modules */
    LitMap* modules;
    /* currently defined globals */
    LitMap* globals;
    LitFiber* fiber;
    // For garbage collection
    size_t gray_count;
    size_t gray_capacity;
    LitObject** gray_stack;
};


#include "prot.inc"

#if 0

    #define lit_value_asnumber(v) \
        ( ((v).isfixednumber) ? ((v).numfixedval) : ((v).numfloatval) )

#else
    #define lit_value_asnumber(v) (v).numfloatval
#endif

#define lit_value_makeobject(obj) lit_value_makeobject_actual((LitObject*)obj)

#define lit_value_istype(value, t) \
    (lit_value_isobject(value) && (lit_value_asobject(value) != NULL) && (lit_value_asobject(value)->type == t))


#define lit_value_isstring(value) \
    lit_value_istype(value, LITTYPE_STRING)

#define lit_value_isfunction(value) \
    lit_value_istype(value, LITTYPE_FUNCTION)

#define lit_value_isnatfunction(value) \
    lit_value_istype(value, LITTYPE_NATIVE_FUNCTION)

#define lit_value_isnatprim(value) \
    lit_value_istype(value, LITTYPE_NATIVE_PRIMITIVE)

#define lit_value_isnatmethod(value) \
    lit_value_istype(value, LITTYPE_NATIVE_METHOD)

#define lit_value_isprimmethod(value) \
    lit_value_istype(value, LITTYPE_PRIMITIVE_METHOD)

#define lit_value_ismodule(value) \
    lit_value_istype(value, LITTYPE_MODULE)

#define lit_value_isclosure(value) \
    lit_value_istype(value, LITTYPE_CLOSURE)

#define lit_value_isupvalue(value) \
    lit_value_istype(value, LITTYPE_UPVALUE)

#define lit_value_isclass(value) \
    lit_value_istype(value, LITTYPE_CLASS)

#define lit_value_isinstance(value) \
    lit_value_istype(value, LITTYPE_INSTANCE)

#define lit_value_isarray(value) \
    lit_value_istype(value, LITTYPE_ARRAY)

static inline bool lit_value_ismap(LitValue value)
{
    return lit_value_istype(value, LITTYPE_MAP);
}

static inline bool lit_value_isboundmethod(LitValue value)
{
    return lit_value_istype(value, LITTYPE_BOUND_METHOD);
}

static inline bool lit_value_isuserdata(LitValue value)
{
    return lit_value_istype(value, LITTYPE_USERDATA);
}

static inline bool lit_value_isrange(LitValue value)
{
    return lit_value_istype(value, LITTYPE_RANGE);
}

static inline bool lit_value_isfield(LitValue value)
{
    return lit_value_istype(value, LITTYPE_FIELD);
}

static inline bool lit_value_isreference(LitValue value)
{
    return lit_value_istype(value, LITTYPE_REFERENCE);
}


static inline LitString* lit_value_asstring(LitValue v)
{
    return (LitString*)lit_value_asobject(v);
}

static inline char* lit_value_ascstring(LitValue v)
{
    return (lit_value_asstring(v)->chars);
}

static inline LitFunction* lit_value_asfunction(LitValue v)
{
    return (LitFunction*)lit_value_asobject(v);
}

static inline LitNativeFunction* lit_value_asnativefunction(LitValue v)
{
    return (LitNativeFunction*)lit_value_asobject(v);
}

static inline LitNativePrimFunction* lit_value_asnativeprimitive(LitValue v)
{
    return (LitNativePrimFunction*)lit_value_asobject(v);
}

static inline LitNativeMethod* lit_value_asnativemethod(LitValue v)
{
    return (LitNativeMethod*)lit_value_asobject(v);
}

static inline LitPrimitiveMethod* lit_value_asprimitivemethod(LitValue v)
{
    return (LitPrimitiveMethod*)lit_value_asobject(v);
}

static inline LitModule* lit_value_asmodule(LitValue v)
{
    return (LitModule*)lit_value_asobject(v);
}

static inline LitClosure* lit_value_asclosure(LitValue v)
{
    return (LitClosure*)lit_value_asobject(v);
}

static inline LitUpvalue* lit_value_asupvalue(LitValue v)
{
    return (LitUpvalue*)lit_value_asobject(v);
}

static inline LitClass* lit_value_asclass(LitValue v)
{
    return (LitClass*)lit_value_asobject(v);
}

static inline LitInstance* lit_value_asinstance(LitValue v)
{
    return (LitInstance*)lit_value_asobject(v);
}

static inline LitArray* lit_value_asarray(LitValue v)
{
    return (LitArray*)lit_value_asobject(v);
}

static inline LitMap* lit_value_asmap(LitValue v)
{
    return (LitMap*)lit_value_asobject(v);
}

static inline LitBoundMethod* lit_value_asboundmethod(LitValue v)
{
    return (LitBoundMethod*)lit_value_asobject(v);
}

static inline LitUserdata* lit_value_asuserdata(LitValue v)
{
    return (LitUserdata*)lit_value_asobject(v);
}

static inline LitRange* lit_value_asrange(LitValue v)
{
    return (LitRange*)lit_value_asobject(v);
}

static inline LitField* lit_value_asfield(LitValue v)
{
    return (LitField*)lit_value_asobject(v);
}

static inline LitFiber* lit_value_asfiber(LitValue v)
{
    return (LitFiber*)lit_value_asobject(v);
}

static inline LitReference* lit_value_asreference(LitValue v)
{
    return (LitReference*)lit_value_asobject(v);
}

static inline LitValue lit_value_makefalse(LitState* state)
{
    (void)state;
    return FALSE_VALUE;
}

static inline LitValue lit_value_maketrue(LitState* state)
{
    (void)state;
    return TRUE_VALUE;
}

static inline LitValue lit_value_makenull(LitState* state)
{
    (void)state;
    return NULL_VALUE;
}


static inline LitValue lit_value_makestring(LitState* state, const char* text)
{
    return lit_value_makeobject(lit_string_copy((state), (text), strlen(text)));
}

static inline LitString* lit_string_copyconst(LitState* state, const char* text)
{
    return lit_string_copy(state, text, strlen(text));
}
