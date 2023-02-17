
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

#define LIT_GROW_ARRAY(state, previous, typesz, oldcount, count) \
    lit_gcmem_memrealloc(state, previous, typesz * (oldcount), typesz * (count))

#define LIT_FREE_ARRAY(state, typesz, pointer, oldcount) \
    lit_gcmem_memrealloc(state, pointer, typesz * (oldcount), 0)

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
    union
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
    LitValue errorval;
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


/*
#define lit_value_asnumber(v) \
    ( (lit_value_isnumber(v) ? (\
            ((v).isfixednumber) ? \
                ((v).numfixedval) : \
                ((v).numfloatval) \
            ) : ( \
                lit_value_isnull(v) ? 0 : ( \
                    lit_value_isbool(v) ? lit_value_asbool(v) : 0 \
                )\
            )))

*/
#define lit_value_asnumber(v) \
        ( \
            ((v).isfixednumber) ? ((v).numfixedval) : ((v).numfloatval) )


#define lit_value_fromobject(obj) lit_value_fromobject_actual((LitObject*)obj)

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

/* libobject.c */
LitUpvalue *lit_object_makeupvalue(LitState *state, LitValue *slot);
LitModule *lit_object_makemodule(LitState *state, LitString *name);
LitUserdata *lit_object_makeuserdata(LitState *state, size_t size, bool ispointeronly);
LitRange *lit_object_makerange(LitState *state, double from, double to);
LitReference *lit_object_makereference(LitState *state, LitValue *slot);
void lit_object_destroy(LitState *state, LitObject *object);
void lit_object_destroylistof(LitState *state, LitObject *objects);
LitValue lit_function_getname(LitVM *vm, LitValue instance);
void lit_state_openobjectlibrary(LitState *state);
/* ccast.c */
void lit_exprlist_init(LitAstExprList *array);
void lit_exprlist_destroy(LitState *state, LitAstExprList *array);
void lit_exprlist_push(LitState *state, LitAstExprList *array, LitAstExpression *value);
void lit_paramlist_init(LitAstParamList *array);
void lit_paramlist_destroy(LitState *state, LitAstParamList *array);
void lit_paramlist_push(LitState *state, LitAstParamList *array, LitAstParameter value);
void lit_paramlist_destroyvalues(LitState *state, LitAstParamList *parameters);
void lit_ast_destroyexprlist(LitState *state, LitAstExprList *expressions);
void lit_ast_destroystmtlist(LitState *state, LitAstExprList *statements);
void lit_ast_destroyexpression(LitState *state, LitAstExpression *expression);
LitAstLiteralExpr *lit_ast_make_literalexpr(LitState *state, size_t line, LitValue value);
LitAstBinaryExpr *lit_ast_make_binaryexpr(LitState *state, size_t line, LitAstExpression *left, LitAstExpression *right, LitTokType op);
LitAstUnaryExpr *lit_ast_make_unaryexpr(LitState *state, size_t line, LitAstExpression *right, LitTokType op);
LitAstVarExpr *lit_ast_make_varexpr(LitState *state, size_t line, const char *name, size_t length);
LitAstAssignExpr *lit_ast_make_assignexpr(LitState *state, size_t line, LitAstExpression *to, LitAstExpression *value);
LitAstCallExpr *lit_ast_make_callexpr(LitState *state, size_t line, LitAstExpression *callee);
LitAstGetExpr *lit_ast_make_getexpr(LitState *state, size_t line, LitAstExpression *where, const char *name, size_t length, bool questionable, bool ignore_result);
LitAstSetExpr *lit_ast_make_setexpr(LitState *state, size_t line, LitAstExpression *where, const char *name, size_t length, LitAstExpression *value);
LitAstLambdaExpr *lit_ast_make_lambdaexpr(LitState *state, size_t line);
LitAstArrayExpr *lit_ast_make_arrayexpr(LitState *state, size_t line);
LitAstObjectExpr *lit_ast_make_objectexpr(LitState *state, size_t line);
LitAstIndexExpr *lit_ast_make_subscriptexpr(LitState *state, size_t line, LitAstExpression *array, LitAstExpression *index);
LitAstThisExpr *lit_ast_make_thisexpr(LitState *state, size_t line);
LitAstSuperExpr *lit_ast_make_superexpr(LitState *state, size_t line, LitString *method, bool ignore_result);
LitAstRangeExpr *lit_ast_make_rangeexpr(LitState *state, size_t line, LitAstExpression *from, LitAstExpression *to);
LitAstTernaryExpr *lit_ast_make_ternaryexpr(LitState *state, size_t line, LitAstExpression *condition, LitAstExpression *if_branch, LitAstExpression *else_branch);
LitAstStrInterExpr *lit_ast_make_strinterpolexpr(LitState *state, size_t line);
LitAstRefExpr *lit_ast_make_referenceexpr(LitState *state, size_t line, LitAstExpression *to);
LitAstExprExpr *lit_ast_make_exprstmt(LitState *state, size_t line, LitAstExpression *expression);
LitAstBlockExpr *lit_ast_make_blockexpr(LitState *state, size_t line);
LitAstAssignVarExpr *lit_ast_make_assignvarexpr(LitState *state, size_t line, const char *name, size_t length, LitAstExpression *init, bool constant);
LitAstIfExpr *lit_ast_make_ifexpr(LitState *state, size_t line, LitAstExpression *condition, LitAstExpression *if_branch, LitAstExpression *else_branch, LitAstExprList *elseif_conditions, LitAstExprList *elseif_branches);
LitAstWhileExpr *lit_ast_make_whileexpr(LitState *state, size_t line, LitAstExpression *condition, LitAstExpression *body);
LitAstForExpr *lit_ast_make_forexpr(LitState *state, size_t line, LitAstExpression *init, LitAstExpression *var, LitAstExpression *condition, LitAstExpression *increment, LitAstExpression *body, bool c_style);
LitAstContinueExpr *lit_ast_make_continueexpr(LitState *state, size_t line);
LitAstBreakExpr *lit_ast_make_breakexpr(LitState *state, size_t line);
LitAstFunctionExpr *lit_ast_make_funcexpr(LitState *state, size_t line, const char *name, size_t length);
LitAstReturnExpr *lit_ast_make_returnexpr(LitState *state, size_t line, LitAstExpression *expression);
LitAstMethodExpr *lit_ast_make_methodexpr(LitState *state, size_t line, LitString *name, bool is_static);
LitAstClassExpr *lit_ast_make_classexpr(LitState *state, size_t line, LitString *name, LitString *parent);
LitAstFieldExpr *lit_ast_make_fieldexpr(LitState *state, size_t line, LitString *name, LitAstExpression *getter, LitAstExpression *setter, bool is_static);
LitAstExprList *lit_ast_allocexprlist(LitState *state);
void lit_ast_destroy_allocdexprlist(LitState *state, LitAstExprList *expressions);
LitAstExprList *lit_ast_allocate_stmtlist(LitState *state);
void lit_ast_destry_allocdstmtlist(LitState *state, LitAstExprList *statements);
/* librange.c */
void lit_open_range_library(LitState *state);
/* ccemit.c */
void lit_privlist_init(LitPrivList *array);
void lit_privlist_destroy(LitState *state, LitPrivList *array);
void lit_privlist_push(LitState *state, LitPrivList *array, LitPrivate value);
void lit_loclist_init(LitLocList *array);
void lit_loclist_destroy(LitState *state, LitLocList *array);
void lit_loclist_push(LitState *state, LitLocList *array, LitLocal value);
void lit_emitter_init(LitState *state, LitEmitter *emitter);
void lit_emitter_destroy(LitEmitter *emitter);
LitModule *lit_emitter_modemit(LitEmitter *emitter, LitAstExprList *statements, LitString *module_name);
/* vm.c */
uint16_t lit_vmexec_readshort(LitExecState *est);
uint8_t lit_vmexec_readbyte(LitExecState *est);
LitValue lit_vmexec_readconstant(LitExecState *est);
LitValue lit_vmexec_readconstantlong(LitExecState *est);
LitString *lit_vmexec_readstring(LitExecState *est);
LitString *lit_vmexec_readstringlong(LitExecState *est);
void lit_vmexec_push(LitFiber *fiber, LitValue v);
LitValue lit_vmexec_pop(LitFiber *fiber);
void lit_vmexec_drop(LitFiber *fiber);
void lit_vmexec_dropn(LitFiber *fiber, int amount);
LitValue lit_vmexec_peek(LitFiber *fiber, short distance);
void lit_vmexec_readframe(LitFiber *fiber, LitExecState *est);
void lit_vmexec_writeframe(LitExecState *est, uint8_t *ip);
void lit_vmexec_resetstack(LitVM *vm);
void lit_vmexec_resetvm(LitState *state, LitVM *vm);
void lit_vm_init(LitState *state, LitVM *vm);
void lit_vm_destroy(LitVM *vm);
void lit_vm_tracestack(LitVM *vm, LitWriter *wr);
bool lit_vm_handleruntimeerror(LitVM *vm, LitString *error_string);
bool lit_vm_vraiseerror(LitVM *vm, const char *format, va_list args);
bool lit_vm_raiseerror(LitVM *vm, const char *format, ...);
bool lit_vm_raiseexitingerror(LitVM *vm, const char *format, ...);
bool lit_vm_callcallable(LitVM *vm, LitFunction *function, LitClosure *closure, uint8_t argc);
const char *lit_vmexec_funcnamefromvalue(LitVM *vm, LitExecState *est, LitValue v);
bool lit_vm_callvalue(LitVM *vm, LitFiber *fiber, LitExecState *est, LitValue callee, uint8_t argc);
LitUpvalue *lit_execvm_captureupvalue(LitState *state, LitValue *local);
void lit_vm_closeupvalues(LitVM *vm, const LitValue *last);
LitInterpretResult lit_vm_execmodule(LitState *state, LitModule *module);
LitInterpretResult lit_vm_execfiber(LitState *state, LitFiber *fiber);
void lit_vmutil_callexitjump(void);
bool lit_vmutil_setexitjump(void);
/* chunk.c */
void lit_chunk_init(LitChunk *chunk);
void lit_chunk_destroy(LitState *state, LitChunk *chunk);
void lit_chunk_push(LitState *state, LitChunk *chunk, uint8_t byte, uint16_t line);
size_t lit_chunk_addconst(LitState *state, LitChunk *chunk, LitValue constant);
size_t lit_chunk_getline(LitChunk *chunk, size_t offset);
void lit_chunk_shrink(LitState *state, LitChunk *chunk);
void lit_chunk_emitbyte(LitState *state, LitChunk *chunk, uint8_t byte);
void lit_chunk_emit2bytes(LitState *state, LitChunk *chunk, uint8_t a, uint8_t b);
void lit_chunk_emitshort(LitState *state, LitChunk *chunk, uint16_t value);
/* libclass.c */
LitClass *lit_create_class(LitState *state, LitString *name);
LitClass *lit_create_classobject(LitState *state, const char *name);
LitField *lit_create_field(LitState *state, LitObject *getter, LitObject *setter);
LitInstance *lit_create_instance(LitState *state, LitClass *klass);
void lit_class_bindconstructor(LitState *state, LitClass *cl, LitNativeMethodFn fn);
LitNativeMethod *lit_class_bindmethod(LitState *state, LitClass *cl, const char *name, LitNativeMethodFn fn);
LitPrimitiveMethod *lit_class_bindprimitive(LitState *state, LitClass *cl, const char *name, LitPrimitiveMethodFn fn);
LitNativeMethod *lit_class_bindstaticmethod(LitState *state, LitClass *cl, const char *name, LitNativeMethodFn fn);
LitPrimitiveMethod *lit_class_bindstaticprimitive(LitState *state, LitClass *cl, const char *name, LitPrimitiveMethodFn fn);
void lit_class_setstaticfield(LitState *state, LitClass *cl, const char *name, LitValue val);
LitField *lit_class_bindgetset(LitState *state, LitClass *cl, const char *name, LitNativeMethodFn getfn, LitNativeMethodFn setfn, bool isstatic);
void lit_class_inheritfrom(LitState *state, LitClass *current, LitClass *other);
void lit_open_class_library(LitState *state);
/* libfiber.c */
LitFiber *lit_create_fiber(LitState *state, LitModule *module, LitFunction *function);
void lit_ensure_fiber_stack(LitState *state, LitFiber *fiber, size_t needed);
void lit_open_fiber_library(LitState *state);
/* libfs.c */
bool lit_fs_diropen(LitDirReader *rd, const char *path);
bool lit_fs_dirread(LitDirReader *rd, LitDirItem *itm);
bool lit_fs_dirclose(LitDirReader *rd);
char *lit_util_readfile(const char *path, size_t *dlen);
bool lit_fs_fileexists(const char *path);
bool lit_fs_direxists(const char *path);
size_t lit_ioutil_writeuint8(FILE *file, uint8_t byte);
size_t lit_ioutil_writeuint16(FILE *file, uint16_t byte);
size_t lit_ioutil_writeuint32(FILE *file, uint32_t byte);
size_t lit_ioutil_writedouble(FILE *file, double byte);
size_t lit_ioutil_writestring(FILE *file, LitString *string);
uint8_t lit_ioutil_readuint8(FILE *file);
uint16_t lit_ioutil_readuint16(FILE *file);
uint32_t lit_ioutil_readuint32(FILE *file);
double lit_ioutil_readdouble(FILE *file);
LitString *lit_ioutil_readstring(LitState *state, FILE *file);
void lit_emufile_init(LitEmulatedFile *file, const char *source, size_t len);
uint8_t lit_emufile_readuint8(LitEmulatedFile *file);
uint16_t lit_emufile_readuint16(LitEmulatedFile *file);
uint32_t lit_emufile_readuint32(LitEmulatedFile *file);
double lit_emufile_readdouble(LitEmulatedFile *file);
LitString *lit_emufile_readstring(LitState *state, LitEmulatedFile *file);
void lit_ioutil_writemodule(LitModule *module, FILE *file);
LitModule *lit_ioutil_readmodule(LitState *state, const char *input, size_t len);
void lit_userfile_cleanup(LitState *state, LitUserdata *data, bool mark);
void lit_open_file_library(LitState *state);
/* ccparser.c */
const char *lit_parser_token2name(int t);
void lit_parser_init(LitState *state, LitParser *parser);
void lit_parser_destroy(LitParser *parser);
bool lit_parser_parsesource(LitParser *parser, const char *file_name, const char *source, LitAstExprList *statements);
/* util.c */
uint64_t pack754(long double f, unsigned bits, unsigned expbits);
long double unpack754(uint64_t i, unsigned bits, unsigned expbits);
double lit_util_uinttofloat(unsigned int val);
unsigned int lit_util_floattouint(double val);
int lit_util_doubletoint(double n);
int lit_util_numbertoint32(double n);
unsigned int lit_util_numbertouint32(double n);
int lit_util_closestpowof2(int n);
char *lit_util_patchfilename(char *file_name);
char *lit_util_copystring(const char *string);
/* error.c */
const char *lit_error_getformatstring(LitError e);
LitString *lit_vformat_error(LitState *state, size_t line, LitError ecode, va_list args);
LitString *lit_format_error(LitState *state, size_t line, LitError ecode, ...);
/* value.c */
LitValue lit_value_fromobject_actual(LitObject* obj);
bool lit_value_isobject(LitValue v);
LitObject *lit_value_asobject(LitValue v);
LitValue lit_value_makebool(LitState *state, bool b);
bool lit_value_isbool(LitValue v);
bool lit_value_isfalsey(LitValue v);
bool lit_value_asbool(LitValue v);
bool lit_value_isnull(LitValue v);
LitObjType lit_value_type(LitValue v);
LitValue lit_value_makefloatnumber(LitState *state, double num);
LitValue lit_value_makefixednumber(LitState *state, int64_t num);
LitValue lit_value_makenumber(LitState* state, double num);

double lit_value_asfloatnumber(LitValue v);
int64_t lit_value_asfixednumber(LitValue v);

bool lit_value_isnumber(LitValue v);
bool lit_value_compare(LitState *state, const LitValue a, const LitValue b);
LitString *lit_value_tostring(LitState *state, LitValue object);
double lit_value_checknumber(LitVM *vm, LitValue *args, uint8_t arg_count, uint8_t id);
double lit_value_getnumber(LitVM *vm, LitValue *args, uint8_t arg_count, uint8_t id, double def);
bool lit_value_checkbool(LitVM *vm, LitValue *args, uint8_t arg_count, uint8_t id);
bool lit_value_getbool(LitVM *vm, LitValue *args, uint8_t arg_count, uint8_t id, bool def);
const char *lit_value_checkstring(LitVM *vm, LitValue *args, uint8_t arg_count, uint8_t id);
const char *lit_value_getstring(LitVM *vm, LitValue *args, uint8_t arg_count, uint8_t id, const char *def);
LitString *lit_value_checkobjstring(LitVM *vm, LitValue *args, uint8_t arg_count, uint8_t id);
LitInstance *lit_value_checkinstance(LitVM *vm, LitValue *args, uint8_t arg_count, uint8_t id);
LitValue *lit_value_checkreference(LitVM *vm, LitValue *args, uint8_t arg_count, uint8_t id);
void lit_value_ensurebool(LitVM *vm, LitValue value, const char *emsg);
void lit_value_ensurestring(LitVM *vm, LitValue value, const char *emsg);
void lit_value_ensurenumber(LitVM *vm, LitValue value, const char *emsg);
void lit_value_ensureobjtype(LitVM *vm, LitValue value, LitObjType type, const char *emsg);
LitValue lit_value_callnew(LitVM *vm, const char *name, LitValue *args, size_t argc, bool ignfiber);
/* libmap.c */
void lit_table_init(LitState *state, LitTable *table);
void lit_table_destroy(LitState *state, LitTable *table);
bool lit_table_set(LitState *state, LitTable *table, LitString *key, LitValue value);
bool lit_table_get(LitTable *table, LitString *key, LitValue *value);
bool lit_table_get_slot(LitTable *table, LitString *key, LitValue **value);
bool lit_table_delete(LitTable *table, LitString *key);
LitString *lit_table_find_string(LitTable *table, const char *chars, size_t length, uint32_t hash);
void lit_table_add_all(LitState *state, LitTable *from, LitTable *to);
void lit_table_removewhite(LitTable *table);
int util_table_iterator(LitTable *table, int number);
LitValue util_table_iterator_key(LitTable *table, int index);
LitMap *lit_create_map(LitState *state);
bool lit_map_set(LitState *state, LitMap *map, LitString *key, LitValue value);
bool lit_map_get(LitMap *map, LitString *key, LitValue *value);
bool lit_map_delete(LitMap *map, LitString *key);
void lit_map_add_all(LitState *state, LitMap *from, LitMap *to);
void lit_open_map_library(LitState *state);
/* main.c */
int exitstate(LitState *state, LitResult result);
void interupt_handler(int signal_id);
int main(int argc, char *argv[]);
int oldmain(int argc, const char *argv[]);
/* gcmem.c */
LitObject *lit_gcmem_allocobject(LitState *state, size_t size, LitObjType type, bool islight);
void *lit_gcmem_memrealloc(LitState *state, void *pointer, size_t old_size, size_t new_size);
void lit_gcmem_marktable(LitVM *vm, LitTable *table);
void lit_gcmem_markobject(LitVM *vm, LitObject *object);
void lit_gcmem_markvalue(LitVM *vm, LitValue value);
void lit_gcmem_vmmarkroots(LitVM *vm);
void lit_gcmem_markarray(LitVM *vm, LitValList *array);
void lit_gcmem_vmblackobject(LitVM *vm, LitObject *object);
void lit_gcmem_vmtracerefs(LitVM *vm);
void lit_gcmem_vmsweep(LitVM *vm);
uint64_t lit_gcmem_collectgarbage(LitVM *vm);
void lit_open_gc_library(LitState *state);
/* debug.c */
void lit_disassemble_module(LitState *state, LitModule *module, const char *source);
void lit_disassemble_chunk(LitState *state, LitChunk *chunk, const char *name, const char *source);
size_t lit_disassemble_instruction(LitState *state, LitChunk *chunk, size_t offset, const char *source);
void lit_trace_frame(LitFiber *fiber, LitWriter *wr);
/* ccscan.c */
void lit_bytelist_init(LitByteList *bl);
void lit_bytelist_destroy(LitState *state, LitByteList *bl);
void lit_bytelist_push(LitState *state, LitByteList *bl, uint8_t value);
void lit_lex_init(LitState *state, LitScanner *scanner, const char *file_name, const char *source);
LitToken lit_lex_rollback(LitScanner *scanner);
LitToken lit_lex_scantoken(LitScanner *scanner);
/* libstring.c */
char *itoa(int value, char *result, int base);
char *lit_util_inttostring(char *dest, size_t n, int x);
uint32_t lit_util_hashstring(const char *key, size_t length);
int lit_util_decodenumbytes(uint8_t byte);
int lit_ustring_length(LitString *string);
LitString *lit_ustring_codepointat(LitState *state, LitString *string, uint32_t index);
LitString *lit_ustring_fromcodepoint(LitState *state, int value);
LitString *lit_ustring_fromrange(LitState *state, LitString *source, int start, uint32_t count);
int lit_util_encodenumbytes(int value);
int lit_ustring_encode(int value, uint8_t *bytes);
int lit_ustring_decode(const uint8_t *bytes, uint32_t length);
int lit_util_ucharoffset(char *str, int index);
LitString *lit_string_makeempty(LitState *state, size_t length, bool reuse);
LitString *lit_string_makelen(LitState *state, char *chars, size_t length, uint32_t hash, bool wassds, bool reuse);
void lit_state_regstring(LitState *state, LitString *string);
LitString *lit_string_take(LitState *state, char *chars, size_t length, bool wassds);
LitString *lit_string_copy(LitState *state, const char *chars, size_t length);
const char *lit_string_getdata(LitString *ls);
size_t lit_string_getlength(LitString *ls);
void lit_string_appendlen(LitString *ls, const char *s, size_t len);
void lit_string_appendobj(LitString *ls, LitString *other);
void lit_string_appendchar(LitString *ls, char ch);
LitValue lit_string_numbertostring(LitState *state, double value);
LitValue lit_string_format(LitState *state, const char *format, ...);
bool lit_string_equal(LitState *state, LitString *a, LitString *b);
bool check_fmt_arg(LitVM *vm, char *buf, size_t ai, size_t argc, LitValue *argv, const char *fmttext);
void lit_open_string_library(LitState *state);
/* libcore.c */
void lit_open_libraries(LitState *state);
void util_custom_quick_sort(LitVM *vm, LitValue *l, int length, LitValue callee);
bool util_is_fiber_done(LitFiber *fiber);
void util_run_fiber(LitVM *vm, LitFiber *fiber, LitValue *argv, size_t argc, bool catcher);
void util_basic_quick_sort(LitState *state, LitValue *clist, int length);
bool util_interpret(LitVM *vm, LitModule *module);
bool util_test_file_exists(const char *filename);
bool util_attempt_to_require(LitVM *vm, LitValue *argv, size_t argc, const char *path, bool ignore_previous, bool folders);
bool util_attempt_to_require_combined(LitVM *vm, LitValue *argv, size_t argc, const char *a, const char *b, bool ignore_previous);
LitValue util_invalid_constructor(LitVM *vm, LitValue instance, size_t argc, LitValue *argv);
void lit_open_core_library(LitState *state);
/* state.c */
void lit_enable_compilation_time_measurement(void);
LitState *lit_make_state(void);
int64_t lit_destroy_state(LitState *state);
void lit_api_init(LitState *state);
void lit_api_destroy(LitState *state);
LitValue lit_state_getglobalvalue(LitState *state, LitString *name);
LitFunction *lit_state_getglobalfunction(LitState *state, LitString *name);
void lit_state_setglobal(LitState *state, LitString *name, LitValue value);
bool lit_state_hasglobal(LitState *state, LitString *name);
void lit_state_defnativefunc(LitState *state, const char *name, LitNativeFunctionFn native);
void lit_state_defnativeprimitive(LitState *state, const char *name, LitNativePrimitiveFn native);
LitValue lit_state_getinstancemethod(LitState *state, LitValue callee, LitString *mthname);
LitInterpretResult lit_state_callinstancemethod(LitState *state, LitValue callee, LitString *mthname, LitValue *argv, size_t argc);
LitValue lit_state_getfield(LitState *state, LitTable *table, const char *name);
LitValue lit_state_getmapfield(LitState *state, LitMap *map, const char *name);
void lit_state_setfield(LitState *state, LitTable *table, const char *name, LitValue value);
void lit_state_setmapfield(LitState *state, LitMap *map, const char *name, LitValue value);
bool lit_state_ensurefiber(LitVM *vm, LitFiber *fiber);
LitInterpretResult lit_state_callfunction(LitState *state, LitFunction *callee, LitValue *argv, uint8_t argc, bool ignfiber);
LitInterpretResult lit_state_callclosure(LitState *state, LitClosure *callee, LitValue *argv, uint8_t argc, bool ignfiber);
LitInterpretResult lit_state_callmethod(LitState *state, LitValue instance, LitValue callee, LitValue *argv, uint8_t argc, bool ignfiber);
LitInterpretResult lit_state_callvalue(LitState *state, LitValue callee, LitValue *argv, uint8_t argc, bool ignfiber);
LitInterpretResult lit_state_findandcallmethod(LitState *state, LitValue callee, LitString *method_name, LitValue *argv, uint8_t argc, bool ignfiber);
void lit_state_pushroot(LitState *state, LitObject *object);
void lit_state_pushvalueroot(LitState *state, LitValue value);
LitValue lit_state_peekroot(LitState *state, uint8_t distance);
void lit_state_poproot(LitState *state);
void lit_state_poproots(LitState *state, uint8_t amount);
LitClass *lit_state_getclassfor(LitState *state, LitValue value);
LitModule *lit_state_compilemodule(LitState *state, LitString *module_name, const char *code, size_t len);
LitModule *lit_state_getmodule(LitState *state, const char *name);
LitInterpretResult lit_state_execsource(LitState *state, const char *module_name, const char *code, size_t len);
LitInterpretResult lit_state_internexecsource(LitState *state, LitString *module_name, const char *code, size_t len);
bool lit_state_compileandsave(LitState *state, char *files[], size_t num_files, const char *output_file);
LitInterpretResult lit_state_execfile(LitState *state, const char *file);
LitInterpretResult lit_state_dumpfile(LitState *state, const char *file);
void lit_state_raiseerror(LitState *state, LitErrType type, const char *message, ...);
void lit_state_printf(LitState *state, const char *message, ...);
/* libarray.c */
void lit_datalist_init(LitDataList *dl, size_t typsz);
void lit_datalist_destroy(LitState *state, LitDataList *dl);
size_t lit_datalist_count(LitDataList *dl);
size_t lit_datalist_size(LitDataList *dl);
size_t lit_datalist_capacity(LitDataList *dl);
void lit_datalist_clear(LitDataList *dl);
void lit_datalist_setcount(LitDataList *dl, size_t nc);
void lit_datalist_deccount(LitDataList *dl);
intptr_t lit_datalist_get(LitDataList *dl, size_t idx);
intptr_t lit_datalist_set(LitDataList *dl, size_t idx, intptr_t val);
void lit_datalist_push(LitState *state, LitDataList *dl, intptr_t value);
void lit_datalist_ensuresize(LitState *state, LitDataList *dl, size_t size);
void lit_vallist_init(LitValList *vl);
void lit_vallist_destroy(LitState *state, LitValList *vl);
size_t lit_vallist_size(LitValList *vl);
size_t lit_vallist_count(LitValList *vl);
size_t lit_vallist_capacity(LitValList *vl);
void lit_vallist_setcount(LitValList *vl, size_t nc);
void lit_vallist_clear(LitValList *vl);
void lit_vallist_deccount(LitValList *vl);
void lit_vallist_ensuresize(LitState *state, LitValList *values, size_t size);
LitValue lit_vallist_set(LitValList *vl, size_t idx, LitValue val);
LitValue lit_vallist_get(LitValList *vl, size_t idx);
void lit_vallist_push(LitState *state, LitValList *vl, LitValue value);
LitArray *lit_create_array(LitState *state);
size_t lit_array_count(LitArray *arr);
LitValue lit_array_pop(LitState *state, LitArray *arr);
int lit_array_indexof(LitArray *array, LitValue value);
LitValue lit_array_removeat(LitArray *array, size_t index);
void lit_array_push(LitState *state, LitArray *array, LitValue val);
LitValue lit_array_get(LitState *state, LitArray *array, size_t idx);
LitArray *lit_array_splice(LitState *state, LitArray *oa, int from, int to);
void lit_open_array_library(LitState *state);
/* libmodule.c */
void lit_open_module_library(LitState *state);
/* libmath.c */
void lit_open_math_library(LitState *state);
/* ccopt.c */
void lit_astopt_optdbg(const char *fmt, ...);
void lit_varlist_init(LitVarList *array);
void lit_varlist_destroy(LitState *state, LitVarList *array);
void lit_varlist_push(LitState *state, LitVarList *array, LitVariable value);
void lit_astopt_init(LitState *state, LitOptimizer *optimizer);
void lit_astopt_optast(LitOptimizer *optimizer, LitAstExprList *statements);
bool lit_astopt_isoptenabled(LitOptimization optimization);
void lit_astopt_setoptenabled(LitOptimization optimization, bool enabled);
void lit_astopt_setalloptenabled(bool enabled);
void lit_astopt_setoptlevel(LitOptLevel level);
const char *lit_astopt_getoptname(LitOptimization optimization);
const char *lit_astopt_getoptdescr(LitOptimization optimization);
const char *lit_astopt_getoptleveldescr(LitOptLevel level);

/* writer.c */
/* writer.c */
void lit_writer_init_file(LitState *state, LitWriter *wr, FILE *fh, _Bool forceflush);
void lit_writer_init_string(LitState *state, LitWriter *wr);
void lit_writer_writebyte(LitWriter *wr, int byte);
void lit_writer_writestringl(LitWriter *wr, const char *str, size_t len);
void lit_writer_writestring(LitWriter *wr, const char *str);
void lit_writer_writeformat(LitWriter *wr, const char *fmt, ...);
LitString *lit_writer_get_string(LitWriter *wr);
void lit_towriter_array(LitState *state, LitWriter *wr, LitArray *array, size_t size);
void lit_towriter_map(LitState *state, LitWriter *wr, LitMap *map, size_t size);
void lit_towriter_object(LitState *state, LitWriter *wr, LitValue value, bool withquot);
void lit_towriter_value(LitState *state, LitWriter *wr, LitValue value, bool withquot);
const char *lit_tostring_typename(LitValue value);
const char *lit_tostring_exprtype(LitExprType t);
const char *lit_tostring_optok(LitTokType t);
void lit_towriter_expr(LitState *state, LitWriter *wr, LitAstExpression *expr);
void lit_towriter_ast(LitState *state, LitWriter *wr, LitAstExprList *exlist);

/* libfunc.c */
LitFunction *lit_object_makefunction(LitState *state, LitModule *module);
LitClosure *lit_object_makeclosure(LitState *state, LitFunction *function);
LitNativeFunction *lit_object_makenativefunction(LitState *state, LitNativeFunctionFn function, LitString *name);
LitNativePrimFunction *lit_object_makenativeprimitive(LitState *state, LitNativePrimitiveFn function, LitString *name);
LitNativeMethod *lit_object_makenativemethod(LitState *state, LitNativeMethodFn method, LitString *name);
LitPrimitiveMethod *lit_object_makeprimitivemethod(LitState *state, LitPrimitiveMethodFn method, LitString *name);
LitBoundMethod *lit_object_makeboundmethod(LitState *state, LitValue receiver, LitValue method);
bool lit_value_iscallablefunction(LitValue value);
void lit_state_openfunctionlibrary(LitState *state);


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
    return lit_value_fromobject(lit_string_copy((state), (text), strlen(text)));
}

static inline LitString* lit_string_copyconst(LitState* state, const char* text)
{
    return lit_string_copy(state, text, strlen(text));
}
