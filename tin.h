
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
#define RETURN_OK(r) return (TinInterpretResult){ TINSTATE_OK, r };

#define RETURN_RUNTIME_ERROR() return (TinInterpretResult){ TINSTATE_RUNTIMEERROR, NULL_VALUE };

#define INTERPRET_RUNTIME_FAIL ((TinInterpretResult){ TINSTATE_INVALID, NULL_VALUE })



#define NULL_VALUE ((TinValue){.type=TINVAL_NULL})
#define TRUE_VALUE ((TinValue){.type=TINVAL_BOOL, .boolval=true})
#define FALSE_VALUE ((TinValue){.type=TINVAL_BOOL, .boolval=false})

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
typedef struct /**/TinTableEntry TinTableEntry;
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


struct TinTableEntry
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
    TinTableEntry* entries;
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
    char* chars;
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

/* libobject.c */
TinUpvalue *tin_object_makeupvalue(TinState *state, TinValue *slot);
TinModule *tin_object_makemodule(TinState *state, TinString *name);
TinUserdata *tin_object_makeuserdata(TinState *state, size_t size, bool ispointeronly);
TinRange *tin_object_makerange(TinState *state, double from, double to);
TinReference *tin_object_makereference(TinState *state, TinValue *slot);
void tin_object_destroy(TinState *state, TinObject *object);
void tin_object_destroylistof(TinState *state, TinObject *objects);
TinValue tin_function_getname(TinVM *vm, TinValue instance);
void tin_state_openobjectlibrary(TinState *state);
/* ccast.c */
void tin_exprlist_init(TinAstExprList *array);
void tin_exprlist_destroy(TinState *state, TinAstExprList *array);
void tin_exprlist_push(TinState *state, TinAstExprList *array, TinAstExpression *value);
void tin_paramlist_init(TinAstParamList *array);
void tin_paramlist_destroy(TinState *state, TinAstParamList *array);
void tin_paramlist_push(TinState *state, TinAstParamList *array, TinAstParameter value);
void tin_paramlist_destroyvalues(TinState *state, TinAstParamList *parameters);
void tin_ast_destroyexprlist(TinState *state, TinAstExprList *expressions);
void tin_ast_destroystmtlist(TinState *state, TinAstExprList *statements);
void tin_ast_destroyexpression(TinState *state, TinAstExpression *expression);
TinAstLiteralExpr *tin_ast_make_literalexpr(TinState *state, size_t line, TinValue value);
TinAstBinaryExpr *tin_ast_make_binaryexpr(TinState *state, size_t line, TinAstExpression *left, TinAstExpression *right, TinAstTokType op);
TinAstUnaryExpr *tin_ast_make_unaryexpr(TinState *state, size_t line, TinAstExpression *right, TinAstTokType op);
TinAstVarExpr *tin_ast_make_varexpr(TinState *state, size_t line, const char *name, size_t length);
TinAstAssignExpr *tin_ast_make_assignexpr(TinState *state, size_t line, TinAstExpression *to, TinAstExpression *value);
TinAstCallExpr *tin_ast_make_callexpr(TinState *state, size_t line, TinAstExpression *callee);
TinAstGetExpr *tin_ast_make_getexpr(TinState *state, size_t line, TinAstExpression *where, const char *name, size_t length, bool questionable, bool ignoreresult);
TinAstSetExpr *tin_ast_make_setexpr(TinState *state, size_t line, TinAstExpression *where, const char *name, size_t length, TinAstExpression *value);
TinAstFunctionExpr *tin_ast_make_lambdaexpr(TinState *state, size_t line);
TinAstArrayExpr *tin_ast_make_arrayexpr(TinState *state, size_t line);
TinAstObjectExpr *tin_ast_make_objectexpr(TinState *state, size_t line);
TinAstIndexExpr *tin_ast_make_subscriptexpr(TinState *state, size_t line, TinAstExpression *array, TinAstExpression *index);
TinAstThisExpr *tin_ast_make_thisexpr(TinState *state, size_t line);
TinAstSuperExpr *tin_ast_make_superexpr(TinState *state, size_t line, TinString *method, bool ignoreresult);
TinAstRangeExpr *tin_ast_make_rangeexpr(TinState *state, size_t line, TinAstExpression *from, TinAstExpression *to);
TinAstTernaryExpr *tin_ast_make_ternaryexpr(TinState *state, size_t line, TinAstExpression *condition, TinAstExpression *ifbranch, TinAstExpression *elsebranch);
TinAstStrInterExpr *tin_ast_make_strinterpolexpr(TinState *state, size_t line);
TinAstRefExpr *tin_ast_make_referenceexpr(TinState *state, size_t line, TinAstExpression *to);
TinAstExprExpr *tin_ast_make_exprstmt(TinState *state, size_t line, TinAstExpression *expression);
TinAstBlockExpr *tin_ast_make_blockexpr(TinState *state, size_t line);
TinAstAssignVarExpr *tin_ast_make_assignvarexpr(TinState *state, size_t line, const char *name, size_t length, TinAstExpression *init, bool constant);
TinAstIfExpr *tin_ast_make_ifexpr(TinState *state, size_t line, TinAstExpression *condition, TinAstExpression *ifbranch, TinAstExpression *elsebranch, TinAstExprList *elseifconditions, TinAstExprList *elseifbranches);
TinAstWhileExpr *tin_ast_make_whileexpr(TinState *state, size_t line, TinAstExpression *condition, TinAstExpression *body);
TinAstForExpr *tin_ast_make_forexpr(TinState *state, size_t line, TinAstExpression *init, TinAstExpression *var, TinAstExpression *condition, TinAstExpression *increment, TinAstExpression *body, bool cstyle);
TinAstContinueExpr *tin_ast_make_continueexpr(TinState *state, size_t line);
TinAstBreakExpr *tin_ast_make_breakexpr(TinState *state, size_t line);
TinAstFunctionExpr *tin_ast_make_funcexpr(TinState *state, size_t line, const char *name, size_t length);
TinAstReturnExpr *tin_ast_make_returnexpr(TinState *state, size_t line, TinAstExpression *expression);
TinAstMethodExpr *tin_ast_make_methodexpr(TinState *state, size_t line, TinString *name, bool isstatic);
TinAstClassExpr *tin_ast_make_classexpr(TinState *state, size_t line, TinString *name, TinString *parent);
TinAstFieldExpr *tin_ast_make_fieldexpr(TinState *state, size_t line, TinString *name, TinAstExpression *getter, TinAstExpression *setter, bool isstatic);
TinAstExprList *tin_ast_allocexprlist(TinState *state);
void tin_ast_destroy_allocdexprlist(TinState *state, TinAstExprList *expressions);
TinAstExprList *tin_ast_allocate_stmtlist(TinState *state);
void tin_ast_destry_allocdstmtlist(TinState *state, TinAstExprList *statements);
/* librange.c */
void tin_open_range_library(TinState *state);
/* ccemit.c */
void tin_privlist_init(TinAstPrivList *array);
void tin_privlist_destroy(TinState *state, TinAstPrivList *array);
void tin_privlist_push(TinState *state, TinAstPrivList *array, TinAstPrivate value);
void tin_loclist_init(TinAstLocList *array);
void tin_loclist_destroy(TinState *state, TinAstLocList *array);
void tin_loclist_push(TinState *state, TinAstLocList *array, TinAstLocal value);
void tin_astemit_init(TinState *state, TinAstEmitter *emitter);
void tin_astemit_destroy(TinAstEmitter *emitter);
TinModule *tin_astemit_modemit(TinAstEmitter *emitter, TinAstExprList *statements, TinString *module_name);
/* vm.c */
uint16_t tin_vmexec_readshort(TinExecState *est);
uint8_t tin_vmexec_readbyte(TinExecState *est);
TinValue tin_vmexec_readconstant(TinExecState *est);
TinValue tin_vmexec_readconstantlong(TinExecState *est);
TinString *tin_vmexec_readstring(TinExecState *est);
TinString *tin_vmexec_readstringlong(TinExecState *est);
void tin_vmexec_push(TinFiber *fiber, TinValue v);
TinValue tin_vmexec_pop(TinFiber *fiber);
void tin_vmexec_drop(TinFiber *fiber);
void tin_vmexec_dropn(TinFiber *fiber, int amount);
TinValue tin_vmexec_peek(TinFiber *fiber, short distance);
void tin_vmexec_readframe(TinFiber *fiber, TinExecState *est);
void tin_vmexec_writeframe(TinExecState *est, uint8_t *ip);
void tin_vmexec_resetstack(TinVM *vm);
void tin_vmexec_resetvm(TinState *state, TinVM *vm);
void tin_vm_init(TinState *state, TinVM *vm);
void tin_vm_destroy(TinVM *vm);
void tin_vm_tracestack(TinVM *vm, TinWriter *wr);
bool tin_vm_handleruntimeerror(TinVM *vm, TinString *error_string);
bool tin_vm_vraiseerror(TinVM *vm, const char *format, va_list args);
bool tin_vm_raiseerror(TinVM *vm, const char *format, ...);
bool tin_vm_raiseexitingerror(TinVM *vm, const char *format, ...);
bool tin_vm_callcallable(TinVM *vm, TinFunction *function, TinClosure *closure, uint8_t argc);
const char *tin_vmexec_funcnamefromvalue(TinVM *vm, TinExecState *est, TinValue v);
bool tin_vm_callvalue(TinVM *vm, TinFiber *fiber, TinExecState *est, TinValue callee, uint8_t argc);
TinUpvalue *tin_execvm_captureupvalue(TinState *state, TinValue *local);
void tin_vm_closeupvalues(TinVM *vm, const TinValue *last);
TinInterpretResult tin_vm_execmodule(TinState *state, TinModule *module);
TinInterpretResult tin_vm_execfiber(TinState *state, TinFiber *fiber);
void tin_vmutil_callexitjump(void);
bool tin_vmutil_setexitjump(void);
/* chunk.c */
void tin_chunk_init(TinChunk *chunk);
void tin_chunk_destroy(TinState *state, TinChunk *chunk);
void tin_chunk_push(TinState *state, TinChunk *chunk, uint8_t byte, uint16_t line);
size_t tin_chunk_addconst(TinState *state, TinChunk *chunk, TinValue constant);
size_t tin_chunk_getline(TinChunk *chunk, size_t offset);
void tin_chunk_shrink(TinState *state, TinChunk *chunk);
void tin_chunk_emitbyte(TinState *state, TinChunk *chunk, uint8_t byte);
void tin_chunk_emit2bytes(TinState *state, TinChunk *chunk, uint8_t a, uint8_t b);
void tin_chunk_emitshort(TinState *state, TinChunk *chunk, uint16_t value);
/* libclass.c */
TinClass *tin_create_class(TinState *state, TinString *name);
TinClass *tin_create_classobject(TinState *state, const char *name);
TinField *tin_create_field(TinState *state, TinObject *getter, TinObject *setter);
TinInstance *tin_create_instance(TinState *state, TinClass *klass);
void tin_class_bindconstructor(TinState *state, TinClass *cl, TinNativeMethodFn fn);
TinNativeMethod *tin_class_bindmethod(TinState *state, TinClass *cl, const char *name, TinNativeMethodFn fn);
TinPrimitiveMethod *tin_class_bindprimitive(TinState *state, TinClass *cl, const char *name, TinPrimitiveMethodFn fn);
TinNativeMethod *tin_class_bindstaticmethod(TinState *state, TinClass *cl, const char *name, TinNativeMethodFn fn);
TinPrimitiveMethod *tin_class_bindstaticprimitive(TinState *state, TinClass *cl, const char *name, TinPrimitiveMethodFn fn);
void tin_class_setstaticfield(TinState *state, TinClass *cl, const char *name, TinValue val);
TinField *tin_class_bindgetset(TinState *state, TinClass *cl, const char *name, TinNativeMethodFn getfn, TinNativeMethodFn setfn, bool isstatic);
void tin_class_inheritfrom(TinState *state, TinClass *current, TinClass *other);
void tin_open_class_library(TinState *state);
/* libfiber.c */
TinFiber *tin_create_fiber(TinState *state, TinModule *module, TinFunction *function);
void tin_ensure_fiber_stack(TinState *state, TinFiber *fiber, size_t needed);
void tin_open_fiber_library(TinState *state);
/* libfs.c */
bool tin_fs_diropen(TinDirReader *rd, const char *path);
bool tin_fs_dirread(TinDirReader *rd, TinDirItem *itm);
bool tin_fs_dirclose(TinDirReader *rd);
char *tin_util_readfile(const char *path, size_t *dlen);
bool tin_fs_fileexists(const char *path);
bool tin_fs_direxists(const char *path);
size_t tin_ioutil_writeuint8(FILE *file, uint8_t byte);
size_t tin_ioutil_writeuint16(FILE *file, uint16_t byte);
size_t tin_ioutil_writeuint32(FILE *file, uint32_t byte);
size_t tin_ioutil_writedouble(FILE *file, double byte);
size_t tin_ioutil_writestring(FILE *file, TinString *string);
uint8_t tin_ioutil_readuint8(FILE *file);
uint16_t tin_ioutil_readuint16(FILE *file);
uint32_t tin_ioutil_readuint32(FILE *file);
double tin_ioutil_readdouble(FILE *file);
TinString *tin_ioutil_readstring(TinState *state, FILE *file);
void tin_emufile_init(TinEmulatedFile *file, const char *source, size_t len);
uint8_t tin_emufile_readuint8(TinEmulatedFile *file);
uint16_t tin_emufile_readuint16(TinEmulatedFile *file);
uint32_t tin_emufile_readuint32(TinEmulatedFile *file);
double tin_emufile_readdouble(TinEmulatedFile *file);
TinString *tin_emufile_readstring(TinState *state, TinEmulatedFile *file);
void tin_ioutil_writemodule(TinModule *module, FILE *file);
TinModule *tin_ioutil_readmodule(TinState *state, const char *input, size_t len);
void tin_userfile_cleanup(TinState *state, TinUserdata *data, bool mark);
void tin_open_file_library(TinState *state);
/* ccparser.c */
const char *tin_astparser_token2name(int t);
void tin_astparser_init(TinState *state, TinAstParser *parser);
void tin_astparser_destroy(TinAstParser *parser);
bool tin_astparser_parsesource(TinAstParser *parser, const char *filename, const char *source, TinAstExprList *statements);
/* util.c */
uint64_t pack754(long double f, unsigned bits, unsigned expbits);
long double unpack754(uint64_t i, unsigned bits, unsigned expbits);
double tin_util_uinttofloat(unsigned int val);
unsigned int tin_util_floattouint(double val);
int tin_util_doubletoint(double n);
int tin_util_numbertoint32(double n);
unsigned int tin_util_numbertouint32(double n);
int tin_util_closestpowof2(int n);
char *tin_util_patchfilename(char *filename);
char *tin_util_copystring(const char *string);
/* error.c */
TinString *tin_vformat_error(TinState *state, size_t line, const char* fmt, va_list args);
TinString *tin_format_error(TinState *state, size_t line, const char* fmt, ...);
/* value.c */
TinValue tin_value_fromobject_actual(TinObject* obj);
TinObject *tin_value_asobject(TinValue v);
TinValue tin_value_makebool(TinState *state, bool b);
TinObjType tin_value_type(TinValue v);
TinValue tin_value_makefloatnumber(TinState *state, double num);
TinValue tin_value_makefixednumber(TinState *state, int64_t num);
TinValue tin_value_makenumber(TinState* state, double num);


bool tin_value_compare(TinState *state, const TinValue a, const TinValue b);
TinString *tin_value_tostring(TinState *state, TinValue object);
double tin_value_checknumber(TinVM *vm, TinValue *args, uint8_t arg_count, uint8_t id);
double tin_value_getnumber(TinVM *vm, TinValue *args, uint8_t arg_count, uint8_t id, double def);
bool tin_value_checkbool(TinVM *vm, TinValue *args, uint8_t arg_count, uint8_t id);
bool tin_value_getbool(TinVM *vm, TinValue *args, uint8_t arg_count, uint8_t id, bool def);
const char *tin_value_checkstring(TinVM *vm, TinValue *args, uint8_t arg_count, uint8_t id);
const char *tin_value_getstring(TinVM *vm, TinValue *args, uint8_t arg_count, uint8_t id, const char *def);
TinString *tin_value_checkobjstring(TinVM *vm, TinValue *args, uint8_t arg_count, uint8_t id);
TinInstance *tin_value_checkinstance(TinVM *vm, TinValue *args, uint8_t arg_count, uint8_t id);
TinValue *tin_value_checkreference(TinVM *vm, TinValue *args, uint8_t arg_count, uint8_t id);
void tin_value_ensurebool(TinVM *vm, TinValue value, const char *emsg);
void tin_value_ensurestring(TinVM *vm, TinValue value, const char *emsg);
void tin_value_ensurenumber(TinVM *vm, TinValue value, const char *emsg);
void tin_value_ensureobjtype(TinVM *vm, TinValue value, TinObjType type, const char *emsg);
TinValue tin_value_callnew(TinVM *vm, const char *name, TinValue *args, size_t argc, bool ignfiber);
/* libmap.c */
void tin_table_init(TinState *state, TinTable *table);
void tin_table_destroy(TinState *state, TinTable *table);
bool tin_table_set(TinState *state, TinTable *table, TinString *key, TinValue value);
bool tin_table_get(TinTable *table, TinString *key, TinValue *value);
bool tin_table_get_slot(TinTable *table, TinString *key, TinValue **value);
bool tin_table_delete(TinTable *table, TinString *key);
TinString *tin_table_find_string(TinTable *table, const char *chars, size_t length, uint32_t hash);
void tin_table_add_all(TinState *state, TinTable *from, TinTable *to);
void tin_table_removewhite(TinTable *table);
int util_table_iterator(TinTable *table, int number);
TinValue util_table_iterator_key(TinTable *table, int index);
TinMap *tin_create_map(TinState *state);
bool tin_map_set(TinState *state, TinMap *map, TinString *key, TinValue value);
bool tin_map_get(TinMap *map, TinString *key, TinValue *value);
bool tin_map_delete(TinMap *map, TinString *key);
void tin_map_add_all(TinState *state, TinMap *from, TinMap *to);
void tin_open_map_library(TinState *state);
/* main.c */
int exitstate(TinState *state, TinStatus result);
void interupt_handler(int signal_id);
int main(int argc, char *argv[]);
int oldmain(int argc, const char *argv[]);
/* gcmem.c */
TinObject *tin_gcmem_allocobject(TinState *state, size_t size, TinObjType type, bool islight);
void *tin_gcmem_memrealloc(TinState *state, void *pointer, size_t old_size, size_t new_size);
void tin_gcmem_marktable(TinVM *vm, TinTable *table);
void tin_gcmem_markobject(TinVM *vm, TinObject *object);
void tin_gcmem_markvalue(TinVM *vm, TinValue value);
void tin_gcmem_vmmarkroots(TinVM *vm);
void tin_gcmem_markarray(TinVM *vm, TinValList *array);
void tin_gcmem_vmblackobject(TinVM *vm, TinObject *object);
void tin_gcmem_vmtracerefs(TinVM *vm);
void tin_gcmem_vmsweep(TinVM *vm);
uint64_t tin_gcmem_collectgarbage(TinVM *vm);
void tin_open_gc_library(TinState *state);
/* debug.c */
void tin_disassemble_module(TinState *state, TinModule *module, const char *source);
void tin_disassemble_chunk(TinState *state, TinChunk *chunk, const char *name, const char *source);
size_t tin_disassemble_instruction(TinState *state, TinChunk *chunk, size_t offset, const char *source);
void tin_trace_frame(TinFiber *fiber, TinWriter *wr);
/* ccscan.c */
void tin_bytelist_init(TinAstByteList *bl);
void tin_bytelist_destroy(TinState *state, TinAstByteList *bl);
void tin_bytelist_push(TinState *state, TinAstByteList *bl, uint8_t value);
void tin_astlex_init(TinState *state, TinAstScanner *scanner, const char *filename, const char *source);
TinAstToken tin_astlex_scantoken(TinAstScanner *scanner);
/* libstring.c */
char *itoa(int value, char *result, int base);
char *tin_util_inttostring(char *dest, size_t n, int x);
uint32_t tin_util_hashstring(const char *key, size_t length);
int tin_util_decodenumbytes(uint8_t byte);
int tin_ustring_length(TinString *string);
TinString *tin_ustring_codepointat(TinState *state, TinString *string, uint32_t index);
TinString *tin_ustring_fromcodepoint(TinState *state, int value);
TinString *tin_ustring_fromrange(TinState *state, TinString *source, int start, uint32_t count);
int tin_util_encodenumbytes(int value);
int tin_ustring_encode(int value, uint8_t *bytes);
int tin_ustring_decode(const uint8_t *bytes, uint32_t length);
int tin_util_ucharoffset(char *str, int index);
TinString *tin_string_makeempty(TinState *state, size_t length, bool reuse);
TinString *tin_string_makelen(TinState *state, char *chars, size_t length, uint32_t hash, bool wassds, bool reuse);
void tin_state_regstring(TinState *state, TinString *string);
TinString *tin_string_take(TinState *state, char *chars, size_t length, bool wassds);
TinString *tin_string_copy(TinState *state, const char *chars, size_t length);
const char *tin_string_getdata(TinString *ls);
size_t tin_string_getlength(TinString *ls);
void tin_string_appendlen(TinString *ls, const char *s, size_t len);
void tin_string_appendobj(TinString *ls, TinString *other);
void tin_string_appendchar(TinString *ls, char ch);
TinValue tin_string_numbertostring(TinState *state, double value);
TinValue tin_string_format(TinState *state, const char *format, ...);
bool tin_string_equal(TinState *state, TinString *a, TinString *b);
bool check_fmt_arg(TinVM *vm, char *buf, size_t ai, size_t argc, TinValue *argv, const char *fmttext);
void tin_open_string_library(TinState *state);
/* libcore.c */
void tin_open_libraries(TinState *state);
void util_custom_quick_sort(TinVM *vm, TinValue *l, int length, TinValue callee);
bool util_is_fiber_done(TinFiber *fiber);
void util_run_fiber(TinVM *vm, TinFiber *fiber, TinValue *argv, size_t argc, bool catcher);
void util_basic_quick_sort(TinState *state, TinValue *clist, int length);
bool util_interpret(TinVM *vm, TinModule *module);
bool util_test_file_exists(const char *filename);
bool util_attempt_to_require(TinVM *vm, TinValue *argv, size_t argc, const char *path, bool ignore_previous, bool folders);
bool util_attempt_to_require_combined(TinVM *vm, TinValue *argv, size_t argc, const char *a, const char *b, bool ignore_previous);
TinValue util_invalid_constructor(TinVM *vm, TinValue instance, size_t argc, TinValue *argv);
void tin_open_core_library(TinState *state);
/* state.c */
void tin_enable_compilation_time_measurement(void);
TinState *tin_make_state(void);
int64_t tin_destroy_state(TinState *state);
void tin_api_init(TinState *state);
void tin_api_destroy(TinState *state);
TinValue tin_state_getglobalvalue(TinState *state, TinString *name);
TinFunction *tin_state_getglobalfunction(TinState *state, TinString *name);
void tin_state_setglobal(TinState *state, TinString *name, TinValue value);
bool tin_state_hasglobal(TinState *state, TinString *name);
void tin_state_defnativefunc(TinState *state, const char *name, TinNativeFunctionFn native);
void tin_state_defnativeprimitive(TinState *state, const char *name, TinNativePrimitiveFn native);
TinValue tin_state_getinstancemethod(TinState *state, TinValue callee, TinString *mthname);
TinInterpretResult tin_state_callinstancemethod(TinState *state, TinValue callee, TinString *mthname, TinValue *argv, size_t argc);
TinValue tin_state_getfield(TinState *state, TinTable *table, const char *name);
TinValue tin_state_getmapfield(TinState *state, TinMap *map, const char *name);
void tin_state_setfield(TinState *state, TinTable *table, const char *name, TinValue value);
void tin_state_setmapfield(TinState *state, TinMap *map, const char *name, TinValue value);
bool tin_state_ensurefiber(TinVM *vm, TinFiber *fiber);
TinInterpretResult tin_state_callfunction(TinState *state, TinFunction *callee, TinValue *argv, uint8_t argc, bool ignfiber);
TinInterpretResult tin_state_callclosure(TinState *state, TinClosure *callee, TinValue *argv, uint8_t argc, bool ignfiber);
TinInterpretResult tin_state_callmethod(TinState *state, TinValue instance, TinValue callee, TinValue *argv, uint8_t argc, bool ignfiber);
TinInterpretResult tin_state_callvalue(TinState *state, TinValue callee, TinValue *argv, uint8_t argc, bool ignfiber);
TinInterpretResult tin_state_findandcallmethod(TinState *state, TinValue callee, TinString *method_name, TinValue *argv, uint8_t argc, bool ignfiber);
void tin_state_pushroot(TinState *state, TinObject *object);
void tin_state_pushvalueroot(TinState *state, TinValue value);
TinValue tin_state_peekroot(TinState *state, uint8_t distance);
void tin_state_poproot(TinState *state);
void tin_state_poproots(TinState *state, uint8_t amount);
TinClass *tin_state_getclassfor(TinState *state, TinValue value);
TinModule *tin_state_compilemodule(TinState *state, TinString *module_name, const char *code, size_t len);
TinModule *tin_state_getmodule(TinState *state, const char *name);
TinInterpretResult tin_state_execsource(TinState *state, const char *module_name, const char *code, size_t len);
TinInterpretResult tin_state_internexecsource(TinState *state, TinString *module_name, const char *code, size_t len);
bool tin_state_compileandsave(TinState *state, char *files[], size_t num_files, const char *output_file);
TinInterpretResult tin_state_execfile(TinState *state, const char *file);
TinInterpretResult tin_state_dumpfile(TinState *state, const char *file);
void tin_state_raiseerror(TinState *state, TinErrType type, const char *message, ...);
void tin_state_printf(TinState *state, const char *message, ...);
/* libarray.c */
void tin_datalist_init(TinDataList *dl, size_t typsz);
void tin_datalist_destroy(TinState *state, TinDataList *dl);
size_t tin_datalist_count(TinDataList *dl);
size_t tin_datalist_size(TinDataList *dl);
size_t tin_datalist_capacity(TinDataList *dl);
void tin_datalist_clear(TinDataList *dl);
void tin_datalist_setcount(TinDataList *dl, size_t nc);
void tin_datalist_deccount(TinDataList *dl);
intptr_t tin_datalist_get(TinDataList *dl, size_t idx);
intptr_t tin_datalist_set(TinDataList *dl, size_t idx, intptr_t val);
void tin_datalist_push(TinState *state, TinDataList *dl, intptr_t value);
void tin_datalist_ensuresize(TinState *state, TinDataList *dl, size_t size);
void tin_vallist_init(TinValList *vl);
void tin_vallist_destroy(TinState *state, TinValList *vl);
size_t tin_vallist_size(TinValList *vl);
size_t tin_vallist_count(TinValList *vl);
size_t tin_vallist_capacity(TinValList *vl);
void tin_vallist_setcount(TinValList *vl, size_t nc);
void tin_vallist_clear(TinValList *vl);
void tin_vallist_deccount(TinValList *vl);
void tin_vallist_ensuresize(TinState *state, TinValList *values, size_t size);
TinValue tin_vallist_set(TinValList *vl, size_t idx, TinValue val);
TinValue tin_vallist_get(TinValList *vl, size_t idx);
void tin_vallist_push(TinState *state, TinValList *vl, TinValue value);
TinArray *tin_create_array(TinState *state);
size_t tin_array_count(TinArray *arr);
TinValue tin_array_pop(TinState *state, TinArray *arr);
int tin_array_indexof(TinArray *array, TinValue value);
TinValue tin_array_removeat(TinArray *array, size_t index);
void tin_array_push(TinState *state, TinArray *array, TinValue val);
TinValue tin_array_get(TinState *state, TinArray *array, size_t idx);
TinArray *tin_array_splice(TinState *state, TinArray *oa, int from, int to);
void tin_open_array_library(TinState *state);
/* libmodule.c */
void tin_open_module_library(TinState *state);
/* libmath.c */
void tin_open_math_library(TinState *state);
/* ccopt.c */
void tin_astopt_optdbg(const char *fmt, ...);
void tin_varlist_init(TinVarList *array);
void tin_varlist_destroy(TinState *state, TinVarList *array);
void tin_varlist_push(TinState *state, TinVarList *array, TinVariable value);
void tin_astopt_init(TinState *state, TinAstOptimizer *optimizer);
void tin_astopt_optast(TinAstOptimizer *optimizer, TinAstExprList *statements);
bool tin_astopt_isoptenabled(TinAstOptType optimization);
void tin_astopt_setoptenabled(TinAstOptType optimization, bool enabled);
void tin_astopt_setalloptenabled(bool enabled);
void tin_astopt_setoptlevel(TinAstOptLevel level);
const char *tin_astopt_getoptname(TinAstOptType optimization);
const char *tin_astopt_getoptdescr(TinAstOptType optimization);
const char *tin_astopt_getoptleveldescr(TinAstOptLevel level);

/* writer.c */
/* writer.c */
void tin_writer_init_file(TinState *state, TinWriter *wr, FILE *fh, _Bool forceflush);
void tin_writer_init_string(TinState *state, TinWriter *wr);
void tin_writer_writebyte(TinWriter *wr, int byte);
void tin_writer_writestringl(TinWriter *wr, const char *str, size_t len);
void tin_writer_writestring(TinWriter *wr, const char *str);
void tin_writer_writeformat(TinWriter *wr, const char *fmt, ...);
void tin_writer_writeescapedbyte(TinWriter* wr, int ch);
void tin_writer_writeescapedstring(TinWriter* wr, const char* str, size_t len, bool withquot);
TinString *tin_writer_get_string(TinWriter *wr);
void tin_towriter_array(TinState *state, TinWriter *wr, TinArray *array, size_t size);
void tin_towriter_map(TinState *state, TinWriter *wr, TinMap *map, size_t size);
void tin_towriter_object(TinState *state, TinWriter *wr, TinValue value, bool withquot);
void tin_towriter_value(TinState *state, TinWriter *wr, TinValue value, bool withquot);
const char *tin_tostring_typename(TinValue value);
const char *tin_tostring_exprtype(TinAstExprType t);
const char *tin_tostring_optok(TinAstTokType t);
void tin_towriter_expr(TinState *state, TinWriter *wr, TinAstExpression *expr);
void tin_towriter_ast(TinState *state, TinWriter *wr, TinAstExprList *exlist);

/* libfunc.c */
TinFunction *tin_object_makefunction(TinState *state, TinModule *module);
TinClosure *tin_object_makeclosure(TinState *state, TinFunction *function);
TinNativeFunction *tin_object_makenativefunction(TinState *state, TinNativeFunctionFn function, TinString *name);
TinNativePrimFunction *tin_object_makenativeprimitive(TinState *state, TinNativePrimitiveFn function, TinString *name);
TinNativeMethod *tin_object_makenativemethod(TinState *state, TinNativeMethodFn method, TinString *name);
TinPrimitiveMethod *tin_object_makeprimitivemethod(TinState *state, TinPrimitiveMethodFn method, TinString *name);
TinBoundMethod *tin_object_makeboundmethod(TinState *state, TinValue receiver, TinValue method);
bool tin_value_iscallablefunction(TinValue value);
void tin_state_openfunctionlibrary(TinState *state);


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
    return (tin_value_asstring(v)->chars);
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

static inline TinValue tin_value_makefalse(TinState* state)
{
    (void)state;
    return FALSE_VALUE;
}

static inline TinValue tin_value_maketrue(TinState* state)
{
    (void)state;
    return TRUE_VALUE;
}

static inline TinValue tin_value_makenull(TinState* state)
{
    (void)state;
    return NULL_VALUE;
}


static inline TinValue tin_value_makestring(TinState* state, const char* text)
{
    return tin_value_fromobject(tin_string_copy((state), (text), strlen(text)));
}

static inline TinString* tin_string_copyconst(TinState* state, const char* text)
{
    return tin_string_copy(state, text, strlen(text));
}
