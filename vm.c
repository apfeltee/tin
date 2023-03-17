
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <setjmp.h>
#include "priv.h"

/*
* visual studio doesn't support computed gotos, so
* instead a switch-case is used. 
*/
#if !defined(_MSC_VER)
    #define TIN_USE_COMPUTEDGOTO
#endif

#ifdef TIN_TRACE_EXECUTION
    #define vm_traceframe(fiber)\
        tin_trace_frame(fiber);
#else
    #define vm_traceframe(fiber) \
        do \
        { \
        } while(0);
#endif


#define TIN_VM_INLINE

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

#define vm_returnerror() \
    vm_popgc(est); \
    return false;

#define vm_pushgc(est, allow) \
    est->wasallowed = est->state->gcallow; \
    est->state->gcallow = allow;

#define vm_popgc(est) \
    est->state->gcallow = est->wasallowed;

/*
* can't be turned into a function because it is expected to return in
* tin_vm_execfiber.
* might be possible to wrap this by using an enum to specify
* if (and what) to return, but it'll be quite a bit of work to refactor.
* likewise, any macro that uses vm_recoverstate can't be turned into
* a function.
*/
#define vm_recoverstate(est) \
    tin_vmexec_writeframe(est, est->ip); \
    est->fiber = est->vm->fiber; \
    if(est->fiber == NULL) \
    { \
        *finalresult = tin_vmexec_pop(est->fiber); \
        return true; \
    } \
    if(est->fiber->abort) \
    { \
        vm_returnerror(); \
    } \
    tin_vmexec_readframe(est->fiber, est); \
    vm_traceframe(est->fiber);

#define vm_callvalue(callee, argc) \
    if(tin_vm_callvalue(est, callee, argc)) \
    { \
        vm_recoverstate(est); \
    }

#define vmexec_raiseerrorfmt(format, ...) \
    if(tin_vm_raiseerror(est->vm, format, __VA_ARGS__)) \
    { \
        vm_recoverstate(est);  \
        continue; \
    } \
    else \
    { \
        vm_returnerror(); \
    }

#define vmexec_raiseerror(format) \
    vmexec_raiseerrorfmt(format, NULL);

#define vmexec_advinvokefromclass(zklass, mthname, argc, raiseerr, stat, ignoring, callee) \
    TinValue mthval; \
    if((tin_value_isinstance(callee) && (tin_table_get(&tin_value_asinstance(callee)->fields, mthname, &mthval))) \
       || tin_table_get(&zklass->stat, mthname, &mthval)) \
    { \
        if(ignoring) \
        { \
            if(tin_vm_callvalue(est, mthval, argc)) \
            { \
                vm_recoverstate(est); \
                est->frame->result_ignored = true; \
            } \
            else \
            { \
                est->fiber->stack_top[-1] = callee; \
            } \
        } \
        else \
        { \
            vm_callvalue(mthval, argc); \
        } \
    } \
    else \
    { \
        if(raiseerr) \
        { \
            vmexec_raiseerrorfmt("cannot call undefined method '%s' of class '%s'", mthname->chars, \
                               zklass->name->chars) \
        } \
    } \
    if(raiseerr) \
    { \
        continue; \
    }

// calls vm_recoverstate
#define vmexec_invokefromclass(klass, mthname, argc, raiseerr, stat, ignoring) \
    vmexec_advinvokefromclass(klass, mthname, argc, raiseerr, stat, ignoring, tin_vmexec_peek(est, argc))

// calls vm_recoverstate
#define vm_invokemethod(instance, mthname, argc) \
    if(tin_value_isnull(instance)) \
    { \
        fprintf(stderr, "mthname=<%s>\n", mthname);\
        vmexec_raiseerrorfmt("cannot call method '%s' of null-instance", mthname); \
    } \
    TinClass* klass = tin_state_getclassfor(est->state, instance); \
    if(klass == NULL) \
    { \
        vmexec_raiseerrorfmt("cannot call method '%s' of a non-class", mthname); \
    } \
    tin_vmexec_writeframe(est, est->ip); \
    vmexec_advinvokefromclass(klass, tin_string_copyconst(est->state, mthname), argc, true, methods, false, instance); \
    tin_vmexec_readframe(est->fiber, est);

#define vm_binaryop(op, opstring) \
    TinValue a = tin_vmexec_peek(est, 1); \
    TinValue b = tin_vmexec_peek(est, 0); \
    /* implicitly converts NULL to numeric 0 */ \
    if(tin_value_isnumber(a) || tin_value_isnull(a)) \
    { \
        if(!tin_value_isnumber(b)) \
        { \
            if(!tin_value_isnull(b)) \
            { \
                vmexec_raiseerrorfmt("cannot use op '%s' with a 'number' and a '%s'", opstring, tin_tostring_typename(b)); \
            } \
        } \
        tin_vmexec_drop(est->fiber); \
        if(vm_binaryop_actual(est->vm, est->fiber, op, a, b))\
        { \
            continue; \
        } \
    } \
    if(tin_value_isnull(a)) \
    { \
        tin_vmexec_drop(est->fiber); \
        if(tin_value_isnull(b)) \
        { \
            *(est->fiber->stack_top - 1) = TRUE_VALUE; \
        } \
        else \
        { \
            vmexec_raiseerrorfmt("cannot use op %s on a null value", opstring); \
            *(est->fiber->stack_top - 1) = FALSE_VALUE;\
        } \
    } \
    else \
    { \
        vm_invokemethod(a, opstring, 1); \
    }

#define vm_invokeoperation(ignoring) \
    uint8_t argc = tin_vmexec_readbyte(est); \
    TinString* mthname = tin_vmexec_readstringlong(est); \
    TinValue receiver = tin_vmexec_peek(est, argc); \
    if(tin_value_isnull(receiver)) \
    { \
        vmexec_raiseerrorfmt("cannot index a null value with '%s'", mthname->chars); \
    } \
    tin_vmexec_writeframe(est, est->ip); \
    if(tin_value_isclass(receiver)) \
    { \
        vmexec_advinvokefromclass(tin_value_asclass(receiver), mthname, argc, true, static_fields, ignoring, receiver); \
        continue; \
    } \
    else if(tin_value_isinstance(receiver)) \
    { \
        TinInstance* instance = tin_value_asinstance(receiver); \
        TinValue vmmethvalue; \
        if(tin_table_get(&instance->fields, mthname, &vmmethvalue)) \
        { \
            est->fiber->stack_top[-argc - 1] = vmmethvalue; \
            vm_callvalue(vmmethvalue, argc); \
            tin_vmexec_readframe(est->fiber, est); \
            continue; \
        } \
        vmexec_advinvokefromclass(instance->klass, mthname, argc, true, methods, ignoring, receiver); \
    } \
    else \
    { \
        TinClass* type = tin_state_getclassfor(est->state, receiver); \
        if(type == NULL) \
        { \
            vmexec_raiseerror("cannot get class"); \
        } \
        vmexec_advinvokefromclass(type, mthname, argc, true, methods, ignoring, receiver); \
    }

enum
{
    RECOVER_RETURNOK,
    RECOVER_RETURNFAIL,
    RECOVER_NOTHING
};

static jmp_buf jumpbuffer;

//#define TIN_TRACE_EXECUTION

TIN_VM_INLINE uint16_t tin_vmexec_readshort(TinExecState *est);
TIN_VM_INLINE uint8_t tin_vmexec_readbyte(TinExecState *est);
TIN_VM_INLINE TinValue tin_vmexec_readconstant(TinExecState *est);
TIN_VM_INLINE TinValue tin_vmexec_readconstantlong(TinExecState *est);
TIN_VM_INLINE TinString *tin_vmexec_readstring(TinExecState *est);
TIN_VM_INLINE TinString *tin_vmexec_readstringlong(TinExecState *est);
TIN_VM_INLINE void tin_vmexec_push(TinFiber *fiber, TinValue v);
TIN_VM_INLINE TinValue tin_vmexec_pop(TinFiber *fiber);
TIN_VM_INLINE void tin_vmexec_drop(TinFiber *fiber);
TIN_VM_INLINE void tin_vmexec_dropn(TinFiber *fiber, int amount);
TIN_VM_INLINE TinValue tin_vmexec_peek(TinExecState *est, short distance);
TIN_VM_INLINE void tin_vmexec_readframe(TinFiber *fiber, TinExecState *est);
TIN_VM_INLINE void tin_vmexec_writeframe(TinExecState *est, uint8_t *ip);
void tin_vmexec_resetstack(TinVM *vm);
void tin_vmexec_resetvm(TinState *state, TinVM *vm);
void tin_vm_init(TinState *state, TinVM *vm);
void tin_vm_destroy(TinVM *vm);
void tin_vm_tracestack(TinVM *vm, TinWriter *wr);
bool tin_vm_handleruntimeerror(TinVM *vm, TinString *errorstring);
bool tin_vm_vraiseerror(TinVM *vm, const char *format, va_list args);
bool tin_vm_raiseerror(TinVM *vm, const char *format, ...);
bool tin_vm_raiseexitingerror(TinVM *vm, const char *format, ...);
bool tin_vm_callcallable(TinVM *vm, TinFunction *function, TinClosure *closure, uint8_t argc);
bool tin_vm_callvalue(TinExecState* est, TinValue callee, uint8_t argc);
TinUpvalue *tin_execvm_captureupvalue(TinState *state, TinValue *local);
void tin_vm_closeupvalues(TinVM *vm, const TinValue *last);
TinInterpretResult tin_vm_execmodule(TinState *state, TinModule *module);
bool tin_vm_internexecfiber(TinState* state, TinFiber* fiber, TinValue* finalresult);
void tin_vmutil_callexitjump(void);
bool tin_vmutil_setexitjump(void);


TIN_VM_INLINE uint16_t tin_vmexec_readshort(TinExecState* est)
{
    est->ip += 2u;
    return (uint16_t)((est->ip[-2] << 8u) | est->ip[-1]);
}

TIN_VM_INLINE uint8_t tin_vmexec_readbyte(TinExecState* est)
{
    return (*est->ip++);
}

TIN_VM_INLINE TinValue tin_vmexec_readconstant(TinExecState* est)
{
    return tin_vallist_get(&est->currentchunk->constants, tin_vmexec_readbyte(est));
}

TIN_VM_INLINE TinValue tin_vmexec_readconstantlong(TinExecState* est)
{
    return tin_vallist_get(&est->currentchunk->constants, tin_vmexec_readshort(est));
}

TIN_VM_INLINE TinString* tin_vmexec_readstring(TinExecState* est)
{
    return tin_value_asstring(tin_vmexec_readconstant(est));
}

TIN_VM_INLINE TinString* tin_vmexec_readstringlong(TinExecState* est)
{
    return tin_value_asstring(tin_vmexec_readconstantlong(est));
}


TIN_VM_INLINE void tin_vmexec_push(TinFiber* fiber, TinValue v)
{
    *fiber->stack_top++ = v;
}

TIN_VM_INLINE TinValue tin_vmexec_pop(TinFiber* fiber)
{
    return *(--fiber->stack_top);
}

TIN_VM_INLINE void tin_vmexec_drop(TinFiber* fiber)
{
    fiber->stack_top--;
}

TIN_VM_INLINE void tin_vmexec_dropn(TinFiber* fiber, int amount)
{
    fiber->stack_top -= amount;
}

TIN_VM_INLINE TinValue tin_vmexec_peek(TinExecState* est, short distance)
{
    int ofs;
    ofs = ((-1) - distance);
    if(ofs < 0)
    {
        //return NULL_VALUE;
    }
    return est->fiber->stack_top[ofs];
}

TIN_VM_INLINE void tin_vmexec_readframe(TinFiber* fiber, TinExecState* est)
{
    est->frame = &fiber->frames[fiber->frame_count - 1];
    est->currentchunk = &est->frame->function->chunk;
    est->ip = est->frame->ip;
    est->slots = est->frame->slots;
    fiber->module = est->frame->function->module;
    est->privates = fiber->module->privates;
    est->upvalues = est->frame->closure == NULL ? NULL : est->frame->closure->upvalues;
}

TIN_VM_INLINE void tin_vmexec_writeframe(TinExecState* est, uint8_t* ip)
{
    est->frame->ip = ip;
}

void tin_vmexec_resetstack(TinVM* vm)
{
    if(vm->fiber != NULL)
    {
        vm->fiber->stack_top = vm->fiber->stack;
    }
}

void tin_vmexec_resetvm(TinState* state, TinVM* vm)
{
    vm->state = state;
    vm->gcobjects = NULL;
    vm->fiber = NULL;
    vm->gcgraystack = NULL;
    vm->gcgraycount = 0;
    vm->gcgraycapacity = 0;
    tin_table_init(vm->state, &vm->gcstrings);
    vm->globals = NULL;
    vm->modules = NULL;
}

void tin_vm_init(TinState* state, TinVM* vm)
{
    tin_vmexec_resetvm(state, vm);
    vm->globals = tin_create_map(state);
    vm->modules = tin_create_map(state);
}

void tin_vm_destroy(TinVM* vm)
{
    tin_table_destroy(vm->state, &vm->gcstrings);
    tin_object_destroylistof(vm->state, vm->gcobjects);
    tin_vmexec_resetvm(vm->state, vm);
}

void tin_vm_tracestack(TinVM* vm, TinWriter* wr)
{
    TinValue* top;
    TinValue* slot;
    TinFiber* fiber;
    fiber = vm->fiber;
    if(fiber->stack_top == fiber->stack || fiber->frame_count == 0)
    {
        return;
    }
    top = fiber->frames[fiber->frame_count - 1].slots;
    tin_writer_writeformat(wr, "        | %s", COLOR_GREEN);
    for(slot = fiber->stack; slot < top; slot++)
    {
        tin_writer_writeformat(wr, "[ ");
        tin_towriter_value(vm->state, wr, *slot, true);
        tin_writer_writeformat(wr, " ]");
    }
    tin_writer_writeformat(wr, "%s", COLOR_RESET);
    for(slot = top; slot < fiber->stack_top; slot++)
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
            vm->fiber->stack_top -= fiber->arg_count;
            vm->fiber->stack_top[-1] = errval;
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
    count = (int)fiber->frame_count - 1;
    length = snprintf(NULL, 0, "%s%s\n", COLOR_RED, errorstring->chars);
    for(i = count; i >= 0; i--)
    {
        frame = &fiber->frames[i];
        function = frame->function;
        chunk = &function->chunk;
        name = function->name == NULL ? "unknown" : function->name->chars;

        if(chunk->has_line_info)
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
    start = buffer + sprintf(buffer, "%s%s\n", COLOR_RED, errorstring->chars);
    for(i = count; i >= 0; i--)
    {
        frame = &fiber->frames[i];
        function = frame->function;
        chunk = &function->chunk;
        name = function->name == NULL ? "unknown" : function->name->chars;
        if(chunk->has_line_info)
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
    tin_vmexec_resetstack(vm);
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
    tin_vmutil_callexitjump();
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
    //if(fiber->frame_count == TIN_CALL_FRAMES_MAX)
    //{
        //tin_vm_raiseerror(vm, "tin_vm_callcallable stack overflow");
        //return true;
    //}
    #endif
    if(fiber->frame_count + 1 > fiber->frame_capacity)
    {
        //newcapacity = fmin(TIN_CALL_FRAMES_MAX, fiber->frame_capacity * 2);
        newcapacity = (fiber->frame_capacity * 2);
        newsize = (sizeof(TinCallFrame) * newcapacity);
        osize = (sizeof(TinCallFrame) * fiber->frame_capacity);
        fiber->frames = (TinCallFrame*)tin_gcmem_memrealloc(vm->state, fiber->frames, osize, newsize);
        fiber->frame_capacity = newcapacity;
    }

    functionargcount = function->arg_count;
    tin_ensure_fiber_stack(vm->state, fiber, function->maxslots + (int)(fiber->stack_top - fiber->stack));
    frame = &fiber->frames[fiber->frame_count++];
    frame->function = function;
    frame->closure = closure;
    frame->ip = function->chunk.code;
    frame->slots = fiber->stack_top - argc - 1;
    frame->result_ignored = false;
    frame->return_to_c = false;
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
                tin_vm_push(vm, tin_value_fromobject(tin_create_array(vm->state)));
            }
        }
        else if(function->vararg)
        {
            array = tin_create_array(vm->state);
            varargcount = argc - functionargcount + 1;
            tin_state_pushroot(vm->state, (TinObject*)array);
            tin_vallist_ensuresize(vm->state, &array->list, varargcount);
            tin_state_poproot(vm->state);
            for(i = 0; i < varargcount; i++)
            {
                tin_vallist_set(&array->list, i, vm->fiber->stack_top[(int)i - (int)varargcount]);
            }
            vm->fiber->stack_top -= varargcount;
            tin_vm_push(vm, tin_value_fromobject(array));
        }
        else
        {
            vm->fiber->stack_top -= (argc - functionargcount);
        }
    }
    else if(function->vararg)
    {
        array = tin_create_array(vm->state);
        varargcount = argc - functionargcount + 1;
        tin_state_pushroot(vm->state, (TinObject*)array);
        tin_vallist_push(vm->state, &array->list, *(fiber->stack_top - 1));
        *(fiber->stack_top - 1) = tin_value_fromobject(array);
        tin_state_poproot(vm->state);
    }
    return true;
}

const char* tin_vmexec_funcnamefromvalue(TinExecState* est, TinValue v)
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

bool tin_vm_callvalue(TinExecState* est, TinValue callee, uint8_t argc)
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
        if(tin_vmutil_setexitjump())
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
                    vm_pushgc(est, false);
                    result = tin_value_asnativefunction(callee)->function(est->vm, argc, est->vm->fiber->stack_top - argc);
                    est->vm->fiber->stack_top -= argc + 1;
                    tin_vm_push(est->vm, result);
                    vm_popgc(est);
                    return false;
                }
                break;
            case TINTYPE_NATIVEPRIMITIVE:
                {
                    vm_pushgc(est, false);
                    est->fiber = est->vm->fiber;
                    bres = tin_value_asnativeprimitive(callee)->function(est->vm, argc, est->fiber->stack_top - argc);
                    if(bres)
                    {
                        est->fiber->stack_top -= argc;
                    }
                    vm_popgc(est);
                    return bres;
                }
                break;
            case TINTYPE_NATIVEMETHOD:
                {
                    vm_pushgc(est, false);
                    mthobj = tin_value_asnativemethod(callee);
                    est->fiber = est->vm->fiber;
                    result = mthobj->method(est->vm, *(est->vm->fiber->stack_top - argc - 1), argc, est->vm->fiber->stack_top - argc);
                    est->vm->fiber->stack_top -= argc + 1;
                    //if(!tin_value_isnull(result))
                    {
                        if(!est->vm->fiber->abort)
                        {
                            tin_vm_push(est->vm, result);
                        }
                    }
                    vm_popgc(est);
                    return false;
                }
                break;
            case TINTYPE_PRIMITIVEMETHOD:
                {
                    vm_pushgc(est, false);
                    est->fiber = est->vm->fiber;
                    bres = tin_value_asprimitivemethod(callee)->method(est->vm, *(est->fiber->stack_top - argc - 1), argc, est->fiber->stack_top - argc);
                    if(bres)
                    {
                        est->fiber->stack_top -= argc;
                    }
                    vm_popgc(est);
                    return bres;
                }
                break;
            case TINTYPE_CLASS:
                {
                    klass = tin_value_asclass(callee);
                    instance = tin_create_instance(est->vm->state, klass);
                    est->vm->fiber->stack_top[-argc - 1] = tin_value_fromobject(instance);
                    if(klass->init_method != NULL)
                    {
                        return tin_vm_callvalue(est, tin_value_fromobject(klass->init_method), argc);
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
                        vm_pushgc(est, false);
                        result = tin_value_asnativemethod(mthval)->method(est->vm, boundmethod->receiver, argc, est->vm->fiber->stack_top - argc);
                        est->vm->fiber->stack_top -= argc + 1;
                        tin_vm_push(est->vm, result);
                        vm_popgc(est);
                        return false;
                    }
                    else if(tin_value_isprimmethod(mthval))
                    {
                        est->fiber = est->vm->fiber;
                        vm_pushgc(est, false);
                        if(tin_value_asprimitivemethod(mthval)->method(est->vm, boundmethod->receiver, argc, est->fiber->stack_top - argc))
                        {
                            est->fiber->stack_top -= argc;
                            return true;
                        }
                        vm_popgc(est);
                        return false;
                    }
                    else
                    {
                        est->vm->fiber->stack_top[-argc - 1] = boundmethod->receiver;
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
        fromval = tin_vmexec_peek(est, 0);
    }
    fname = "unknown";
    fprintf(stderr, "fromval type=%d %s\n", tin_value_type(fromval), tin_tostring_typename(fromval));
    if(tin_value_isfunction(fromval))
    {
        fname = tin_vmexec_funcnamefromvalue(est, fromval);
    }
    else if(tin_value_isstring(fromval))
    {
        fname = tin_value_ascstring(fromval);
    }
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

TinUpvalue* tin_execvm_captureupvalue(TinState* state, TinValue* local)
{
    TinUpvalue* upvalue;
    TinUpvalue* createdupvalue;
    TinUpvalue* previousupvalue;
    previousupvalue = NULL;
    upvalue = state->vm->fiber->open_upvalues;
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
        state->vm->fiber->open_upvalues = createdupvalue;
    }
    else
    {
        previousupvalue->next = createdupvalue;
    }
    return createdupvalue;
}

void tin_vm_closeupvalues(TinVM* vm, const TinValue* last)
{
    TinFiber* fiber;
    TinUpvalue* upvalue;
    fiber = vm->fiber;
    while(fiber->open_upvalues != NULL && fiber->open_upvalues->location >= last)
    {
        upvalue = fiber->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        fiber->open_upvalues = upvalue->next;
    }
}

TinInterpretResult tin_vm_execmodule(TinState* state, TinModule* module)
{
    TinVM* vm;
    TinFiber* fiber;
    TinInterpretResult result;
    vm = state->vm;
    fiber = tin_create_fiber(state, module, module->main_function);
    vm->fiber = fiber;
    tin_vm_push(vm, tin_value_fromobject(module->main_function));
    result = tin_vm_execfiber(state, fiber);
    return result;
}


double tin_util_uinttofloat(unsigned int val);
unsigned int tin_util_floattouint(double val);
int tin_util_numbertoint32(double n);
int tin_util_doubletoint(double n);
unsigned int tin_util_numbertouint32(double n);


int vmutil_numtoint32(TinValue val)
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

unsigned int vmutil_numtouint32(TinValue val)
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

static inline bool vm_binaryop_actual(TinVM* vm, TinFiber* fiber, int op, TinValue a, TinValue b)
{
    int64_t ia;
    int64_t ib;
    bool eq;
    TinValue res;
    switch(op)
    {
        case OP_ADD:
            {
                if(a.isfixednumber && b.isfixednumber)
                {
                    res = tin_value_makefixednumber(vm->state, tin_value_asfixednumber(a) + tin_value_asfixednumber(b));
                }
                else if(!a.isfixednumber && !b.isfixednumber)
                {
                    res = tin_value_makefloatnumber(vm->state, tin_value_asfloatnumber(a) + tin_value_asfloatnumber(b));
                }
                else
                {
                    res = tin_value_makefloatnumber(vm->state, tin_value_asnumber(a) + tin_value_asnumber(b));
                }
                *(fiber->stack_top - 1) = res;
            }
            break;
        case OP_SUBTRACT:
            {
                if(a.isfixednumber && b.isfixednumber)
                {
                    res = tin_value_makefixednumber(vm->state, tin_value_asfixednumber(a) - tin_value_asfixednumber(b));
                }
                else if(!a.isfixednumber && !b.isfixednumber)
                {
                    res = tin_value_makefloatnumber(vm->state, tin_value_asfloatnumber(a) - tin_value_asfloatnumber(b));
                }
                else
                {
                    res = tin_value_makefloatnumber(vm->state, tin_value_asnumber(a) - tin_value_asnumber(b));
                }
                *(fiber->stack_top - 1) = res;
            }
            break;
        case OP_MULTIPLY:
            {
                if(a.isfixednumber && b.isfixednumber)
                {
                    res = tin_value_makefixednumber(vm->state, tin_value_asfixednumber(a) * tin_value_asfixednumber(b));
                }
                else if(!a.isfixednumber && !b.isfixednumber)
                {
                    res = tin_value_makefloatnumber(vm->state, tin_value_asfloatnumber(a) * tin_value_asfloatnumber(b));
                }
                else
                {
                    res = tin_value_makefloatnumber(vm->state, tin_value_asnumber(a) * tin_value_asnumber(b));
                }
                *(fiber->stack_top - 1) = res;
            }
            break;
        case OP_DIVIDE:
            {
                if(a.isfixednumber && b.isfixednumber)
                {
                    res = tin_value_makefixednumber(vm->state, tin_value_asfixednumber(a) / tin_value_asfixednumber(b));
                }
                else if(!a.isfixednumber && !b.isfixednumber)
                {
                    res = tin_value_makefloatnumber(vm->state, tin_value_asfloatnumber(a) / tin_value_asfloatnumber(b));
                }
                else
                {
                    res = tin_value_makefloatnumber(vm->state, tin_value_asnumber(a) / tin_value_asnumber(b));
                }
                *(fiber->stack_top - 1) = res;
            }
            break;
        case OP_BAND:
            {
                ia = tin_value_asfixednumber(a);
                ib = tin_value_asfixednumber(b);
                *(fiber->stack_top - 1) = (tin_value_makefixednumber(vm->state, ia & ib));
            }
            break;
        case OP_BOR:
            {
                ia = tin_value_asfixednumber(a);
                ib = tin_value_asfixednumber(b);
                *(fiber->stack_top - 1) = (tin_value_makefixednumber(vm->state, ia | ib));
            }
            break;
        case OP_BXOR:
            {
                ia = tin_value_asfixednumber(a);
                ib = tin_value_asfixednumber(b);
                *(fiber->stack_top - 1) = (tin_value_makefixednumber(vm->state, ia ^ ib));
            }
            break;
        case OP_LSHIFT:
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
                *(fiber->stack_top - 1) = tin_value_makefixednumber(vm->state, ires);
            }
            break;
        case OP_RSHIFT:
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
                *(fiber->stack_top - 1) = tin_value_makefixednumber(vm->state, ires);
            }
            break;
        case OP_EQUAL:
            {
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
                *(fiber->stack_top - 1) = tin_value_makebool(vm->state, eq);
            }
            break;
        case OP_GREATER:
            {
                *(fiber->stack_top - 1) = (tin_value_makebool(vm->state, tin_value_asnumber(a) > tin_value_asnumber(b)));
            }
            break;
        case OP_GREATER_EQUAL:
            {
                *(fiber->stack_top - 1) = (tin_value_makebool(vm->state, tin_value_asnumber(a) >= tin_value_asnumber(b)));
            }
            break;
        case OP_LESS:
            {
                *(fiber->stack_top - 1) = (tin_value_makebool(vm->state, tin_value_asnumber(a) < tin_value_asnumber(b)));
            }
            break;
        case OP_LESS_EQUAL:
            {
                *(fiber->stack_top - 1) = (tin_value_makebool(vm->state, tin_value_asnumber(a) <= tin_value_asnumber(b)));
            }
            break;
        default:
            {
                fprintf(stderr, "unhandled instruction in vm_binaryop\n");
                return false;
            }
            break;
    }
    return true;
}

//(state, vm, est, fiber)
bool tin_vmdo_call(TinExecState* est, TinValue* finalresult)
{
    size_t argc;
    TinValue peeked;
    argc = tin_vmexec_readbyte(est);
    tin_vmexec_writeframe(est, est->ip);
    peeked = tin_vmexec_peek(est, argc);
    vm_callvalue(peeked, argc);
    return true;
}

TinInterpretResult tin_vm_execfiber(TinState* state, TinFiber* fiber)
{
    TinValue finalresult;
    if(!tin_vm_internexecfiber(state, fiber, &finalresult))
    {
        return (TinInterpretResult){ TINSTATE_RUNTIMEERROR, tin_value_makenull(state) };
    }
    return (TinInterpretResult){ TINSTATE_OK, finalresult };
}

bool tin_vm_internexecfiber(TinState* exstate, TinFiber* fiber, TinValue* finalresult)
{
    //vartop
    uint8_t instruction;
    uint8_t nowinstr;
    TinValList* values;
    TinExecState eststack;
    TinExecState* est;
    TinVM* vm;
    vm = exstate->vm;
    vm->fiber = fiber;
    fiber->abort = false;
    est = &eststack;
    est->fiber = fiber;
    est->state = exstate;
    est->vm = vm;
    est->frame = &est->fiber->frames[est->fiber->frame_count - 1];
    est->currentchunk = &est->frame->function->chunk;
    est->fiber->module = est->frame->function->module;
    est->ip = est->frame->ip;
    est->slots = est->frame->slots;
    est->privates = est->fiber->module->privates;
    est->upvalues = est->frame->closure == NULL ? NULL : est->frame->closure->upvalues;
    vm_pushgc(est, true);

    // Has to be inside of the function in order for goto to work
    #ifdef TIN_USE_COMPUTEDGOTO
        static void* dispatchtable[] =
        {
            #define OPCODE(name, effect) &&OP_##name,
            #include "opcodes.inc"
            #undef OPCODE
        };
    #endif
#ifdef TIN_TRACE_EXECUTION
    vm_traceframe(est->fiber);
#endif

    while(true)
    {
#ifdef TIN_TRACE_STACK
        tin_vm_tracestack(vm);
#endif

#ifdef TIN_CHECK_STACK_SIZE
        if((est->fiber->stack_top - est->frame->slots) > est->fiber->stack_capacity)
        {
            vmexec_raiseerrorfmt("fiber stack too small (%i > %i)", (int)(est->fiber->stack_top - est->frame->slots),
                               est->fiber->stack_capacity);
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
                tin_vmexec_drop(est->fiber);
                continue;
            }
            op_case(OP_RETURN)
            {
                size_t argc;
                TinValue result;
                TinFiber* parent;
                result = tin_vmexec_pop(est->fiber);
                tin_vm_closeupvalues(vm, est->slots);
                tin_vmexec_writeframe(est, est->ip);
                est->fiber->frame_count--;
                if(est->frame->return_to_c)
                {
                    est->frame->return_to_c = false;
                    est->fiber->module->return_value = result;
                    est->fiber->stack_top = est->frame->slots;
                    *finalresult = result;
                    return true;
                }
                if(est->fiber->frame_count == 0)
                {
                    est->fiber->module->return_value = result;
                    if(est->fiber->parent == NULL)
                    {
                        tin_vmexec_drop(est->fiber);
                        est->state->gcallow = est->wasallowed;
                        *finalresult = result;
                        return true;
                    }
                    argc = est->fiber->arg_count;
                    parent = est->fiber->parent;
                    est->fiber->parent = NULL;
                    vm->fiber = est->fiber = parent;
                    tin_vmexec_readframe(est->fiber, est);
                    vm_traceframe(est->fiber);
                    est->fiber->stack_top -= argc;
                    est->fiber->stack_top[-1] = result;
                    continue;
                }
                est->fiber->stack_top = est->frame->slots;
                if(est->frame->result_ignored)
                {
                    est->fiber->stack_top++;
                    est->frame->result_ignored = false;
                }
                else
                {
                    tin_vmexec_push(est->fiber, result);
                }
                tin_vmexec_readframe(est->fiber, est);
                vm_traceframe(est->fiber);
                continue;
            }
            op_case(OP_CONSTANT)
            {
                tin_vmexec_push(est->fiber, tin_vmexec_readconstant(est));
                continue;
            }
            op_case(OP_CONSTANT_LONG)
            {
                tin_vmexec_push(est->fiber, tin_vmexec_readconstantlong(est));
                continue;
            }
            op_case(OP_TRUE)
            {
                tin_vmexec_push(est->fiber, TRUE_VALUE);
                continue;
            }
            op_case(OP_FALSE)
            {
                tin_vmexec_push(est->fiber, FALSE_VALUE);
                continue;
            }
            op_case(OP_NULL)
            {
                tin_vmexec_push(est->fiber, tin_value_makenull(vm->state));
                continue;
            }
            op_case(OP_ARRAY)
            {
                tin_vmexec_push(est->fiber, tin_value_fromobject(tin_create_array(est->state)));
                continue;
            }
            op_case(OP_OBJECT)
            {
                // TODO: use object, or map for literal '{...}' constructs?
                // objects would be more general-purpose, but don't implement anything map-like.
                //tin_vmexec_push(est->fiber, tin_value_fromobject(tin_create_instance(state, state->primobjectclass)));
                tin_vmexec_push(est->fiber, tin_value_fromobject(tin_create_map(est->state)));
                continue;
            }
            op_case(OP_RANGE)
            {
                TinValue vala;
                TinValue valb;
                vala = tin_vmexec_pop(est->fiber);
                valb = tin_vmexec_pop(est->fiber);
                if(!tin_value_isnumber(vala) || !tin_value_isnumber(valb))
                {
                    vmexec_raiseerror("range fields must be numbers");
                }
                tin_vmexec_push(est->fiber, tin_value_fromobject(tin_object_makerange(est->state, tin_value_asnumber(vala), tin_value_asnumber(valb))));
                continue;
            }
            op_case(OP_NEGATE)
            {
                TinValue tmpval;
                TinValue popped;
                if(!tin_value_isnumber(tin_vmexec_peek(est, 0)))
                {
                    vmexec_raiseerror("operand must be a number");
                }
                popped = tin_vmexec_pop(est->fiber);
                if(popped.isfixednumber)
                {
                    tmpval = tin_value_makefixednumber(vm->state, -tin_value_asfixednumber(popped));
                }
                else
                {
                    tmpval = tin_value_makefloatnumber(vm->state, -tin_value_asfloatnumber(popped));
                }
                tin_vmexec_push(est->fiber, tmpval);
                continue;
            }
            op_case(OP_NOT)
            {
                TinValue tmpval;
                TinValue popped;
                if(tin_value_isinstance(tin_vmexec_peek(est, 0)))
                {
                    tin_vmexec_writeframe(est, est->ip);
                    vmexec_invokefromclass(tin_value_asinstance(tin_vmexec_peek(est, 0))->klass, tin_string_copyconst(est->state, "!"), 0, false, methods, false);
                    continue;
                }
                popped = tin_vmexec_pop(est->fiber);
                tmpval = tin_value_makebool(vm->state, tin_value_isfalsey(popped));
                tin_vmexec_push(est->fiber, tmpval);
                continue;
            }
            op_case(OP_BNOT)
            {
                int64_t ival;
                TinValue tmpval;
                TinValue popped;
                if(!tin_value_isnumber(tin_vmexec_peek(est, 0)))
                {
                    vmexec_raiseerror("Operand must be a number");
                }
                popped = tin_vmexec_pop(est->fiber);
                ival = (int)tin_value_asnumber(popped);
                tmpval = tin_value_makefixednumber(vm->state, ~ival);
                tin_vmexec_push(est->fiber, tmpval);
                continue;
            }
            op_case(OP_ADD)
            {
                vm_binaryop(nowinstr, "+");
                continue;
            }
            op_case(OP_SUBTRACT)
            {
                vm_binaryop(nowinstr, "-");
                continue;
            }
            op_case(OP_MULTIPLY)
            {
                vm_binaryop(nowinstr, "*");
                continue;
            }
            op_case(OP_POWER)
            {
                TinValue vala;
                TinValue valb;
                vala = tin_vmexec_peek(est, 1);
                valb = tin_vmexec_peek(est, 0);
                if(tin_value_isnumber(vala) && tin_value_isnumber(valb))
                {
                    tin_vmexec_drop(est->fiber);
                    *(est->fiber->stack_top - 1) = tin_value_makefloatnumber(vm->state, pow(tin_value_asnumber(vala), tin_value_asnumber(valb)));
                }
                else
                {
                    vm_invokemethod(vala, "**", 1);
                }
                continue;
            }
            op_case(OP_DIVIDE)
            {
                vm_binaryop(nowinstr, "/");
                continue;
            }
            op_case(OP_FLOOR_DIVIDE)
            {
                TinValue vala;
                TinValue valb;
                vala = tin_vmexec_peek(est, 1);
                valb = tin_vmexec_peek(est, 0);
                if(tin_value_isnumber(vala) && tin_value_isnumber(valb))
                {
                    tin_vmexec_drop(est->fiber);
                    *(est->fiber->stack_top - 1) = tin_value_makefloatnumber(vm->state, floor(tin_value_asnumber(vala) / tin_value_asnumber(valb)));
                }
                else
                {
                    vm_invokemethod(vala, "#", 1);
                }
                continue;
            }
            op_case(OP_MOD)
            {
                TinValue vala;
                TinValue valb;
                vala = tin_vmexec_peek(est, 1);
                valb = tin_vmexec_peek(est, 0);
                if(tin_value_isnumber(vala) && tin_value_isnumber(valb))
                {
                    tin_vmexec_drop(est->fiber);
                    *(est->fiber->stack_top - 1) = tin_value_makefloatnumber(vm->state, fmod(tin_value_asnumber(vala), tin_value_asnumber(valb)));
                }
                else
                {
                    vm_invokemethod(vala, "%", 1);
                }
                continue;
            }
            op_case(OP_BAND)
            {
                vm_binaryop(nowinstr, "&");
                continue;
            }
            op_case(OP_BOR)
            {
                vm_binaryop(nowinstr, "|");
                continue;
            }
            op_case(OP_BXOR)
            {
                vm_binaryop(nowinstr, "^");
                continue;
            }
            op_case(OP_LSHIFT)
            {
                vm_binaryop(nowinstr, "<<");
                continue;
            }
            op_case(OP_RSHIFT)
            {
                vm_binaryop(nowinstr, ">>");
                continue;
            }
            op_case(OP_EQUAL)
            {
                vm_binaryop(nowinstr, "==");
                continue;
            }
            op_case(OP_GREATER)
            {
                vm_binaryop(nowinstr, ">");
                continue;
            }
            op_case(OP_GREATER_EQUAL)
            {
                vm_binaryop(nowinstr, ">=");
                continue;
            }
            op_case(OP_LESS)
            {
                vm_binaryop(nowinstr, "<");
                continue;
            }
            op_case(OP_LESS_EQUAL)
            {
                vm_binaryop(nowinstr, "<=");
                continue;
            }
            op_case(OP_SET_GLOBAL)
            {
                TinString* name;
                name = tin_vmexec_readstringlong(est);
                tin_table_set(est->state, &vm->globals->values, name, tin_vmexec_peek(est, 0));
                continue;
            }
            op_case(OP_GET_GLOBAL)
            {
                TinValue setval;
                TinString* name;
                name = tin_vmexec_readstringlong(est);
                //fprintf(stderr, "GET_GLOBAL: %s\n", name->chars);
                if(!tin_table_get(&vm->globals->values, name, &setval))
                {
                    tin_vmexec_push(est->fiber, tin_value_makenull(vm->state));
                }
                else
                {
                    tin_vmexec_push(est->fiber, setval);
                }
                continue;
            }
            op_case(OP_SET_LOCAL)
            {
                uint8_t index;
                index = tin_vmexec_readbyte(est);
                est->slots[index] = tin_vmexec_peek(est, 0);
                continue;
            }
            op_case(OP_GET_LOCAL)
            {
                tin_vmexec_push(est->fiber, est->slots[tin_vmexec_readbyte(est)]);
                continue;
            }
            op_case(OP_SET_LOCAL_LONG)
            {
                uint8_t index;
                index = tin_vmexec_readshort(est);
                est->slots[index] = tin_vmexec_peek(est, 0);
                continue;
            }
            op_case(OP_GET_LOCAL_LONG)
            {
                tin_vmexec_push(est->fiber, est->slots[tin_vmexec_readshort(est)]);
                continue;
            }
            op_case(OP_SET_PRIVATE)
            {
                uint8_t index;
                index = tin_vmexec_readbyte(est);
                est->privates[index] = tin_vmexec_peek(est, 0);
                continue;
            }
            op_case(OP_GET_PRIVATE)
            {
                tin_vmexec_push(est->fiber, est->privates[tin_vmexec_readbyte(est)]);
                continue;
            }
            op_case(OP_SET_PRIVATE_LONG)
            {
                uint8_t index;
                index = tin_vmexec_readshort(est);
                est->privates[index] = tin_vmexec_peek(est, 0);
                continue;
            }
            op_case(OP_GET_PRIVATE_LONG)
            {
                tin_vmexec_push(est->fiber, est->privates[tin_vmexec_readshort(est)]);
                continue;
            }
            op_case(OP_SET_UPVALUE)
            {
                uint8_t index;
                index = tin_vmexec_readbyte(est);
                *est->upvalues[index]->location = tin_vmexec_peek(est, 0);
                continue;
            }
            op_case(OP_GET_UPVALUE)
            {
                tin_vmexec_push(est->fiber, *est->upvalues[tin_vmexec_readbyte(est)]->location);
                continue;
            }
            op_case(OP_JUMP_IF_FALSE)
            {
                uint16_t offset;
                TinValue popped;
                offset = tin_vmexec_readshort(est);
                popped = tin_vmexec_pop(est->fiber);
                if(tin_value_isfalsey(popped))
                {
                    est->ip += offset;
                }
                continue;
            }
            op_case(OP_JUMP_IF_NULL)
            {
                uint16_t offset;
                offset = tin_vmexec_readshort(est);
                if(tin_value_isnull(tin_vmexec_peek(est, 0)))
                {
                    est->ip += offset;
                }
                continue;
            }
            op_case(OP_JUMP_IF_NULL_POPPING)
            {
                uint16_t offset;
                TinValue popped;
                offset = tin_vmexec_readshort(est);
                popped = tin_vmexec_pop(est->fiber);
                if(tin_value_isnull(popped))
                {
                    est->ip += offset;
                }
                continue;
            }
            op_case(OP_JUMP)
            {
                uint16_t offset;
                offset = tin_vmexec_readshort(est);
                est->ip += offset;
                continue;
            }
            op_case(OP_JUMP_BACK)
            {
                uint16_t offset;
                offset = tin_vmexec_readshort(est);
                est->ip -= offset;
                continue;
            }
            op_case(OP_AND)
            {
                uint16_t offset;
                offset = tin_vmexec_readshort(est);
                if(tin_value_isfalsey(tin_vmexec_peek(est, 0)))
                {
                    est->ip += offset;
                }
                else
                {
                    tin_vmexec_drop(est->fiber);
                }
                continue;
            }
            op_case(OP_OR)
            {
                uint16_t offset;
                offset = tin_vmexec_readshort(est);
                if(tin_value_isfalsey(tin_vmexec_peek(est, 0)))
                {
                    tin_vmexec_drop(est->fiber);
                }
                else
                {
                    est->ip += offset;
                }
                continue;
            }
            op_case(OP_NULL_OR)
            {
                uint16_t offset;
                offset = tin_vmexec_readshort(est);
                if(tin_value_isnull(tin_vmexec_peek(est, 0)))
                {
                    tin_vmexec_drop(est->fiber);
                }
                else
                {
                    est->ip += offset;
                }
                continue;
            }
            op_case(OP_CALL)
            {
                if(!tin_vmdo_call(est, finalresult))
                {
                    return false;
                }
                continue;
            }
            op_case(OP_CLOSURE)
            {
                size_t i;
                uint8_t index;
                uint8_t islocal;
                TinClosure* closure;
                TinFunction* function;
                function = tin_value_asfunction(tin_vmexec_readconstantlong(est));
                closure = tin_object_makeclosure(est->state, function);
                tin_vmexec_push(est->fiber, tin_value_fromobject(closure));
                for(i = 0; i < closure->upvalue_count; i++)
                {
                    islocal = tin_vmexec_readbyte(est);
                    index = tin_vmexec_readbyte(est);
                    if(islocal)
                    {
                        closure->upvalues[i] = tin_execvm_captureupvalue(est->state, est->frame->slots + index);
                    }
                    else
                    {
                        closure->upvalues[i] = est->upvalues[index];
                    }
                }
                continue;
            }
            op_case(OP_CLOSE_UPVALUE)
            {
                tin_vm_closeupvalues(vm, est->fiber->stack_top - 1);
                tin_vmexec_drop(est->fiber);
                continue;
            }
            op_case(OP_CLASS)
            {
                TinString* name;
                TinClass* klassobj;
                name = tin_vmexec_readstringlong(est);
                klassobj = tin_create_class(est->state, name);
                tin_vmexec_push(est->fiber, tin_value_fromobject(klassobj));
                klassobj->super = est->state->primobjectclass;
                tin_table_add_all(est->state, &klassobj->super->methods, &klassobj->methods);
                tin_table_add_all(est->state, &klassobj->super->static_fields, &klassobj->static_fields);
                tin_table_set(est->state, &vm->globals->values, name, tin_value_fromobject(klassobj));
                continue;
            }
            op_case(OP_GET_FIELD)
            {
                TinValue tmpval;
                TinValue object;
                TinValue getval;
                TinString* name;
                TinClass* klassobj;
                TinField* field;
                TinInstance* instobj;
                object = tin_vmexec_peek(est, 1);
                name = tin_value_asstring(tin_vmexec_peek(est, 0));
                if(tin_value_isnull(object))
                {
                    vmexec_raiseerrorfmt("attempt to set field '%s' of a null value", name->chars);
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
                                    vmexec_raiseerrorfmt("class %s does not have a getter for field '%s'",
                                                       instobj->klass->name->chars, name->chars);
                                }
                                tin_vmexec_drop(est->fiber);
                                tin_vmexec_writeframe(est, est->ip);
                                field = tin_value_asfield(getval);
                                tmpval =tin_value_fromobject(field->getter);
                                vm_callvalue(tmpval, 0);
                                tin_vmexec_readframe(est->fiber, est);
                                continue;
                            }
                            else
                            {
                                getval = tin_value_fromobject(tin_object_makeboundmethod(est->state, tin_value_fromobject(instobj), getval));
                            }
                        }
                        else
                        {
                            getval = tin_value_makenull(vm->state);
                        }
                    }
                }
                else if(tin_value_isclass(object))
                {
                    klassobj = tin_value_asclass(object);
                    if(tin_table_get(&klassobj->static_fields, name, &getval))
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
                                vmexec_raiseerrorfmt("class %s does not have a getter for field '%s'", klassobj->name->chars,
                                                   name->chars);
                            }
                            tin_vmexec_drop(est->fiber);
                            tin_vmexec_writeframe(est, est->ip);
                            tmpval = tin_value_fromobject(field->getter);
                            vm_callvalue(tmpval, 0);
                            tin_vmexec_readframe(est->fiber, est);
                            continue;
                        }
                    }
                    else
                    {
                        getval = tin_value_makenull(vm->state);
                    }
                }
                else
                {
                    klassobj = tin_state_getclassfor(est->state, object);
                    if(klassobj == NULL)
                    {
                        vmexec_raiseerrorfmt("GET_FIELD: cannot get class object for type '%s'", tin_tostring_typename(object));
                    }
                    if(tin_table_get(&klassobj->methods, name, &getval))
                    {
                        if(tin_value_isfield(getval))
                        {
                            field = tin_value_asfield(getval);
                            if(field->getter == NULL)
                            {
                                vmexec_raiseerrorfmt("class %s does not have a getter for field '%s'", klassobj->name->chars,
                                                   name->chars);
                            }
                            tin_vmexec_drop(est->fiber);
                            tin_vmexec_writeframe(est, est->ip);
                            tmpval = tin_value_fromobject(tin_value_asfield(getval)->getter);
                            vm_callvalue(tmpval, 0);
                            tin_vmexec_readframe(est->fiber, est);
                            continue;
                        }
                        else if(tin_value_isnatmethod(getval) || tin_value_isprimmethod(getval))
                        {
                            getval = tin_value_fromobject(tin_object_makeboundmethod(est->state, object, getval));
                        }
                    }
                    else
                    {
                        getval = tin_value_makenull(vm->state);
                    }
                }
                tin_vmexec_drop(est->fiber);// Pop field name
                est->fiber->stack_top[-1] = getval;
                continue;
            }
            op_case(OP_SET_FIELD)
            {
                TinValue value;
                TinValue tmpval;
                TinValue setter;
                TinValue instval;
                TinClass* klassobj;
                TinField* field;
                TinString* fieldname;
                TinInstance* instobj;
                instval = tin_vmexec_peek(est, 2);
                value = tin_vmexec_peek(est, 1);
                fieldname = tin_value_asstring(tin_vmexec_peek(est, 0));
                if(tin_value_isnull(instval))
                {
                    vmexec_raiseerrorfmt("attempt to set field '%s' of a null value", fieldname->chars)
                }
                if(tin_value_isclass(instval))
                {
                    klassobj = tin_value_asclass(instval);
                    if(tin_table_get(&klassobj->static_fields, fieldname, &setter) && tin_value_isfield(setter))
                    {
                        field = tin_value_asfield(setter);
                        if(field->setter == NULL)
                        {
                            vmexec_raiseerrorfmt("class %s does not have a setter for field '%s'", klassobj->name->chars,
                                               fieldname->chars);
                        }

                        tin_vmexec_dropn(est->fiber, 2);
                        tin_vmexec_push(est->fiber, value);
                        tin_vmexec_writeframe(est, est->ip);
                        tmpval = tin_value_fromobject(field->setter);
                        vm_callvalue(tmpval, 1);
                        tin_vmexec_readframe(est->fiber, est);
                        continue;
                    }
                    if(tin_value_isnull(value))
                    {
                        tin_table_delete(&klassobj->static_fields, fieldname);
                    }
                    else
                    {
                        tin_table_set(est->state, &klassobj->static_fields, fieldname, value);
                    }
                    tin_vmexec_dropn(est->fiber, 2);// Pop field name and the value
                    est->fiber->stack_top[-1] = value;
                }
                else if(tin_value_isinstance(instval))
                {
                    instobj = tin_value_asinstance(instval);
                    if(tin_table_get(&instobj->klass->methods, fieldname, &setter) && tin_value_isfield(setter))
                    {
                        field = tin_value_asfield(setter);
                        if(field->setter == NULL)
                        {
                            vmexec_raiseerrorfmt("class %s does not have a setter for field '%s'", instobj->klass->name->chars,
                                               fieldname->chars);
                        }
                        tin_vmexec_dropn(est->fiber, 2);
                        tin_vmexec_push(est->fiber, value);
                        tin_vmexec_writeframe(est, est->ip);
                        tmpval = tin_value_fromobject(field->setter);
                        vm_callvalue(tmpval, 1);
                        tin_vmexec_readframe(est->fiber, est);
                        continue;
                    }
                    if(tin_value_isnull(value))
                    {
                        tin_table_delete(&instobj->fields, fieldname);
                    }
                    else
                    {
                        tin_table_set(est->state, &instobj->fields, fieldname, value);
                    }
                    tin_vmexec_dropn(est->fiber, 2);// Pop field name and the value
                    est->fiber->stack_top[-1] = value;
                }
                else
                {
                    klassobj = tin_state_getclassfor(est->state, instval);
                    if(klassobj == NULL)
                    {
                        vmexec_raiseerror("SET_FIELD: only instances and classes have fields");
                    }
                    if(tin_table_get(&klassobj->methods, fieldname, &setter) && tin_value_isfield(setter))
                    {
                        field = tin_value_asfield(setter);
                        if(field->setter == NULL)
                        {
                            vmexec_raiseerrorfmt("class '%s' does not have a setter for field '%s'", klassobj->name->chars,
                                               fieldname->chars);
                        }
                        tin_vmexec_dropn(est->fiber, 2);
                        tin_vmexec_push(est->fiber, value);
                        tin_vmexec_writeframe(est, est->ip);
                        tmpval = tin_value_fromobject(field->setter);
                        vm_callvalue(tmpval, 1);
                        tin_vmexec_readframe(est->fiber, est);
                        continue;
                    }
                    else
                    {
                        vmexec_raiseerrorfmt("class '%s' does not contain field '%s'", klassobj->name->chars, fieldname->chars);
                    }
                }
                continue;
            }
            op_case(OP_SUBSCRIPT_GET)
            {
                vm_invokemethod(tin_vmexec_peek(est, 1), "[]", 1);
                continue;
            }
            op_case(OP_SUBSCRIPT_SET)
            {
                vm_invokemethod(tin_vmexec_peek(est, 2), "[]", 2);
                continue;
            }
            op_case(OP_PUSH_ARRAY_ELEMENT)
            {
                size_t arindex;
                values = &tin_value_asarray(tin_vmexec_peek(est, 1))->list;
                arindex = tin_vallist_count(values);
                tin_vallist_ensuresize(est->state, values, arindex + 1);
                tin_vallist_set(values, arindex, tin_vmexec_peek(est, 0));
                tin_vmexec_drop(est->fiber);
                continue;
            }
            op_case(OP_PUSH_OBJECT_FIELD)
            {
                TinValue operand;
                TinValue peek0;
                TinValue peek1;
                TinMap* tmap;
                TinInstance* tinst;
                operand = tin_vmexec_peek(est, 2);
                peek0 = tin_vmexec_peek(est, 0);
                peek1 = tin_vmexec_peek(est, 1);
                if(tin_value_ismap(operand))
                {
                    tmap = tin_value_asmap(operand);
                    fprintf(stderr, "peek1=%s\n", tin_tostring_typename(peek1));
                    tin_table_set(est->state, &tmap->values, tin_value_asstring(peek1), peek0);
                }
                else if(tin_value_isinstance(operand))
                {
                    tinst =tin_value_asinstance(operand); 
                    tin_table_set(est->state, &tinst->fields, tin_value_asstring(peek1), peek0);
                }
                else
                {
                    vmexec_raiseerrorfmt("cannot set field '%s' on type '%s'", tin_tostring_typename(operand));
                }
                tin_vmexec_dropn(est->fiber, 2);
                continue;
            }
            op_case(OP_STATIC_FIELD)
            {
                tin_table_set(est->state, &tin_value_asclass(tin_vmexec_peek(est, 1))->static_fields, tin_vmexec_readstringlong(est), tin_vmexec_peek(est, 0));
                tin_vmexec_drop(est->fiber);
                continue;
            }
            op_case(OP_METHOD)
            {
                TinString* name;
                TinClass* klassobj;
                klassobj = tin_value_asclass(tin_vmexec_peek(est, 1));
                name = tin_vmexec_readstringlong(est);
                if((klassobj->init_method == NULL || (klassobj->super != NULL && klassobj->init_method == ((TinClass*)klassobj->super)->init_method))
                   && tin_string_getlength(name) == 11 && memcmp(name->chars, "constructor", 11) == 0)
                {
                    klassobj->init_method = tin_value_asobject(tin_vmexec_peek(est, 0));
                }
                tin_table_set(est->state, &klassobj->methods, name, tin_vmexec_peek(est, 0));
                tin_vmexec_drop(est->fiber);
                continue;
            }
            op_case(OP_DEFINE_FIELD)
            {
                tin_table_set(est->state, &tin_value_asclass(tin_vmexec_peek(est, 1))->methods, tin_vmexec_readstringlong(est), tin_vmexec_peek(est, 0));
                tin_vmexec_drop(est->fiber);
                continue;
            }
            op_case(OP_INVOKE)
            {
                vm_invokeoperation(false);
                continue;
            }
            op_case(OP_INVOKE_IGNORING)
            {
                vm_invokeoperation(true);
                continue;
            }
            op_case(OP_INVOKE_SUPER)
            {
                size_t argc;
                TinValue popped;
                TinClass* klassobj;
                TinString* mthname;
                argc = tin_vmexec_readbyte(est);
                mthname = tin_vmexec_readstringlong(est);
                popped = tin_vmexec_pop(est->fiber);
                klassobj = tin_value_asclass(popped);
                tin_vmexec_writeframe(est, est->ip);
                vmexec_invokefromclass(klassobj, mthname, argc, true, methods, false);
                continue;
            }
            op_case(OP_INVOKE_SUPER_IGNORING)
            {
                size_t argc;
                TinValue popped;
                TinClass* klassobj;
                TinString* mthname;
                argc = tin_vmexec_readbyte(est);
                mthname = tin_vmexec_readstringlong(est);
                popped = tin_vmexec_pop(est->fiber);
                klassobj = tin_value_asclass(popped);
                tin_vmexec_writeframe(est, est->ip);
                vmexec_invokefromclass(klassobj, mthname, argc, true, methods, true);
                continue;
            }
            op_case(OP_GET_SUPER_METHOD)
            {
                TinValue value;
                TinValue instval;
                TinValue popped;
                TinClass* klassobj;
                TinString* mthname;
                mthname = tin_vmexec_readstringlong(est);
                popped = tin_vmexec_pop(est->fiber);
                klassobj = tin_value_asclass(popped);
                instval = tin_vmexec_pop(est->fiber);
                if(tin_table_get(&klassobj->methods, mthname, &value))
                {
                    value = tin_value_fromobject(tin_object_makeboundmethod(est->state, instval, value));
                }
                else
                {
                    value = tin_value_makenull(vm->state);
                }
                tin_vmexec_push(est->fiber, value);
                continue;
            }
            op_case(OP_INHERIT)
            {
                TinValue super;
                TinClass* superklass;
                TinClass* klassobj;
                super = tin_vmexec_peek(est, 1);
                if(!tin_value_isclass(super))
                {
                    vmexec_raiseerror("superclass must be a class");
                }
                klassobj = tin_value_asclass(tin_vmexec_peek(est, 0));
                superklass = tin_value_asclass(super);
                klassobj->super = superklass;
                klassobj->init_method = superklass->init_method;
                tin_table_add_all(est->state, &superklass->methods, &klassobj->methods);
                tin_table_add_all(est->state, &klassobj->super->static_fields, &klassobj->static_fields);
                continue;
            }
            op_case(OP_IS)
            {
                bool found;
                TinValue klassval;
                TinValue instval;
                TinClass* type;
                TinClass* instanceklass;
                instval = tin_vmexec_peek(est, 1);
                if(tin_value_isnull(instval))
                {
                    tin_vmexec_dropn(est->fiber, 2);
                    tin_vmexec_push(est->fiber, FALSE_VALUE);

                    continue;
                }
                instanceklass = tin_state_getclassfor(est->state, instval);
                klassval = tin_vmexec_peek(est, 0);
                if(instanceklass == NULL || !tin_value_isclass(klassval))
                {
                    vmexec_raiseerror("operands must be an instance or a class");
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
                    instanceklass = (TinClass*)instanceklass->super;
                }
                tin_vmexec_dropn(est->fiber, 2);// Drop the instance and class
                tin_vmexec_push(est->fiber, tin_value_makebool(vm->state, found));
                continue;
            }
            op_case(OP_POP_LOCALS)
            {
                tin_vmexec_dropn(est->fiber, tin_vmexec_readshort(est));
                continue;
            }
            op_case(OP_VARARG)
            {
                size_t i;
                TinValue slot;
                slot = est->slots[tin_vmexec_readbyte(est)];
                if(!tin_value_isarray(slot))
                {
                    continue;
                }
                values = &tin_value_asarray(slot)->list;
                tin_ensure_fiber_stack(est->state, est->fiber, tin_vallist_count(values) + est->frame->function->maxslots + (int)(est->fiber->stack_top - est->fiber->stack));
                for(i = 0; i < tin_vallist_count(values); i++)
                {
                    tin_vmexec_push(est->fiber, tin_vallist_get(values, i));
                }
                // Hot-bytecode patching, increment the amount of arguments to OP_CALL
                est->ip[1] = est->ip[1] + tin_vallist_count(values) - 1;
                continue;
            }

            op_case(OP_REFERENCE_GLOBAL)
            {
                TinString* name;
                TinValue* pval;
                name = tin_vmexec_readstringlong(est);
                if(tin_table_get_slot(&vm->globals->values, name, &pval))
                {
                    tin_vmexec_push(est->fiber, tin_value_fromobject(tin_object_makereference(est->state, pval)));
                }
                else
                {
                    vmexec_raiseerror("attempt to reference a null value");
                }
                continue;
            }
            op_case(OP_REFERENCE_PRIVATE)
            {
                tin_vmexec_push(est->fiber, tin_value_fromobject(tin_object_makereference(est->state, &est->privates[tin_vmexec_readshort(est)])));
                continue;
            }
            op_case(OP_REFERENCE_LOCAL)
            {
                tin_vmexec_push(est->fiber, tin_value_fromobject(tin_object_makereference(est->state, &est->slots[tin_vmexec_readshort(est)])));
                continue;
            }
            op_case(OP_REFERENCE_UPVALUE)
            {
                tin_vmexec_push(est->fiber, tin_value_fromobject(tin_object_makereference(est->state, est->upvalues[tin_vmexec_readbyte(est)]->location)));
                continue;
            }
            op_case(OP_REFERENCE_FIELD)
            {
                TinValue object;
                TinString* name;
                TinValue* pval;
                object = tin_vmexec_peek(est, 1);
                if(tin_value_isnull(object))
                {
                    vmexec_raiseerror("attempt to index a null value");
                }
                name = tin_value_asstring(tin_vmexec_peek(est, 0));
                if(tin_value_isinstance(object))
                {
                    if(!tin_table_get_slot(&tin_value_asinstance(object)->fields, name, &pval))
                    {
                        vmexec_raiseerror("attempt to reference a null value");
                    }
                }
                else
                {
                    tin_towriter_value(est->state, &est->state->debugwriter, object, true);
                    printf("\n");
                    vmexec_raiseerrorfmt("cannot reference field '%s' of a non-instance", name->chars);
                }
                tin_vmexec_drop(est->fiber);// Pop field name
                est->fiber->stack_top[-1] = tin_value_fromobject(tin_object_makereference(est->state, pval));
                continue;
            }
            op_case(OP_SET_REFERENCE)
            {
                TinValue reference;
                reference = tin_vmexec_pop(est->fiber);
                if(!tin_value_isreference(reference))
                {
                    vmexec_raiseerror("cannot set reference value of a non-reference");
                }
                *tin_value_asreference(reference)->slot = tin_vmexec_peek(est, 0);
                continue;
            }
            vm_default()
            {
                vmexec_raiseerrorfmt("unknown VM op code '%d'", *est->ip);
                break;
            }
        }
    }
    vm_returnerror();
    return false;
}

void tin_vmutil_callexitjump()
{
    longjmp(jumpbuffer, 1);
}

bool tin_vmutil_setexitjump()
{
    return setjmp(jumpbuffer);
}


