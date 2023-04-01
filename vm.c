

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <setjmp.h>
#include "priv.h"

/*
* function/macro prefixes:
*
*   tin_vmmac_* = macros
*   tin_vmintern_* = functions used exclusively in this file (which may be inlined)
*   tin_vmdo_* = functions that are called in execfiber
*   tin_vm_* = all other vm functions
*/

// visual studio doesn't support computed gotos, so
// instead a switch-case is used. 
#if !defined(_MSC_VER)
    #define TIN_USE_COMPUTEDGOTO
#endif

#ifdef TIN_TRACE_EXECUTION
    #define tin_vmmac_traceframe(fiber)\
        tin_trace_frame(fiber);
#else
    #define tin_vmmac_traceframe(fiber) \
        do \
        { \
        } while(0);
#endif

// the following macros cannot be turned into a function without
// breaking everything

#ifdef TIN_USE_COMPUTEDGOTO
    #define vm_default()
    #define op_case(name) \
        name:
#else
    #define vm_default() default:
    #define op_case(name) \
        case name:
#endif

#define tin_vmmac_returnerror() \
    tin_vmmac_popgc(est); \
    return false;

#define tin_vmmac_pushgc(est, allow) \
    est->wasallowed = est->state->gcallow; \
    est->state->gcallow = allow;

#define tin_vmmac_popgc(est) \
    est->state->gcallow = est->wasallowed;

#define tin_vmmac_continue(bval) \
    continue;


// can't be turned into a function because it is expected to return in
// tin_vm_execfiber.
// might be possible to wrap this by using an enum to specify
// if (and what) to return, but it'll be quite a bit of work to refactor.
// likewise, any macro that uses tin_vmmac_recoverstate can't be turned into
// a function.
#define tin_vmmac_recoverstate(est) \
    tin_vmintern_writeframe(est, est->ip); \
    est->fiber = est->vm->fiber; \
    if(est->fiber == NULL) \
    { \
        *finalresult = tin_vmintern_pop(est); \
        return true; \
    } \
    if(est->fiber->abort) \
    { \
        tin_vmmac_returnerror(); \
    } \
    tin_vmintern_readframe(est); \
    tin_vmmac_traceframe(est->fiber);


#define tin_vmmac_callvalue(callee, name, argc) \
    if(tin_vm_callvalue(est, callee, name, argc)) \
    { \
        tin_vmmac_recoverstate(est); \
    }


#define tin_vmmac_raiseerrorfmtcont(format, ...) \
    if(tin_vm_raiseerror(est->vm, format, __VA_ARGS__)) \
    { \
        tin_vmmac_recoverstate(est);  \
        tin_vmmac_continue(false); \
    } \
    else \
    { \
        tin_vmmac_returnerror(); \
    }


#define tin_vmmac_raiseerrorfmtnocont(format, ...) \
    if(tin_vm_raiseerror(est->vm, format, __VA_ARGS__)) \
    { \
        tin_vmmac_recoverstate(est);  \
        return true; \
    } \
    else \
    { \
        tin_vmmac_returnerror(); \
    }

#define tin_vmmac_raiseerror(format) \
    tin_vmmac_raiseerrorfmtcont(format, NULL);

#define tin_vmmac_advinvokefromclass(zklass, mthname, argc, raiseerr, stat, ignoring, callee) \
    TinValue mthval; \
    if((tin_value_isinstance(callee) && (tin_table_get(&tin_value_asinstance(callee)->fields, mthname, &mthval))) \
       || tin_table_get(&zklass->stat, mthname, &mthval)) \
    { \
        if(ignoring) \
        { \
            if(tin_vm_callvalue(est, mthval, mthname, argc)) \
            { \
                tin_vmmac_recoverstate(est); \
                est->frame->ignresult = true; \
            } \
            else \
            { \
                est->fiber->stacktop[-1] = callee; \
            } \
        } \
        else \
        { \
            tin_vmmac_callvalue(mthval, mthname, argc); \
        } \
    } \
    else \
    { \
        if(raiseerr) \
        { \
            tin_vmmac_raiseerrorfmtcont("cannot call undefined method '%s' of class '%s'", mthname->data, \
                               zklass->name->data) \
        } \
    } \
    if(raiseerr) \
    { \
        tin_vmmac_continue(false); \
    }


// calls tin_vmmac_recoverstate
#define tin_vmmac_invokefromclass(klass, mthname, argc, raiseerr, stat, ignoring) \
    tin_vmmac_advinvokefromclass(klass, mthname, argc, raiseerr, stat, ignoring, tin_vmintern_peek(est, argc))

// calls tin_vmmac_recoverstate
#define tin_vmmac_invokemethod(instance, mthname, argc) \
    if(tin_value_isnull(instance)) \
    { \
        /*fprintf(stderr, "mthname=<%s>\n", mthname); */\
        tin_vmmac_raiseerrorfmtcont("cannot call method '%s' of null-instance", mthname); \
    } \
    TinClass* klass = tin_state_getclassfor(est->state, instance); \
    if(klass == NULL) \
    { \
        tin_vmmac_raiseerrorfmtcont("cannot call method '%s' of a non-class", mthname); \
    } \
    tin_vmintern_writeframe(est, est->ip); \
    tin_vmmac_advinvokefromclass(klass, tin_string_copyconst(est->state, mthname), argc, true, methods, false, instance); \
    tin_vmintern_readframe(est);

#define tin_vmmac_binaryop(op, opstring) \
    TinValue a = tin_vmintern_peek(est, 1); \
    TinValue b = tin_vmintern_peek(est, 0); \
    /* implicitly converts NULL to numeric 0 */ \
    if(tin_value_isnumber(a) || tin_value_isnull(a)) \
    { \
        if(!tin_value_isnumber(b)) \
        { \
            if(!tin_value_isnull(b)) \
            { \
                tin_vmmac_raiseerrorfmtcont("cannot use op '%s' with a 'number' and a '%s'", opstring, tin_tostring_typename(b)); \
            } \
        } \
        tin_vmintern_drop(est); \
        if(vm_binaryop_actual(est, op, a, b))\
        { \
            tin_vmmac_continue(true); \
        } \
    } \
    if(tin_value_isnull(a)) \
    { \
        tin_vmintern_drop(est); \
        if(tin_value_isnull(b)) \
        { \
            *(est->fiber->stacktop - 1) = tin_value_makebool(est->state, true); \
        } \
        else \
        { \
            tin_vmmac_raiseerrorfmtcont("cannot use op %s on a null value", opstring); \
            *(est->fiber->stacktop - 1) = tin_value_makebool(est->state, false);\
        } \
    } \
    else \
    { \
        tin_vmmac_invokemethod(a, opstring, 1); \
    }

enum
{
    RECOVER_RETURNOK,
    RECOVER_RETURNFAIL,
    RECOVER_NOTHING
};

static jmp_buf jumpbuffer;

//#define TIN_TRACE_EXECUTION

TIN_VM_INLINE uint16_t tin_vmintern_readshort(TinExecState *est);
TIN_VM_INLINE uint8_t tin_vmintern_readbyte(TinExecState *est);
TIN_VM_INLINE TinValue tin_vmintern_readconstant(TinExecState *est);
TIN_VM_INLINE TinValue tin_vmintern_readconstantlong(TinExecState *est);
TIN_VM_INLINE TinString *tin_vmintern_readstring(TinExecState *est);
TIN_VM_INLINE TinString *tin_vmintern_readstringlong(TinExecState *est);
TIN_VM_INLINE void tin_vmintern_push(TinExecState *est, TinValue v);
TIN_VM_INLINE TinValue tin_vmintern_pop(TinExecState* est);
TIN_VM_INLINE void tin_vmintern_drop(TinExecState* est);
TIN_VM_INLINE void tin_vmintern_dropn(TinExecState *est, int amount);
TIN_VM_INLINE TinValue tin_vmintern_peek(TinExecState *est, short distance);
TIN_VM_INLINE void tin_vmintern_readframe(TinExecState *est);
TIN_VM_INLINE void tin_vmintern_writeframe(TinExecState *est, uint8_t *ip);
void tin_vmintern_resetstack(TinVM *vm);
void tin_vmintern_resetvm(TinState *state, TinVM *vm);
void tin_vmintern_tracestack(TinVM *vm, TinWriter *wr);
TinUpvalue *tin_vmintern_captureupvalue(TinState *state, TinValue *local);
void tin_vmintern_closeupvalues(TinVM *vm, const TinValue *last);

bool tin_vmintern_execfiber(TinState* exstate, TinFiber* exfiber, TinValue* finalresult);
TinInterpretResult tin_vm_execfiber(TinState* state, TinFiber* fiber);

void tin_vm_init(TinState *state, TinVM *vm);
void tin_vm_destroy(TinVM *vm);
bool tin_vm_handleruntimeerror(TinVM *vm, TinString *errorstring);
bool tin_vm_vraiseerror(TinVM *vm, const char *format, va_list args);
bool tin_vm_raiseerror(TinVM *vm, const char *format, ...);
bool tin_vm_raiseexitingerror(TinVM *vm, const char *format, ...);
bool tin_vm_callcallable(TinVM *vm, TinFunction *function, TinClosure *closure, uint8_t argc);
bool tin_vm_callvalue(TinExecState* est, TinValue callee, TinString* name, uint8_t argc);
TinInterpretResult tin_vm_execmodule(TinState *state, TinModule *module);
bool tin_vmintern_execfiber(TinState* state, TinFiber* fiber, TinValue* finalresult);
void tin_vm_callexitjump(TinVM* vm);
bool tin_vm_setexitjump(TinVM* vm);


TIN_VM_INLINE uint16_t tin_vmintern_readshort(TinExecState* est)
{
    est->ip += 2u;
    return (uint16_t)((est->ip[-2] << 8u) | est->ip[-1]);
}

TIN_VM_INLINE uint8_t tin_vmintern_readbyte(TinExecState* est)
{
    return (*est->ip++);
}

TIN_VM_INLINE TinValue tin_vmintern_readconstant(TinExecState* est)
{
    return tin_vallist_get(&est->currentchunk->constants, tin_vmintern_readbyte(est));
}

TIN_VM_INLINE TinValue tin_vmintern_readconstantlong(TinExecState* est)
{
    return tin_vallist_get(&est->currentchunk->constants, tin_vmintern_readshort(est));
}

TIN_VM_INLINE TinString* tin_vmintern_readstring(TinExecState* est)
{
    return tin_value_asstring(tin_vmintern_readconstant(est));
}

TIN_VM_INLINE TinString* tin_vmintern_readstringlong(TinExecState* est)
{
    return tin_value_asstring(tin_vmintern_readconstantlong(est));
}


TIN_VM_INLINE void tin_vmintern_push(TinExecState* est, TinValue v)
{
    *est->fiber->stacktop++ = v;
}

TIN_VM_INLINE TinValue tin_vmintern_pop(TinExecState* est)
{
    return *(--est->fiber->stacktop);
}

TIN_VM_INLINE void tin_vmintern_drop(TinExecState* est)
{
    est->fiber->stacktop--;
}

TIN_VM_INLINE void tin_vmintern_dropn(TinExecState* est, int amount)
{
    est->fiber->stacktop -= amount;
}

TIN_VM_INLINE TinValue tin_vmintern_peek(TinExecState* est, short distance)
{
    int ofs;
    ofs = ((-1) - distance);
    return est->fiber->stacktop[ofs];
}

TIN_VM_INLINE void tin_vmintern_readframe(TinExecState* est)
{
    est->frame = &est->fiber->framevalues[est->fiber->framecount - 1];
    if(est->frame->function == NULL)
    {
        est->frame->function = (&est->fiber->framevalues[0])->function;
    }
    est->currentchunk = &est->frame->function->chunk;
    est->ip = est->frame->ip;
    est->slots = est->frame->slots;
    est->fiber->module = est->frame->function->module;
    est->privates = est->fiber->module->privates;
    est->upvalues = est->frame->closure == NULL ? NULL : est->frame->closure->upvalues;
}

TIN_VM_INLINE void tin_vmintern_writeframe(TinExecState* est, uint8_t* ip)
{
    est->frame->ip = ip;
}

void tin_vmintern_resetstack(TinVM* vm)
{
    if(vm->fiber != NULL)
    {
        vm->fiber->stacktop = vm->fiber->stackvalues;
    }
}

void tin_vmintern_resetvm(TinState* state, TinVM* vm)
{
    vm->state = state;
    vm->gcobjects = NULL;
    vm->fiber = NULL;
    vm->gcgraystack = NULL;
    vm->gcgraycount = 0;
    vm->gcgraycapacity = 0;
    tin_strreg_init(vm->state);
    vm->globals = NULL;
    vm->modules = NULL;
}

void tin_vm_init(TinState* state, TinVM* vm)
{
    tin_vmintern_resetvm(state, vm);
    vm->globals = tin_object_makemap(state);
    vm->modules = tin_object_makemap(state);
}

void tin_vm_destroy(TinVM* vm)
{
    tin_strreg_destroy(vm->state);
    tin_object_destroylistof(vm->state, vm->gcobjects);
    tin_vmintern_resetvm(vm->state, vm);
}

void tin_vm_callexitjump(TinVM* vm)
{
    (void)vm;
    longjmp(jumpbuffer, 1);
}

bool tin_vm_setexitjump(TinVM* vm)
{
    (void)vm;
    return setjmp(jumpbuffer);
}

void tin_vmintern_tracestack(TinVM* vm, TinWriter* wr)
{
    TinValue* top;
    TinValue* slot;
    TinFiber* fiber;
    fiber = vm->fiber;
    if(fiber->stacktop == fiber->stackvalues || fiber->framecount == 0)
    {
        return;
    }
    top = fiber->framevalues[fiber->framecount - 1].slots;
    tin_writer_writeformat(wr, "        | %s", COLOR_GREEN);
    for(slot = fiber->stackvalues; slot < top; slot++)
    {
        tin_writer_writeformat(wr, "[ ");
        tin_towriter_value(vm->state, wr, *slot, true);
        tin_writer_writeformat(wr, " ]");
    }
    tin_writer_writeformat(wr, "%s", COLOR_RESET);
    for(slot = top; slot < fiber->stacktop; slot++)
    {
        tin_writer_writeformat(wr, "[ ");
        tin_towriter_value(vm->state, wr, *slot, true);
        tin_writer_writeformat(wr, " ]");
    }
    tin_writer_writeformat(wr, "\n");
}

bool tin_vm_handleruntimeerror(TinVM* vm, TinString* errorstring)
{
    int i;
    int count;
    bool haschunk;
    size_t length;
    char* start;
    char* buffer;
    const char* name;
    TinCallFrame* frame;
    TinFunction* function;
    TinChunk* chunk;
    TinValue errval;
    TinFiber* fiber;
    TinFiber* caller;
    errval = tin_value_fromobject(errorstring);
    fiber = vm->fiber;
    while(fiber != NULL)
    {
        fiber->errorval = errval;
        if(fiber->catcher)
        {
            vm->fiber = fiber->parent;
            vm->fiber->stacktop -= fiber->funcargcount;
            vm->fiber->stacktop[-1] = errval;
            return true;
        }
        caller = fiber->parent;
        fiber->parent = NULL;
        fiber = caller;
    }
    fiber = vm->fiber;
    fiber->abort = true;
    fiber->errorval = errval;
    if(fiber->parent != NULL)
    {
        fiber->parent->abort = true;
    }
    // Maan, formatting c strings is hard...
    count = (int)fiber->framecount - 1;
    length = snprintf(NULL, 0, "%s%s\n", COLOR_RED, errorstring->data);
    for(i = count; i >= 0; i--)
    {
        frame = &fiber->framevalues[i];
        function = frame->function;
        chunk = &function->chunk;
        name = "unknown";
        haschunk = false;
        if(function != NULL)
        {
            if(function->name != NULL)
            {
                haschunk = true;
                name = function->name->data;
            }
        }
        if(haschunk && chunk->haslineinfo)
        {
            length += snprintf(NULL, 0, "[line %d] in %s()\n", (int)tin_chunk_getline(chunk, frame->ip - chunk->code - 1), name);
        }
        else
        {
            length += snprintf(NULL, 0, "\tin %s()\n", name);
        }
    }
    length += snprintf(NULL, 0, "%s", COLOR_RESET);
    buffer = (char*)malloc(length + 1);
    buffer[length] = '\0';
    start = buffer + sprintf(buffer, "%s%s\n", COLOR_RED, errorstring->data);
    for(i = count; i >= 0; i--)
    {
        frame = &fiber->framevalues[i];
        function = frame->function;
        chunk = &function->chunk;
        haschunk = false;
        name = "unknown";
        if(function != NULL)
        {
            if(function->name != NULL)
            {
                haschunk = true;
                name = function->name->data;
            }
        }
        if(haschunk && chunk->haslineinfo)
        {
            start += sprintf(start, "[line %d] in %s()\n", (int)tin_chunk_getline(chunk, frame->ip - chunk->code - 1), name);
        }
        else
        {
            start += sprintf(start, "\tin %s()\n", name);
        }
    }
    start += sprintf(start, "%s", COLOR_RESET);
    tin_state_raiseerror(vm->state, RUNTIME_ERROR, buffer);
    free(buffer);
    tin_vmintern_resetstack(vm);
    return false;
}

bool tin_vm_vraiseerror(TinVM* vm, const char* format, va_list args)
{
    size_t buffersize;
    char* buffer;
    va_list argscopy;
    va_copy(argscopy, args);
    buffersize = vsnprintf(NULL, 0, format, argscopy) + 1;
    va_end(argscopy);
    buffer = (char*)malloc(buffersize+1);
    vsnprintf(buffer, buffersize, format, args);
    return tin_vm_handleruntimeerror(vm, tin_string_take(vm->state, buffer, buffersize, false));
}

bool tin_vm_raiseerror(TinVM* vm, const char* format, ...)
{
    bool result;
    va_list args;
    va_start(args, format);
    result = tin_vm_vraiseerror(vm, format, args);
    va_end(args);
    return result;
}

bool tin_vm_raiseexitingerror(TinVM* vm, const char* format, ...)
{
    bool result;
    va_list args;
    va_start(args, format);
    result = tin_vm_vraiseerror(vm, format, args);
    va_end(args);
    tin_vm_callexitjump(vm);
    return result;
}

bool tin_vm_callcallable(TinVM* vm, TinFunction* function, TinClosure* closure, uint8_t argc)
{
    bool vararg;
    size_t amount;
    size_t i;
    size_t osize;
    size_t newcapacity;
    size_t newsize;
    size_t varargcount;
    size_t functionargcount;
    TinCallFrame* frame;
    TinFiber* fiber;
    TinArray* array;
    fiber = vm->fiber;

    #if 0
    //if(fiber->framecount == TIN_CALL_FRAMES_MAX)
    //{
        //tin_vm_raiseerror(vm, "call stack overflow");
        //return true;
    //}
    #endif
    if(fiber->framecount + 1 > fiber->framecap)
    {
        //newcapacity = fmin(TIN_CALL_FRAMES_MAX, fiber->framecap * 2);
        newcapacity = (fiber->framecap * 2);
        newsize = (sizeof(TinCallFrame) * newcapacity);
        osize = (sizeof(TinCallFrame) * fiber->framecap);
        fiber->framevalues = (TinCallFrame*)tin_gcmem_memrealloc(vm->state, fiber->framevalues, osize, newsize);
        fiber->framecap = newcapacity;
    }

    functionargcount = function->argcount;
    tin_fiber_ensurestack(vm->state, fiber, function->maxslots + (int)(fiber->stacktop - fiber->stackvalues));
    frame = &fiber->framevalues[fiber->framecount++];
    frame->function = function;
    frame->closure = closure;
    frame->ip = function->chunk.code;
    frame->slots = fiber->stacktop - argc - 1;
    frame->ignresult = false;
    frame->returntonative = false;
    if(argc != functionargcount)
    {
        vararg = function->vararg;
        if(argc < functionargcount)
        {
            amount = (int)functionargcount - argc - (vararg ? 1 : 0);
            for(i = 0; i < amount; i++)
            {
                tin_vm_push(vm, tin_value_makenull(vm->state));
            }
            if(vararg)
            {
                tin_vm_push(vm, tin_value_fromobject(tin_object_makearray(vm->state)));
            }
        }
        else if(function->vararg)
        {
            array = tin_object_makearray(vm->state);
            varargcount = argc - functionargcount + 1;
            tin_state_pushroot(vm->state, (TinObject*)array);
            tin_vallist_ensuresize(vm->state, &array->list, varargcount);
            tin_state_poproot(vm->state);
            for(i = 0; i < varargcount; i++)
            {
                tin_vallist_set(vm->state, &array->list, i, vm->fiber->stacktop[(int)i - (int)varargcount]);
            }
            vm->fiber->stacktop -= varargcount;
            tin_vm_push(vm, tin_value_fromobject(array));
        }
        else
        {
            vm->fiber->stacktop -= (argc - functionargcount);
        }
    }
    else if(function->vararg)
    {
        array = tin_object_makearray(vm->state);
        varargcount = argc - functionargcount + 1;
        tin_state_pushroot(vm->state, (TinObject*)array);
        tin_vallist_push(vm->state, &array->list, *(fiber->stacktop - 1));
        *(fiber->stacktop - 1) = tin_value_fromobject(array);
        tin_state_poproot(vm->state);
    }
    return true;
}

const char* tin_vmintern_funcnamefromvalue(TinExecState* est, TinValue v)
{
    TinValue vn;
    (void)est;
    vn = tin_function_getname(est->vm, v);
    if(!tin_value_isnull(vn))
    {
        return tin_value_ascstring(vn);
    }
    return "unknown";
}

bool tin_vm_callvalue(TinExecState* est, TinValue callee, TinString* name, uint8_t argc)
{
    size_t i;
    bool bres;
    const char* fname;
    TinValue mthval;
    TinValue result;
    TinValue fromval;
    TinNativeMethod* mthobj;
    TinFiber* valfiber;
    TinClosure* closure;
    TinBoundMethod* boundmethod;
    TinInstance* instance;
    TinClass* klass;
    (void)valfiber;
    if(tin_value_isobject(callee))
    {
        if(tin_vm_setexitjump(est->vm))
        {
            return true;
        }
        switch(tin_value_type(callee))
        {
            case TINTYPE_FUNCTION:
                {
                    return tin_vm_callcallable(est->vm, tin_value_asfunction(callee), NULL, argc);
                }
                break;
            case TINTYPE_CLOSURE:
                {
                    closure = tin_value_asclosure(callee);
                    return tin_vm_callcallable(est->vm, closure->function, closure, argc);
                }
                break;
            case TINTYPE_NATIVEFUNCTION:
                {
                    tin_vmmac_pushgc(est, false);
                    result = tin_value_asnativefunction(callee)->function(est->vm, argc, est->vm->fiber->stacktop - argc);
                    est->vm->fiber->stacktop -= argc + 1;
                    tin_vm_push(est->vm, result);
                    tin_vmmac_popgc(est);
                    return false;
                }
                break;
            case TINTYPE_NATIVEPRIMITIVE:
                {
                    tin_vmmac_pushgc(est, false);
                    est->fiber = est->vm->fiber;
                    bres = tin_value_asnativeprimitive(callee)->function(est->vm, argc, est->fiber->stacktop - argc);
                    if(bres)
                    {
                        est->fiber->stacktop -= argc;
                    }
                    tin_vmmac_popgc(est);
                    return bres;
                }
                break;
            case TINTYPE_NATIVEMETHOD:
                {
                    tin_vmmac_pushgc(est, false);
                    mthobj = tin_value_asnativemethod(callee);
                    est->fiber = est->vm->fiber;
                    result = mthobj->method(est->vm, *(est->vm->fiber->stacktop - argc - 1), argc, est->vm->fiber->stacktop - argc);
                    est->vm->fiber->stacktop -= argc + 1;
                    //if(!tin_value_isnull(result))
                    {
                        if(!est->vm->fiber->abort)
                        {
                            tin_vm_push(est->vm, result);
                        }
                    }
                    tin_vmmac_popgc(est);
                    return false;
                }
                break;
            case TINTYPE_PRIMITIVEMETHOD:
                {
                    tin_vmmac_pushgc(est, false);
                    est->fiber = est->vm->fiber;
                    bres = tin_value_asprimitivemethod(callee)->method(est->vm, *(est->fiber->stacktop - argc - 1), argc, est->fiber->stacktop - argc);
                    if(bres)
                    {
                        est->fiber->stacktop -= argc;
                    }
                    tin_vmmac_popgc(est);
                    return bres;
                }
                break;
            case TINTYPE_CLASS:
                {
                    klass = tin_value_asclass(callee);
                    instance = tin_object_makeinstance(est->vm->state, klass);
                    est->vm->fiber->stacktop[-argc - 1] = tin_value_fromobject(instance);
                    if(klass->initmethod != NULL)
                    {
                        return tin_vm_callvalue(est, tin_value_fromobject(klass->initmethod), klass->name, argc);
                    }
                    // Remove the arguments, so that they don't mess up the stack
                    // (default constructor has no arguments)
                    for(i = 0; i < argc; i++)
                    {
                        tin_vm_pop(est->vm);
                    }
                    return false;
                }
                break;
            case TINTYPE_BOUNDMETHOD:
                {
                    boundmethod = tin_value_asboundmethod(callee);
                    mthval = boundmethod->method;
                    if(tin_value_isnatmethod(mthval))
                    {
                        tin_vmmac_pushgc(est, false);
                        result = tin_value_asnativemethod(mthval)->method(est->vm, boundmethod->receiver, argc, est->vm->fiber->stacktop - argc);
                        est->vm->fiber->stacktop -= argc + 1;
                        tin_vm_push(est->vm, result);
                        tin_vmmac_popgc(est);
                        return false;
                    }
                    else if(tin_value_isprimmethod(mthval))
                    {
                        est->fiber = est->vm->fiber;
                        tin_vmmac_pushgc(est, false);
                        if(tin_value_asprimitivemethod(mthval)->method(est->vm, boundmethod->receiver, argc, est->fiber->stacktop - argc))
                        {
                            est->fiber->stacktop -= argc;
                            return true;
                        }
                        tin_vmmac_popgc(est);
                        return false;
                    }
                    else
                    {
                        est->vm->fiber->stacktop[-argc - 1] = boundmethod->receiver;
                        return tin_vm_callcallable(est->vm, tin_value_asfunction(mthval), NULL, argc);
                    }
                }
                break;
            default:
                {
                }
                break;

        }
    }
    fromval = callee;
    if(tin_value_isnull(fromval))
    {
        /*
        if(est->frame->function != NULL)
        {
            fromval =  tin_value_fromobject(est->frame->function);
        }
        */
        //fromval = est->slots[0];
        fromval = tin_vmintern_peek(est, 0);
    }
    fname = name->data;

    if(tin_value_isnull(callee))
    {
        tin_vm_raiseerror(est->vm, "attempt to call '%s' which is null", fname);
    }
    else
    {
        tin_vm_raiseerror(est->vm, "attempt to call '%s' which is neither function nor class, but is %s", fname, tin_tostring_typename(callee));
    }
    return true;
}

TinUpvalue* tin_vmintern_captureupvalue(TinState* state, TinValue* local)
{
    TinUpvalue* upvalue;
    TinUpvalue* createdupvalue;
    TinUpvalue* previousupvalue;
    previousupvalue = NULL;
    upvalue = state->vm->fiber->openupvalues;
    while(upvalue != NULL && upvalue->location > local)
    {
        previousupvalue = upvalue;
        upvalue = upvalue->next;
    }
    if(upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }
    createdupvalue = tin_object_makeupvalue(state, local);
    createdupvalue->next = upvalue;
    if(previousupvalue == NULL)
    {
        state->vm->fiber->openupvalues = createdupvalue;
    }
    else
    {
        previousupvalue->next = createdupvalue;
    }
    return createdupvalue;
}

void tin_vmintern_closeupvalues(TinVM* vm, const TinValue* last)
{
    TinFiber* fiber;
    TinUpvalue* upvalue;
    fiber = vm->fiber;
    while(fiber->openupvalues != NULL && fiber->openupvalues->location >= last)
    {
        upvalue = fiber->openupvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        fiber->openupvalues = upvalue->next;
    }
}

TinInterpretResult tin_vm_execmodule(TinState* state, TinModule* module)
{
    TinVM* vm;
    TinFiber* fiber;
    TinInterpretResult result;
    vm = state->vm;
    fiber = tin_object_makefiber(state, module, module->mainfunction);
    vm->fiber = fiber;
    tin_vm_push(vm, tin_value_fromobject(module->mainfunction));
    result = tin_vm_execfiber(state, fiber);
    return result;
}

TIN_VM_INLINE const char* vmutil_op2s(int op)
{
    switch(op)
    {
        case OP_MATHADD: return "+";
        case OP_MATHSUB: return "-";
        case OP_MATHMULT: return "*";
        case OP_MATHPOWER: return "**";
        case OP_MATHMOD: return "%";
        case OP_MATHDIV: return "/";
        case OP_BINAND: return "&";
        case OP_BINOR: return "|";
        case OP_BINXOR: return "^";
        case OP_LEFTSHIFT: return "<<";
        case OP_RIGHTSHIFT: return ">>";
        case OP_EQUAL: return "==";
        case OP_GREATERTHAN: return ">";
        case OP_GREATEREQUAL: return ">=";
        case OP_LESSTHAN: return "<";
        case OP_LESSEQUAL: return "<=";
        default:
            break;
    }
    return "??";
}

TIN_VM_INLINE int vmutil_numtoint32(TinValue val)
{
    if(tin_value_isnull(val))
    {
        return 0;
    }
    if(tin_value_isbool(val))
    {
        return val.boolval;
    }
    if(val.isfixednumber)
    {
        return val.numfixedval;
    }
    return tin_util_numbertoint32(val.numfloatval);
}

TIN_VM_INLINE unsigned int vmutil_numtouint32(TinValue val)
{
    if(tin_value_isnull(val))
    {
        return 0;
    }
    if(tin_value_isbool(val))
    {
        return val.boolval;
    }
    if(val.isfixednumber)
    {
        return val.numfixedval;
    }
    return tin_util_numbertouint32(val.numfloatval);
}

TIN_VM_INLINE bool vm_binaryop_actual(TinExecState* est, int op, TinValue a, TinValue b)
{
    int64_t ia;
    int64_t ib;
    bool eq;
    TinValue res;
    switch(op)
    {
        case OP_MATHMOD:
            {
                if(a.isfixednumber && b.isfixednumber)
                {
                    res = tin_value_makefixednumber(est->vm->state, tin_value_asfixednumber(a) % tin_value_asfixednumber(b));
                }
                else if(!a.isfixednumber && !b.isfixednumber)
                {
                    res = tin_value_makefloatnumber(est->vm->state, fmod(tin_value_asfloatnumber(a),  tin_value_asfloatnumber(b)));
                }
                else
                {
                    res = tin_value_makefloatnumber(est->vm->state, tin_value_asfixednumber(a) % tin_value_asfixednumber(b));
                }
                *(est->fiber->stacktop - 1) = res;

            }
            break;
        case OP_MATHPOWER:
            {
                *(est->fiber->stacktop - 1) = tin_value_makefloatnumber(est->vm->state, pow(tin_value_asnumber(a), tin_value_asnumber(b)));
            }
            break;
        case OP_MATHADD:
            {
                if(a.isfixednumber && b.isfixednumber)
                {
                    res = tin_value_makefixednumber(est->vm->state, tin_value_asfixednumber(a) + tin_value_asfixednumber(b));
                }
                else if(!a.isfixednumber && !b.isfixednumber)
                {
                    res = tin_value_makefloatnumber(est->vm->state, tin_value_asfloatnumber(a) + tin_value_asfloatnumber(b));
                }
                else
                {
                    res = tin_value_makefloatnumber(est->vm->state, tin_value_asnumber(a) + tin_value_asnumber(b));
                }
                *(est->fiber->stacktop - 1) = res;
            }
            break;
        case OP_MATHSUB:
            {
                if(a.isfixednumber && b.isfixednumber)
                {
                    res = tin_value_makefixednumber(est->vm->state, tin_value_asfixednumber(a) - tin_value_asfixednumber(b));
                }
                else if(!a.isfixednumber && !b.isfixednumber)
                {
                    res = tin_value_makefloatnumber(est->vm->state, tin_value_asfloatnumber(a) - tin_value_asfloatnumber(b));
                }
                else
                {
                    res = tin_value_makefloatnumber(est->vm->state, tin_value_asnumber(a) - tin_value_asnumber(b));
                }
                *(est->fiber->stacktop - 1) = res;
            }
            break;
        case OP_MATHMULT:
            {
                if(a.isfixednumber && b.isfixednumber)
                {
                    res = tin_value_makefixednumber(est->vm->state, tin_value_asfixednumber(a) * tin_value_asfixednumber(b));
                }
                else if(!a.isfixednumber && !b.isfixednumber)
                {
                    res = tin_value_makefloatnumber(est->vm->state, tin_value_asfloatnumber(a) * tin_value_asfloatnumber(b));
                }
                else
                {
                    res = tin_value_makefloatnumber(est->vm->state, tin_value_asnumber(a) * tin_value_asnumber(b));
                }
                *(est->fiber->stacktop - 1) = res;
            }
            break;
        case OP_MATHDIV:
            {
                if(a.isfixednumber && b.isfixednumber)
                {
                    res = tin_value_makefixednumber(est->vm->state, tin_value_asfixednumber(a) / tin_value_asfixednumber(b));
                }
                else if(!a.isfixednumber && !b.isfixednumber)
                {
                    res = tin_value_makefloatnumber(est->vm->state, tin_value_asfloatnumber(a) / tin_value_asfloatnumber(b));
                }
                else
                {
                    res = tin_value_makefloatnumber(est->vm->state, tin_value_asnumber(a) / tin_value_asnumber(b));
                }
                *(est->fiber->stacktop - 1) = res;
            }
            break;
        case OP_BINAND:
            {
                ia = tin_value_asfixednumber(a);
                ib = tin_value_asfixednumber(b);
                *(est->fiber->stacktop - 1) = (tin_value_makefixednumber(est->vm->state, ia & ib));
            }
            break;
        case OP_BINOR:
            {
                ia = tin_value_asfixednumber(a);
                ib = tin_value_asfixednumber(b);
                *(est->fiber->stacktop - 1) = (tin_value_makefixednumber(est->vm->state, ia | ib));
            }
            break;
        case OP_BINXOR:
            {
                ia = tin_value_asfixednumber(a);
                ib = tin_value_asfixednumber(b);
                *(est->fiber->stacktop - 1) = (tin_value_makefixednumber(est->vm->state, ia ^ ib));
            }
            break;
        case OP_LEFTSHIFT:
            {
                int ires;
                int uleft;
                unsigned int uright;
                uleft = vmutil_numtoint32(a);
                uright = vmutil_numtouint32(b);
                if(!b.isfixednumber)
                {
                    ires = uleft << (uright & 0x1F);
                }
                else
                {
                    ires = uleft << uright;
                }
                *(est->fiber->stacktop - 1) = tin_value_makefixednumber(est->vm->state, ires);
            }
            break;
        case OP_RIGHTSHIFT:
            {
                int ires;
                int uleft;
                unsigned int uright;
                uleft = vmutil_numtoint32(a);
                uright = vmutil_numtouint32(b);
                if(!b.isfixednumber)
                {
                    ires = uleft >> (uright & 0x1F);
                }
                else
                {
                    ires = uleft >> uright;
                }
                *(est->fiber->stacktop - 1) = tin_value_makefixednumber(est->vm->state, ires);
            }
            break;
        case OP_EQUAL:
            {
                eq = false;
                if(a.isfixednumber && b.isfixednumber)
                {
                    eq = (tin_value_asfixednumber(a) == tin_value_asfixednumber(b));
                }
                else if(!a.isfixednumber && !b.isfixednumber)
                {
                    eq = (tin_value_asfloatnumber(a) == tin_value_asfloatnumber(b));
                }
                else
                {
                    eq = (tin_value_asnumber(a) == tin_value_asnumber(b));
                }
                *(est->fiber->stacktop - 1) = tin_value_makebool(est->vm->state, eq);
            }
            break;
        case OP_GREATERTHAN:
            {
                *(est->fiber->stacktop - 1) = (tin_value_makebool(est->vm->state, tin_value_asnumber(a) > tin_value_asnumber(b)));
            }
            break;
        case OP_GREATEREQUAL:
            {
                *(est->fiber->stacktop - 1) = (tin_value_makebool(est->vm->state, tin_value_asnumber(a) >= tin_value_asnumber(b)));
            }
            break;
        case OP_LESSTHAN:
            {
                *(est->fiber->stacktop - 1) = (tin_value_makebool(est->vm->state, tin_value_asnumber(a) < tin_value_asnumber(b)));
            }
            break;
        case OP_LESSEQUAL:
            {
                *(est->fiber->stacktop - 1) = (tin_value_makebool(est->vm->state, tin_value_asnumber(a) <= tin_value_asnumber(b)));
            }
            break;
        default:
            {
                fprintf(stderr, "unhandled instruction in vm_binaryop_actual\n");
                return false;
            }
            break;
    }
    return true;
}

// OP_CALLFUNCTION
TIN_VM_INLINE bool tin_vmdo_call(TinExecState* est, TinValue* finalresult)
{
    size_t argc;
    TinValue peeked;
    TinString* name;
    argc = tin_vmintern_readbyte(est);
    tin_vmintern_writeframe(est, est->ip);
    name = tin_vmintern_readstringlong(est);
    peeked = tin_vmintern_peek(est, argc);
    tin_vmmac_callvalue(peeked, name, argc);
    return true;
}

// OP_FIELDGET
TIN_VM_INLINE bool tin_vmdo_fieldget(TinExecState* est, TinValue* finalresult)
{
    TinValue tmpval;
    TinValue object;
    TinValue getval;
    TinString* name;
    TinClass* klassobj;
    TinField* field;
    TinInstance* instobj;
    object = tin_vmintern_peek(est, 1);
    name = tin_value_asstring(tin_vmintern_peek(est, 0));
    if(tin_value_isnull(object))
    {
        tin_vmmac_raiseerrorfmtnocont("attempt to set field '%s' of a null value", name->data);
    }
    if(tin_value_isinstance(object))
    {
        instobj = tin_value_asinstance(object);
        if(!tin_table_get(&instobj->fields, name, &getval))
        {
            if(tin_table_get(&instobj->klass->methods, name, &getval))
            {
                if(tin_value_isfield(getval))
                {
                    field = tin_value_asfield(getval);
                    if(field->getter == NULL)
                    {
                        tin_vmmac_raiseerrorfmtnocont("class %s does not have a getter for field '%s'",
                                           instobj->klass->name->data, name->data);
                    }
                    tin_vmintern_drop(est);
                    tin_vmintern_writeframe(est, est->ip);
                    field = tin_value_asfield(getval);
                    tmpval =tin_value_fromobject(field->getter);
                    tin_vmmac_callvalue(tmpval, name, 0);
                    tin_vmintern_readframe(est);
                    return true;
                }
                else
                {
                    getval = tin_value_fromobject(tin_object_makeboundmethod(est->state, tin_value_fromobject(instobj), getval));
                }
            }
            else
            {
                getval = tin_value_makenull(est->vm->state);
            }
        }
    }
    else if(tin_value_isclass(object))
    {
        klassobj = tin_value_asclass(object);
        if(tin_table_get(&klassobj->staticfields, name, &getval))
        {
            if(tin_value_isnatmethod(getval) || tin_value_isprimmethod(getval))
            {
                getval = tin_value_fromobject(tin_object_makeboundmethod(est->state, tin_value_fromobject(klassobj), getval));
            }
            else if(tin_value_isfield(getval))
            {
                field = tin_value_asfield(getval);
                if(field->getter == NULL)
                {
                    tin_vmmac_raiseerrorfmtnocont("class %s does not have a getter for field '%s'", klassobj->name->data,
                                       name->data);
                }
                tin_vmintern_drop(est);
                tin_vmintern_writeframe(est, est->ip);
                tmpval = tin_value_fromobject(field->getter);
                tin_vmmac_callvalue(tmpval, name, 0);
                tin_vmintern_readframe(est);
                return true;
            }
        }
        else
        {
            getval = tin_value_makenull(est->vm->state);
        }
    }
    else
    {
        klassobj = tin_state_getclassfor(est->state, object);
        if(klassobj == NULL)
        {
            tin_vmmac_raiseerrorfmtnocont("GET_FIELD: cannot get class object for type '%s'", tin_tostring_typename(object));
        }
        if(tin_table_get(&klassobj->methods, name, &getval))
        {
            if(tin_value_isfield(getval))
            {
                field = tin_value_asfield(getval);
                if(field->getter == NULL)
                {
                    tin_vmmac_raiseerrorfmtnocont("class %s does not have a getter for field '%s'", klassobj->name->data,
                                       name->data);
                }
                tin_vmintern_drop(est);
                tin_vmintern_writeframe(est, est->ip);
                tmpval = tin_value_fromobject(tin_value_asfield(getval)->getter);
                tin_vmmac_callvalue(tmpval, name, 0);
                tin_vmintern_readframe(est);
                return true;
            }
            else if(tin_value_isnatmethod(getval) || tin_value_isprimmethod(getval))
            {
                getval = tin_value_fromobject(tin_object_makeboundmethod(est->state, object, getval));
            }
        }
        else
        {
            getval = tin_value_makenull(est->vm->state);
        }
    }
    tin_vmintern_drop(est);// Pop field name
    est->fiber->stacktop[-1] = getval;
    return true;
}

// OP_FIELDSET
TIN_VM_INLINE bool tin_vmdo_fieldset(TinExecState* est, TinValue* finalresult)
{
    TinValue value;
    TinValue tmpval;
    TinValue setter;
    TinValue instval;
    TinClass* klassobj;
    TinField* field;
    TinString* fieldname;
    TinInstance* instobj;
    instval = tin_vmintern_peek(est, 2);
    value = tin_vmintern_peek(est, 1);
    fieldname = tin_value_asstring(tin_vmintern_peek(est, 0));
    if(tin_value_isnull(instval))
    {
        tin_vmmac_raiseerrorfmtnocont("attempt to set field '%s' of a null value", fieldname->data)
    }
    if(tin_value_isclass(instval))
    {
        klassobj = tin_value_asclass(instval);
        if(tin_table_get(&klassobj->staticfields, fieldname, &setter) && tin_value_isfield(setter))
        {
            field = tin_value_asfield(setter);
            if(field->setter == NULL)
            {
                tin_vmmac_raiseerrorfmtnocont("class %s does not have a setter for field '%s'", klassobj->name->data,
                                   fieldname->data);
            }

            tin_vmintern_dropn(est, 2);
            tin_vmintern_push(est, value);
            tin_vmintern_writeframe(est, est->ip);
            tmpval = tin_value_fromobject(field->setter);
            tin_vmmac_callvalue(tmpval, fieldname, 1);
            tin_vmintern_readframe(est);
            return true;
        }
        /*
        if(tin_value_isnull(value))
        {
            tin_table_delete(&klassobj->staticfields, fieldname);
        }
        else
        {
            tin_table_set(est->state, &klassobj->staticfields, fieldname, value);
        }
        */
                    tin_table_set(est->state, &klassobj->staticfields, fieldname, value);

        tin_vmintern_dropn(est, 2);// Pop field name and the value
        est->fiber->stacktop[-1] = value;
    }
    else if(tin_value_isinstance(instval))
    {
        instobj = tin_value_asinstance(instval);
        if(tin_table_get(&instobj->klass->methods, fieldname, &setter) && tin_value_isfield(setter))
        {
            field = tin_value_asfield(setter);
            if(field->setter == NULL)
            {
                tin_vmmac_raiseerrorfmtnocont("class %s does not have a setter for field '%s'", instobj->klass->name->data,
                                   fieldname->data);
            }
            tin_vmintern_dropn(est, 2);
            tin_vmintern_push(est, value);
            tin_vmintern_writeframe(est, est->ip);
            tmpval = tin_value_fromobject(field->setter);
            tin_vmmac_callvalue(tmpval, fieldname, 1);
            tin_vmintern_readframe(est);
            return true;
        }
        if(tin_value_isnull(value))
        {
            tin_table_delete(&instobj->fields, fieldname);
        }
        else
        {
            tin_table_set(est->state, &instobj->fields, fieldname, value);
        }
        tin_vmintern_dropn(est, 2);// Pop field name and the value
        est->fiber->stacktop[-1] = value;
    }
    else
    {
        klassobj = tin_state_getclassfor(est->state, instval);
        if(klassobj == NULL)
        {
            tin_vmmac_raiseerrorfmtnocont("SET_FIELD: only instances and classes have fields", 0);
        }
        if(tin_table_get(&klassobj->methods, fieldname, &setter) && tin_value_isfield(setter))
        {
            field = tin_value_asfield(setter);
            if(field->setter == NULL)
            {
                tin_vmmac_raiseerrorfmtnocont("class '%s' does not have a setter for field '%s'", klassobj->name->data,
                                   fieldname->data);
            }
            tin_vmintern_dropn(est, 2);
            tin_vmintern_push(est, value);
            tin_vmintern_writeframe(est, est->ip);
            tmpval = tin_value_fromobject(field->setter);
            tin_vmmac_callvalue(tmpval, fieldname, 1);
            tin_vmintern_readframe(est);
            return true;
        }
        else
        {
            tin_vmmac_raiseerrorfmtnocont("class '%s' does not contain field '%s'", klassobj->name->data, fieldname->data);
        }
    }
    return true;
}

// OP_RANGE
TIN_VM_INLINE bool tin_vmdo_range(TinExecState* est, TinValue* finalresult)
{
    TinValue vala;
    TinValue valb;
    vala = tin_vmintern_pop(est);
    valb = tin_vmintern_pop(est);
    if(!tin_value_isnumber(vala) || !tin_value_isnumber(valb))
    {
        tin_vmmac_raiseerrorfmtnocont("range fields must be numbers", 0);
    }
    tin_vmintern_push(est, tin_value_fromobject(tin_object_makerange(est->state, tin_value_asnumber(vala), tin_value_asnumber(valb))));
    return true;
}

// OP_MAKECLOSURE
TIN_VM_INLINE bool tin_vmdo_makeclosure(TinExecState* est, TinValue* finalresult)
{
    size_t i;
    uint8_t index;
    uint8_t islocal;
    TinClosure* closure;
    TinFunction* function;
    (void)finalresult;
    function = tin_value_asfunction(tin_vmintern_readconstantlong(est));
    closure = tin_object_makeclosure(est->state, function);
    tin_vmintern_push(est, tin_value_fromobject(closure));
    for(i = 0; i < closure->upvalcount; i++)
    {
        islocal = tin_vmintern_readbyte(est);
        index = tin_vmintern_readbyte(est);
        if(islocal)
        {
            closure->upvalues[i] = tin_vmintern_captureupvalue(est->state, est->frame->slots + index);
        }
        else
        {
            closure->upvalues[i] = est->upvalues[index];
        }
    }
    return true;
}

// OP_MAKECLASS
TIN_VM_INLINE bool tin_vmdo_makeclass(TinExecState* est, TinValue* finalresult)
{
    TinString* name;
    TinClass* klassobj;
    (void)finalresult;
    name = tin_vmintern_readstringlong(est);
    klassobj = tin_object_makeclass(est->state, name);
    tin_vmintern_push(est, tin_value_fromobject(klassobj));
    klassobj->parentclass = est->state->primobjectclass;
    tin_table_add_all(est->state, &klassobj->parentclass->methods, &klassobj->methods);
    tin_table_add_all(est->state, &klassobj->parentclass->staticfields, &klassobj->staticfields);
    tin_table_set(est->state, &est->vm->globals->values, name, tin_value_fromobject(klassobj));
    return true;
}

// OP_MAKEMETHOD
TIN_VM_INLINE bool tin_vmdo_makemethod(TinExecState* est, TinValue* finalresult)
{
    size_t ctorlen;
    const char* ctorname;
    TinString* name;
    TinClass* klassobj;
    (void)finalresult;
    ctorname = TIN_VALUE_CTORNAME;
    ctorlen = strlen(ctorname);
    klassobj = tin_value_asclass(tin_vmintern_peek(est, 1));
    name = tin_vmintern_readstringlong(est);
    if((klassobj->initmethod == NULL || (klassobj->parentclass != NULL && klassobj->initmethod == ((TinClass*)klassobj->parentclass)->initmethod))
       && tin_string_getlength(name) == ctorlen && memcmp(name->data, ctorname, ctorlen) == 0)
    {
        klassobj->initmethod = tin_value_asobject(tin_vmintern_peek(est, 0));
    }
    tin_table_set(est->state, &klassobj->methods, name, tin_vmintern_peek(est, 0));
    tin_vmintern_drop(est);
    return true;
}

// OP_OBJECTPUSHFIELD
TIN_VM_INLINE bool tin_vmdo_objectpushfield(TinExecState* est, TinValue* finalresult)
{
    TinValue operand;
    TinValue peek0;
    TinValue peek1;
    TinMap* tmap;
    TinInstance* tinst;
    operand = tin_vmintern_peek(est, 2);
    peek0 = tin_vmintern_peek(est, 0);
    peek1 = tin_vmintern_peek(est, 1);
    if(tin_value_ismap(operand))
    {
        tmap = tin_value_asmap(operand);
        //fprintf(stderr, "peek1=%s\n", tin_tostring_typename(peek1));
        tin_table_set(est->state, &tmap->values, tin_value_asstring(peek1), peek0);
    }
    else if(tin_value_isinstance(operand))
    {
        tinst =tin_value_asinstance(operand); 
        tin_table_set(est->state, &tinst->fields, tin_value_asstring(peek1), peek0);
    }
    else
    {
        tin_vmmac_raiseerrorfmtnocont("cannot set field '%s' on type '%s'", tin_tostring_typename(operand));
    }
    tin_vmintern_dropn(est, 2);
    return true;
}

// OP_VARARG
TIN_VM_INLINE bool tin_vmdo_vararg(TinExecState* est, TinValue* finalresult)
{
    size_t i;
    TinValue slot;
    TinValList* values;
    (void)finalresult;
    slot = est->slots[tin_vmintern_readbyte(est)];
    if(!tin_value_isarray(slot))
    {
        return true;
    }
    values = &tin_value_asarray(slot)->list;
    tin_fiber_ensurestack(est->state, est->fiber, tin_vallist_count(values) + est->frame->function->maxslots + (int)(est->fiber->stacktop - est->fiber->stackvalues));
    for(i = 0; i < tin_vallist_count(values); i++)
    {
        tin_vmintern_push(est, tin_vallist_get(values, i));
    }
    // Hot-bytecode patching, increment the amount of arguments to OP_CALLFUNCTION
    est->ip[1] = est->ip[1] + tin_vallist_count(values) - 1;
    return true;
}

// OP_REFFIELD
TIN_VM_INLINE bool tin_vmdo_reffield(TinExecState* est, TinValue* finalresult)
{
    TinValue object;
    TinString* name;
    TinValue* pval;
    object = tin_vmintern_peek(est, 1);
    if(tin_value_isnull(object))
    {
        tin_vmmac_raiseerrorfmtnocont("attempt to index a null value", 0);
    }
    if(tin_value_isinstance(object))
    {
        name = tin_value_asstring(tin_vmintern_peek(est, 0));
        if(!tin_table_get_slot(&tin_value_asinstance(object)->fields, name, &pval))
        {
            tin_vmmac_raiseerrorfmtnocont("attempt to reference a null value", 0);
        }
    }
    else
    {
        name = tin_value_asstring(tin_vmintern_peek(est, 0));
        tin_towriter_value(est->state, &est->state->debugwriter, object, true);
        printf("\n");
        tin_vmmac_raiseerrorfmtnocont("cannot reference field '%s' of a non-instance", name->data);
    }
    tin_vmintern_drop(est);// Pop field name
    est->fiber->stacktop[-1] = tin_value_fromobject(tin_object_makereference(est->state, pval));
    return true;
}

// OP_GLOBALSET
TIN_VM_INLINE bool tin_vmdo_globalset(TinExecState* est, TinValue* finalresult)
{
    TinString* name;
    (void)finalresult;
    name = tin_vmintern_readstringlong(est);
    tin_table_set(est->state, &est->vm->globals->values, name, tin_vmintern_peek(est, 0));
    return true;
}

// OP_GLOBALGET
TIN_VM_INLINE bool tin_vmdo_globalget(TinExecState* est, TinValue* finalresult)
{
    TinValue setval;
    TinString* name;
    (void)finalresult;
    name = tin_vmintern_readstringlong(est);
    //fprintf(stderr, "GET_GLOBAL: %s\n", name->data);
    if(!tin_table_get(&est->vm->globals->values, name, &setval))
    {
        tin_vmintern_push(est, tin_value_makenull(est->vm->state));
    }
    else
    {
        tin_vmintern_push(est, setval);
    }
    return true;
}

// OP_LOCALGET
TIN_VM_INLINE bool tin_vmdo_localget(TinExecState* est, TinValue* finalresult)
{
    (void)finalresult;
    tin_vmintern_push(est, est->slots[tin_vmintern_readbyte(est)]);
    return true;
}

// OP_LOCALSET
TIN_VM_INLINE bool tin_vmdo_localset(TinExecState* est, TinValue* finalresult)
{
    uint8_t index;
    (void)finalresult;
    index = tin_vmintern_readbyte(est);
    est->slots[index] = tin_vmintern_peek(est, 0);
    return true;
}

// OP_REFGLOBAL
TIN_VM_INLINE bool tin_vmdo_refglobal(TinExecState* est, TinValue* finalresult)
{
    TinString* name;
    TinValue* pval;
    name = tin_vmintern_readstringlong(est);
    if(tin_table_get_slot(&est->vm->globals->values, name, &pval))
    {
        tin_vmintern_push(est, tin_value_fromobject(tin_object_makereference(est->state, pval)));
    }
    else
    {
        tin_vmmac_raiseerrorfmtnocont("attempt to reference a null value", 0);
    }
    return true;
}

// OP_INVOKEIGNORING
TIN_VM_INLINE bool tin_vmdo_invokeignoring(TinExecState* est, TinValue* finalresult)
{
    uint8_t argc;
    TinValue mthval;
    TinValue receiver;
    TinValue vmthval;
    TinClass* type;
    TinInstance* instance;
    TinString* mthname;
    argc = tin_vmintern_readbyte(est);
    mthname = tin_vmintern_readstringlong(est);
    receiver = tin_vmintern_peek(est, argc);
    if(tin_value_isnull(receiver))
    {
        tin_vmmac_raiseerrorfmtnocont("invokeignoring: cannot index a null value with '%s'", mthname->data);
    }
    tin_vmintern_writeframe(est, est->ip);
    if(tin_value_isclass(receiver))
    {
        if((tin_value_isinstance(receiver) && (tin_table_get(&tin_value_asinstance(receiver)->fields, mthname, &mthval)))
           || tin_table_get(&tin_value_asclass(receiver)->staticfields, mthname, &mthval))
        {
            if(tin_vm_callvalue(est, mthval, mthname, argc))
            {
                tin_vmmac_recoverstate(est);
                est->frame->ignresult = true;
            }
            else
            {
                est->fiber->stacktop[-1] = receiver;
            }
        }
        else
        {
            tin_vmmac_raiseerrorfmtnocont("invokeignoring: cannot call undefined method '%s' of class '%s'", mthname->data, tin_value_asclass(receiver)->name->data)
        }
        return true;
    }
    else if(tin_value_isinstance(receiver))
    {
        instance = tin_value_asinstance(receiver);
        if(tin_table_get(&instance->fields, mthname, &vmthval))
        {
            est->fiber->stacktop[-argc - 1] = vmthval;
            tin_vmmac_callvalue(vmthval, mthname, argc);
            tin_vmintern_readframe(est);
            return true;
        }
        if((tin_value_isinstance(receiver) && (tin_table_get(&tin_value_asinstance(receiver)->fields, mthname, &mthval)))
           || tin_table_get(&instance->klass->methods, mthname, &mthval))
        {
            if(tin_vm_callvalue(est, mthval, mthname, argc))
            {
                tin_vmmac_recoverstate(est);
                est->frame->ignresult = true;
            }
            else
            {
                est->fiber->stacktop[-1] = receiver;
            }
        }
        else
        {
            tin_vmmac_raiseerrorfmtnocont("invokeignoring: cannot call undefined method '%s' of class '%s'", mthname->data, instance->klass->name->data)
        }
        return true;
    }
    else
    {
        type = tin_state_getclassfor(est->state, receiver);
        if(type == NULL)
        {
            tin_vmmac_raiseerrorfmtnocont("cannot get class", 0);
        }
        if((tin_value_isinstance(receiver) && (tin_table_get(&tin_value_asinstance(receiver)->fields, mthname, &mthval))) || tin_table_get(&type->methods, mthname, &mthval))
        {
            if(tin_vm_callvalue(est, mthval, mthname, argc))
            {
                tin_vmmac_recoverstate(est);
                est->frame->ignresult = true;
            }
            else
            {
                est->fiber->stacktop[-1] = receiver;
            }
        }
        else
        {
            tin_vmmac_raiseerrorfmtnocont("invokeignoring: cannot call undefined method '%s' of class '%s'", mthname->data, type->name->data)
        }
        return true;
    }
    return true;
}

// OP_INVOKEMETHOD
TIN_VM_INLINE bool tin_vmdo_invokemethod(TinExecState* est, TinValue* finalresult)
{
    uint8_t argc;
    TinValue mthval;
    TinValue receiver;
    TinValue vmthval;
    TinClass* type;
    TinString* mthname;
    TinInstance* instance;
    argc = tin_vmintern_readbyte(est);
    mthname = tin_vmintern_readstringlong(est);
    receiver = tin_vmintern_peek(est, argc);
    
    if(tin_value_isnull(receiver))
    {
        tin_vmmac_raiseerrorfmtnocont("invokemethod: cannot index a null value with '%s'", mthname->data);
    }
    tin_vmintern_writeframe(est, est->ip);
    if(tin_value_isclass(receiver))
    {
        if((tin_value_isinstance(receiver) && (tin_table_get(&tin_value_asinstance(receiver)->fields, mthname, &mthval)))
           || tin_table_get(&tin_value_asclass(receiver)->staticfields, mthname, &mthval))
        {
            tin_vmmac_callvalue(mthval, mthname, argc);
        }
        else
        {
            tin_vmmac_raiseerrorfmtnocont("invokemethod: cannot call undefined method '%s' of class '%s'", mthname->data, tin_value_asclass(receiver)->name->data)
        }
        return true;
    }
    else if(tin_value_isinstance(receiver))
    {
        instance = tin_value_asinstance(receiver);
        if(tin_table_get(&instance->fields, mthname, &vmthval))
        {
            est->fiber->stacktop[-argc - 1] = vmthval;
            tin_vmmac_callvalue(vmthval, mthname, argc);
            tin_vmintern_readframe(est);
            return true;
        }
        if((tin_value_isinstance(receiver) && (tin_table_get(&tin_value_asinstance(receiver)->fields, mthname, &mthval)))
           || tin_table_get(&instance->klass->methods, mthname, &mthval))
        {
            tin_vmmac_callvalue(mthval, mthname, argc);
        }
        else
        {
            tin_vmmac_raiseerrorfmtnocont("invokemethod: cannot call undefined method '%s' of class '%s'", mthname->data, instance->klass->name->data)
        }
        return true;
    }
    else
    {
        type = tin_state_getclassfor(est->state, receiver);
        if(type == NULL)
        {
            tin_vmmac_raiseerrorfmtnocont("cannot get class", 0);
        }
        if((tin_value_isinstance(receiver) && (tin_table_get(&tin_value_asinstance(receiver)->fields, mthname, &mthval))) || tin_table_get(&type->methods, mthname, &mthval))
        {
            tin_vmmac_callvalue(mthval, mthname, argc);
        }
        else
        {
            tin_vmmac_raiseerrorfmtnocont("invokemethod: cannot call undefined method '%s' of class '%s'", mthname->data, type->name->data)
        }
        return true;
    }
    return true;
}


/*
TIN_VM_INLINE bool tin_vmdo_%(TinExecState* est, TinValue* finalresult)
{

    return true;
}
*/

// dofuncs

TinInterpretResult tin_vm_execfiber(TinState* state, TinFiber* fiber)
{
    TinValue finalresult;
    if(!tin_vmintern_execfiber(state, fiber, &finalresult))
    {
        return TIN_MAKESTATUS(TINSTATE_RUNTIMEERROR, tin_value_makenull(state));
    }
    return TIN_MAKESTATUS(TINSTATE_OK, finalresult);
}

bool tin_vmintern_execfiber(TinState* exstate, TinFiber* exfiber, TinValue* finalresult)
{
    //vartop
    uint8_t instruction;
    uint8_t nowinstr;
    TinValList* values;
    TinExecState eststack;
    TinExecState* est;
    TinVM* exvm;
    exvm = exstate->vm;
    exvm->fiber = exfiber;
    exfiber->abort = false;
    est = &eststack;
    est->fiber = exfiber;
    est->state = exstate;
    est->vm = exvm;
    est->frame = &est->fiber->framevalues[est->fiber->framecount - 1];
    est->currentchunk = &est->frame->function->chunk;
    est->fiber->module = est->frame->function->module;
    est->ip = est->frame->ip;
    est->slots = est->frame->slots;
    est->privates = est->fiber->module->privates;
    est->upvalues = est->frame->closure == NULL ? NULL : est->frame->closure->upvalues;
    tin_vmmac_pushgc(est, true);

    // Has to be inside of the function in order for goto to work
    #ifdef TIN_USE_COMPUTEDGOTO
        static void* dispatchtable[] =
        {
            &&OP_POP,
            &&OP_RETURN,
            &&OP_CONSTVALUE,
            &&OP_CONSTLONG,
            &&OP_VALTRUE,
            &&OP_VALFALSE,
            &&OP_VALNULL,
            &&OP_VALARRAY,
            &&OP_VALOBJECT,
            &&OP_RANGE,
            &&OP_NEGATE,
            &&OP_NOT,
            &&OP_MATHADD,
            &&OP_MATHSUB,
            &&OP_MATHMULT,
            &&OP_MATHPOWER,
            &&OP_MATHDIV,
            &&OP_MATHFLOORDIV,
            &&OP_MATHMOD,
            &&OP_BINAND,
            &&OP_BINOR,
            &&OP_BINXOR,
            &&OP_LEFTSHIFT,
            &&OP_RIGHTSHIFT,
            &&OP_BINNOT,
            &&OP_EQUAL,
            &&OP_GREATERTHAN,
            &&OP_GREATEREQUAL,
            &&OP_LESSTHAN,
            &&OP_LESSEQUAL,
            &&OP_GLOBALSET,
            &&OP_GLOBALGET,
            &&OP_LOCALSET,
            &&OP_LOCALGET,
            &&OP_LOCALLONGSET,
            &&OP_LOCALLONGGET,
            &&OP_PRIVATESET,
            &&OP_PRIVATEGET,
            &&OP_PRIVATELONGSET,
            &&OP_PRIVATELONGGET,
            &&OP_UPVALSET,
            &&OP_UPVALGET,
            &&OP_JUMPIFFALSE,
            &&OP_JUMPIFNULL,
            &&OP_JUMPIFNULLPOP,
            &&OP_JUMPALWAYS,
            &&OP_JUMPBACK,
            &&OP_AND,
            &&OP_OR,
            &&OP_NULLOR,
            &&OP_MAKECLOSURE,
            &&OP_UPVALCLOSE,
            &&OP_MAKECLASS,
            &&OP_FIELDGET,
            &&OP_FIELDSET,
            &&OP_GETINDEX,
            &&OP_SETINDEX,
            &&OP_ARRAYPUSHVALUE,
            &&OP_OBJECTPUSHFIELD,
            &&OP_MAKEMETHOD,
            &&OP_FIELDSTATIC,
            &&OP_FIELDDEFINE,
            &&OP_CLASSINHERIT,
            &&OP_ISCLASS,
            &&OP_GETSUPERMETHOD,
            &&OP_CALLFUNCTION,
            &&OP_INVOKEMETHOD,
            &&OP_INVOKESUPER,
            &&OP_INVOKEIGNORING,
            &&OP_INVOKESUPERIGNORING,
            &&OP_POPLOCALS,
            &&OP_VARARG,
            &&OP_REFGLOBAL,
            &&OP_REFPRIVATE,
            &&OP_REFLOCAL,
            &&OP_REFUPVAL,
            &&OP_REFFIELD,
            &&OP_REFSET,
        };

    #endif
#ifdef TIN_TRACE_EXECUTION
    tin_vmmac_traceframe(est->fiber);
#endif

    while(true)
    {
#ifdef TIN_TRACE_STACK
        tin_vmintern_tracestack(est->vm);
#endif

#ifdef TIN_CHECK_STACK_SIZE
        if((est->fiber->stacktop - est->frame->slots) > est->fiber->stackcap)
        {
            tin_vmmac_raiseerrorfmtcont("fiber stack too small (%i > %i)", (int)(est->fiber->stacktop - est->frame->slots),
                               est->fiber->stackcap);
        }
#endif

        #ifdef TIN_USE_COMPUTEDGOTO
            instruction = *est->ip++;
            nowinstr = instruction;
            #ifdef TIN_TRACE_EXECUTION
                tin_disassemble_instruction(est->state, est->currentchunk, (size_t)(est->ip - est->currentchunk->code - 1), NULL);
            #endif
            goto* dispatchtable[instruction];
        #else
            instruction = *est->ip++;
            nowinstr = instruction;
            #ifdef TIN_TRACE_EXECUTION
                tin_disassemble_instruction(est->state, est->currentchunk, (size_t)(est->ip - est->currentchunk->code - 1), NULL);
            #endif
            switch(instruction)
        #endif
        /*
        * each op_case(...){...} *MUST* end with either break, return, or continue.
        * computationally, fall-throughs differ wildly between computed gotos or switch/case statements.
        * in computed gotos, a "fall-through" just executes the next block (since it's just a labelled block),
        * which may invalidate the stack, and while the same is technically true for switch/case, they
        * could end up executing completely unrelated instructions.
        * think, declaring a block for OP_BUILDHOUSE, and the next block is OP_SETHOUSEONFIRE.
        * an easy mistake to make, but crucial to check.
        */
        {
            op_case(OP_POP)
            {
                tin_vmintern_drop(est);
                continue;
            }
            op_case(OP_RETURN)
            {
                size_t argc;
                TinValue result;
                TinFiber* parent;
                result = tin_vmintern_pop(est);
                tin_vmintern_closeupvalues(est->vm, est->slots);
                tin_vmintern_writeframe(est, est->ip);
                est->fiber->framecount--;
                if(est->frame->returntonative)
                {
                    est->frame->returntonative = false;
                    est->fiber->module->returnvalue = result;
                    est->fiber->stacktop = est->frame->slots;
                    *finalresult = result;
                    return true;
                }
                if(est->fiber->framecount == 0)
                {
                    est->fiber->module->returnvalue = result;
                    if(est->fiber->parent == NULL)
                    {
                        tin_vmintern_drop(est);
                        est->state->gcallow = est->wasallowed;
                        *finalresult = result;
                        return true;
                    }
                    argc = est->fiber->funcargcount;
                    parent = est->fiber->parent;
                    est->fiber->parent = NULL;
                    est->vm->fiber = est->fiber = parent;
                    tin_vmintern_readframe(est);
                    tin_vmmac_traceframe(est->fiber);
                    est->fiber->stacktop -= argc;
                    est->fiber->stacktop[-1] = result;
                    continue;
                }
                est->fiber->stacktop = est->frame->slots;
                if(est->frame->ignresult)
                {
                    est->fiber->stacktop++;
                    est->frame->ignresult = false;
                }
                else
                {
                    tin_vmintern_push(est, result);
                }
                tin_vmintern_readframe(est);
                tin_vmmac_traceframe(est->fiber);
                continue;
            }
            op_case(OP_CONSTVALUE)
            {
                tin_vmintern_push(est, tin_vmintern_readconstant(est));
                continue;
            }
            op_case(OP_CONSTLONG)
            {
                tin_vmintern_push(est, tin_vmintern_readconstantlong(est));
                continue;
            }
            op_case(OP_VALTRUE)
            {
                tin_vmintern_push(est, tin_value_makebool(est->state, true));
                continue;
            }
            op_case(OP_VALFALSE)
            {
                tin_vmintern_push(est, tin_value_makebool(est->state, false));
                continue;
            }
            op_case(OP_VALNULL)
            {
                tin_vmintern_push(est, tin_value_makenull(est->state));
                continue;
            }
            op_case(OP_VALARRAY)
            {
                tin_vmintern_push(est, tin_value_fromobject(tin_object_makearray(est->state)));
                continue;
            }
            op_case(OP_VALOBJECT)
            {
                // TODO: use object, or map for literal '{...}' constructs?
                // objects would be more general-purpose, but don't implement anything map-like.
                //tin_vmintern_push(est, tin_value_fromobject(tin_object_makeinstance(state, state->primobjectclass)));
                tin_vmintern_push(est, tin_value_fromobject(tin_object_makemap(est->state)));
                continue;
            }
            op_case(OP_RANGE)
            {
                if(!tin_vmdo_range(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_NEGATE)
            {
                TinValue tmpval;
                TinValue popped;
                if(!tin_value_isnumber(tin_vmintern_peek(est, 0)))
                {
                    tin_vmmac_raiseerror("operand must be a number");
                }
                popped = tin_vmintern_pop(est);
                if(popped.isfixednumber)
                {
                    tmpval = tin_value_makefixednumber(est->vm->state, -tin_value_asfixednumber(popped));
                }
                else
                {
                    tmpval = tin_value_makefloatnumber(est->vm->state, -tin_value_asfloatnumber(popped));
                }
                tin_vmintern_push(est, tmpval);
                continue;
            }
            op_case(OP_NOT)
            {
                TinValue tmpval;
                TinValue popped;
                if(tin_value_isinstance(tin_vmintern_peek(est, 0)))
                {
                    tin_vmintern_writeframe(est, est->ip);
                    tin_vmmac_invokefromclass(tin_value_asinstance(tin_vmintern_peek(est, 0))->klass, tin_string_copyconst(est->state, "!"), 0, false, methods, false);
                    continue;
                }
                popped = tin_vmintern_pop(est);
                tmpval = tin_value_makebool(est->vm->state, tin_value_isfalsey(popped));
                tin_vmintern_push(est, tmpval);
                continue;
            }
            op_case(OP_BINNOT)
            {
                int64_t ival;
                TinValue tmpval;
                TinValue popped;
                if(!tin_value_isnumber(tin_vmintern_peek(est, 0)))
                {
                    tin_vmmac_raiseerror("Operand must be a number");
                }
                popped = tin_vmintern_pop(est);
                ival = (int)tin_value_asnumber(popped);
                tmpval = tin_value_makefixednumber(est->vm->state, ~ival);
                tin_vmintern_push(est, tmpval);
                continue;
            }
            op_case(OP_MATHFLOORDIV)
            {
                TinValue vala;
                TinValue valb;
                vala = tin_vmintern_peek(est, 1);
                valb = tin_vmintern_peek(est, 0);
                if(tin_value_isnumber(vala) && tin_value_isnumber(valb))
                {
                    tin_vmintern_drop(est);
                    *(est->fiber->stacktop - 1) = tin_value_makefloatnumber(est->vm->state, floor(tin_value_asnumber(vala) / tin_value_asnumber(valb)));
                }
                else
                {
                    tin_vmmac_invokemethod(vala, "#", 1);
                }
                continue;
            }
            op_case(OP_MATHADD)
            op_case(OP_MATHSUB)
            op_case(OP_MATHMULT)
            op_case(OP_MATHPOWER)
            op_case(OP_MATHMOD)
            op_case(OP_MATHDIV)
            op_case(OP_BINAND)
            op_case(OP_BINOR)
            op_case(OP_BINXOR)
            op_case(OP_LEFTSHIFT)
            op_case(OP_RIGHTSHIFT)
            op_case(OP_EQUAL)
            op_case(OP_GREATERTHAN)
            op_case(OP_GREATEREQUAL)
            op_case(OP_LESSTHAN)
            op_case(OP_LESSEQUAL)
            {
                tin_vmmac_binaryop(nowinstr, vmutil_op2s(nowinstr));
                continue;
            }
            op_case(OP_GLOBALSET)
            {
                if(!tin_vmdo_globalset(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_GLOBALGET)
            {
                if(!tin_vmdo_globalget(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_LOCALSET)
            {
                if(!tin_vmdo_localset(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_LOCALGET)
            {
                if(!tin_vmdo_localget(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_LOCALLONGSET)
            {
                uint8_t index;
                index = tin_vmintern_readshort(est);
                est->slots[index] = tin_vmintern_peek(est, 0);
                continue;
            }
            op_case(OP_LOCALLONGGET)
            {
                tin_vmintern_push(est, est->slots[tin_vmintern_readshort(est)]);
                continue;
            }
            op_case(OP_PRIVATESET)
            {
                uint8_t index;
                index = tin_vmintern_readbyte(est);
                est->privates[index] = tin_vmintern_peek(est, 0);
                continue;
            }
            op_case(OP_PRIVATEGET)
            {
                tin_vmintern_push(est, est->privates[tin_vmintern_readbyte(est)]);
                continue;
            }
            op_case(OP_PRIVATELONGSET)
            {
                uint8_t index;
                index = tin_vmintern_readshort(est);
                est->privates[index] = tin_vmintern_peek(est, 0);
                continue;
            }
            op_case(OP_PRIVATELONGGET)
            {
                tin_vmintern_push(est, est->privates[tin_vmintern_readshort(est)]);
                continue;
            }
            op_case(OP_UPVALSET)
            {
                uint8_t index;
                index = tin_vmintern_readbyte(est);
                *est->upvalues[index]->location = tin_vmintern_peek(est, 0);
                continue;
            }
            op_case(OP_UPVALGET)
            {
                tin_vmintern_push(est, *est->upvalues[tin_vmintern_readbyte(est)]->location);
                continue;
            }
            op_case(OP_JUMPIFFALSE)
            {
                uint16_t offset;
                TinValue popped;
                offset = tin_vmintern_readshort(est);
                popped = tin_vmintern_pop(est);
                if(tin_value_isfalsey(popped))
                {
                    est->ip += offset;
                }
                continue;
            }
            op_case(OP_JUMPIFNULL)
            {
                uint16_t offset;
                offset = tin_vmintern_readshort(est);
                if(tin_value_isnull(tin_vmintern_peek(est, 0)))
                {
                    est->ip += offset;
                }
                continue;
            }
            op_case(OP_JUMPIFNULLPOP)
            {
                uint16_t offset;
                TinValue popped;
                offset = tin_vmintern_readshort(est);
                popped = tin_vmintern_pop(est);
                if(tin_value_isnull(popped))
                {
                    est->ip += offset;
                }
                continue;
            }
            op_case(OP_JUMPALWAYS)
            {
                uint16_t offset;
                offset = tin_vmintern_readshort(est);
                est->ip += offset;
                continue;
            }
            op_case(OP_JUMPBACK)
            {
                uint16_t offset;
                offset = tin_vmintern_readshort(est);
                est->ip -= offset;
                continue;
            }
            op_case(OP_AND)
            {
                uint16_t offset;
                offset = tin_vmintern_readshort(est);
                if(tin_value_isfalsey(tin_vmintern_peek(est, 0)))
                {
                    est->ip += offset;
                }
                else
                {
                    tin_vmintern_drop(est);
                }
                continue;
            }
            op_case(OP_OR)
            {
                uint16_t offset;
                offset = tin_vmintern_readshort(est);
                if(tin_value_isfalsey(tin_vmintern_peek(est, 0)))
                {
                    tin_vmintern_drop(est);
                }
                else
                {
                    est->ip += offset;
                }
                continue;
            }
            op_case(OP_NULLOR)
            {
                uint16_t offset;
                offset = tin_vmintern_readshort(est);
                if(tin_value_isnull(tin_vmintern_peek(est, 0)))
                {
                    tin_vmintern_drop(est);
                }
                else
                {
                    est->ip += offset;
                }
                continue;
            }
            op_case(OP_CALLFUNCTION)
            {
                if(!tin_vmdo_call(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_MAKECLOSURE)
            {
                if(!tin_vmdo_makeclosure(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_UPVALCLOSE)
            {
                tin_vmintern_closeupvalues(est->vm, est->fiber->stacktop - 1);
                tin_vmintern_drop(est);
                continue;
            }
            op_case(OP_MAKECLASS)
            {
                if(!tin_vmdo_makeclass(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_FIELDGET)
            {
                if(!tin_vmdo_fieldget(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_FIELDSET)
            {
                if(!tin_vmdo_fieldset(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_GETINDEX)
            {
                tin_vmmac_invokemethod(tin_vmintern_peek(est, 1), "[]", 1);
                continue;
            }
            op_case(OP_SETINDEX)
            {
                tin_vmmac_invokemethod(tin_vmintern_peek(est, 2), "[]", 2);
                continue;
            }
            op_case(OP_ARRAYPUSHVALUE)
            {
                size_t arindex;
                values = &tin_value_asarray(tin_vmintern_peek(est, 1))->list;
                arindex = tin_vallist_count(values);
                tin_vallist_ensuresize(est->state, values, arindex + 1);
                tin_vallist_set(est->state, values, arindex, tin_vmintern_peek(est, 0));
                tin_vmintern_drop(est);
                continue;
            }
            op_case(OP_OBJECTPUSHFIELD)
            {
                if(!tin_vmdo_objectpushfield(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_FIELDSTATIC)
            {
                tin_table_set(est->state, &tin_value_asclass(tin_vmintern_peek(est, 1))->staticfields, tin_vmintern_readstringlong(est), tin_vmintern_peek(est, 0));
                tin_vmintern_drop(est);
                continue;
            }
            op_case(OP_MAKEMETHOD)
            {
                if(!tin_vmdo_makemethod(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_FIELDDEFINE)
            {
                tin_table_set(est->state, &tin_value_asclass(tin_vmintern_peek(est, 1))->methods, tin_vmintern_readstringlong(est), tin_vmintern_peek(est, 0));
                tin_vmintern_drop(est);
                continue;
            }
            op_case(OP_INVOKEMETHOD)
            {
                if(!tin_vmdo_invokemethod(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_INVOKEIGNORING)
            {
                if(!tin_vmdo_invokeignoring(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_INVOKESUPER)
            {
                size_t argc;
                TinValue popped;
                TinClass* klassobj;
                TinString* mthname;
                argc = tin_vmintern_readbyte(est);
                mthname = tin_vmintern_readstringlong(est);
                popped = tin_vmintern_pop(est);
                klassobj = tin_value_asclass(popped);
                tin_vmintern_writeframe(est, est->ip);
                tin_vmmac_invokefromclass(klassobj, mthname, argc, true, methods, false);
                continue;
            }
            op_case(OP_INVOKESUPERIGNORING)
            {
                size_t argc;
                TinValue popped;
                TinClass* klassobj;
                TinString* mthname;
                argc = tin_vmintern_readbyte(est);
                mthname = tin_vmintern_readstringlong(est);
                popped = tin_vmintern_pop(est);
                klassobj = tin_value_asclass(popped);
                tin_vmintern_writeframe(est, est->ip);
                tin_vmmac_invokefromclass(klassobj, mthname, argc, true, methods, true);
                continue;
            }
            op_case(OP_GETSUPERMETHOD)
            {
                TinValue value;
                TinValue instval;
                TinValue popped;
                TinClass* klassobj;
                TinString* mthname;
                mthname = tin_vmintern_readstringlong(est);
                popped = tin_vmintern_pop(est);
                klassobj = tin_value_asclass(popped);
                instval = tin_vmintern_pop(est);
                if(tin_table_get(&klassobj->methods, mthname, &value))
                {
                    value = tin_value_fromobject(tin_object_makeboundmethod(est->state, instval, value));
                }
                else
                {
                    value = tin_value_makenull(est->vm->state);
                }
                tin_vmintern_push(est, value);
                continue;
            }
            op_case(OP_CLASSINHERIT)
            {
                TinValue super;
                TinClass* superklass;
                TinClass* klassobj;
                super = tin_vmintern_peek(est, 1);
                if(!tin_value_isclass(super))
                {
                    tin_vmmac_raiseerror("superclass must be a class");
                }
                klassobj = tin_value_asclass(tin_vmintern_peek(est, 0));
                superklass = tin_value_asclass(super);
                klassobj->parentclass = superklass;
                klassobj->initmethod = superklass->initmethod;
                tin_table_add_all(est->state, &superklass->methods, &klassobj->methods);
                tin_table_add_all(est->state, &klassobj->parentclass->staticfields, &klassobj->staticfields);
                continue;
            }
            op_case(OP_ISCLASS)
            {
                bool found;
                TinValue klassval;
                TinValue instval;
                TinClass* type;
                TinClass* instanceklass;
                instval = tin_vmintern_peek(est, 1);
                if(tin_value_isnull(instval))
                {
                    tin_vmintern_dropn(est, 2);
                    tin_vmintern_push(est, tin_value_makebool(est->state, false));

                    continue;
                }
                instanceklass = tin_state_getclassfor(est->state, instval);
                klassval = tin_vmintern_peek(est, 0);
                if(instanceklass == NULL || !tin_value_isclass(klassval))
                {
                    tin_vmmac_raiseerror("operands must be an instance or a class");
                }            
                type = tin_value_asclass(klassval);
                found = false;
                while(instanceklass != NULL)
                {
                    if(instanceklass == type)
                    {
                        found = true;
                        break;
                    }
                    instanceklass = (TinClass*)instanceklass->parentclass;
                }
                tin_vmintern_dropn(est, 2);// Drop the instance and class
                tin_vmintern_push(est, tin_value_makebool(est->vm->state, found));
                continue;
            }
            op_case(OP_POPLOCALS)
            {
                tin_vmintern_dropn(est, tin_vmintern_readshort(est));
                continue;
            }
            op_case(OP_VARARG)
            {
                if(!tin_vmdo_vararg(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_REFGLOBAL)
            {
                if(!tin_vmdo_refglobal(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_REFPRIVATE)
            {
                tin_vmintern_push(est, tin_value_fromobject(tin_object_makereference(est->state, &est->privates[tin_vmintern_readshort(est)])));
                continue;
            }
            op_case(OP_REFLOCAL)
            {
                tin_vmintern_push(est, tin_value_fromobject(tin_object_makereference(est->state, &est->slots[tin_vmintern_readshort(est)])));
                continue;
            }
            op_case(OP_REFUPVAL)
            {
                tin_vmintern_push(est, tin_value_fromobject(tin_object_makereference(est->state, est->upvalues[tin_vmintern_readbyte(est)]->location)));
                continue;
            }
            op_case(OP_REFFIELD)
            {
                if(!tin_vmdo_reffield(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_REFSET)
            {
                TinValue reference;
                reference = tin_vmintern_pop(est);
                if(!tin_value_isreference(reference))
                {
                    tin_vmmac_raiseerror("cannot set reference value of a non-reference");
                }
                *tin_value_asreference(reference)->slot = tin_vmintern_peek(est, 0);
                continue;
            }
            vm_default()
            {
                tin_vmmac_raiseerrorfmtcont("unknown VM op code '%d'", *est->ip);
                break;
            }
        }
    }
    tin_vmmac_returnerror();
    return false;
}

