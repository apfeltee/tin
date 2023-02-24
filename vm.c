
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
    vm_popgc(state); \
    return (TinInterpretResult){ TINRESULT_RUNTIME_ERROR, tin_value_makenull(state) };

#define vm_pushgc(state, allow) \
    bool wasallowed = state->gcallow; \
    state->gcallow = allow;

#define vm_popgc(state) \
    state->gcallow = wasallowed;

/*
* can't be turned into a function because it is expected to return in
* tin_vm_execfiber.
* might be possible to wrap this by using an enum to specify
* if (and what) to return, but it'll be quite a bit of work to refactor.
* likewise, any macro that uses vm_recoverstate can't be turned into
* a function.
*/
#define vm_recoverstate(fiber, est) \
    tin_vmexec_writeframe(&est, est.ip); \
    fiber = vm->fiber; \
    if(fiber == NULL) \
    { \
        return (TinInterpretResult){ TINRESULT_OK, tin_vmexec_pop(fiber) }; \
    } \
    if(fiber->abort) \
    { \
        vm_returnerror(); \
    } \
    tin_vmexec_readframe(fiber, &est); \
    vm_traceframe(fiber);

#define vm_callvalue(callee, argc) \
    if(tin_vm_callvalue(vm, fiber, &est, callee, argc)) \
    { \
        vm_recoverstate(fiber, est); \
    }

#define vmexec_raiseerrorfmt(format, ...) \
    if(tin_vm_raiseerror(vm, format, __VA_ARGS__)) \
    { \
        vm_recoverstate(fiber, est);  \
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
            if(tin_vm_callvalue(vm, fiber, &est, mthval, argc)) \
            { \
                vm_recoverstate(fiber, est); \
                est.frame->result_ignored = true; \
            } \
            else \
            { \
                fiber->stack_top[-1] = callee; \
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
    vmexec_advinvokefromclass(klass, mthname, argc, raiseerr, stat, ignoring, tin_vmexec_peek(fiber, argc))

// calls vm_recoverstate
#define vm_invokemethod(instance, mthname, argc) \
    if(tin_value_isnull(instance)) \
    { \
        fprintf(stderr, "mthname=<%s>\n", mthname);\
        vmexec_raiseerrorfmt("cannot call method '%s' of null-instance", mthname); \
    } \
    TinClass* klass = tin_state_getclassfor(state, instance); \
    if(klass == NULL) \
    { \
        vmexec_raiseerrorfmt("cannot call method '%s' of a non-class", mthname); \
    } \
    tin_vmexec_writeframe(&est, est.ip); \
    vmexec_advinvokefromclass(klass, tin_string_copyconst(state, mthname), argc, true, methods, false, instance); \
    tin_vmexec_readframe(fiber, &est);

#define vm_binaryop(op, opstring) \
    TinValue a = tin_vmexec_peek(fiber, 1); \
    TinValue b = tin_vmexec_peek(fiber, 0); \
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
        tin_vmexec_drop(fiber); \
        if(vm_binaryop_actual(vm, fiber, op, a, b))\
        { \
            continue; \
        } \
    } \
    if(tin_value_isnull(a)) \
    { \
        tin_vmexec_drop(fiber); \
        if(tin_value_isnull(b)) \
        { \
            *(fiber->stack_top - 1) = TRUE_VALUE; \
        } \
        else \
        { \
            vmexec_raiseerrorfmt("cannot use op %s on a null value", opstring); \
            *(fiber->stack_top - 1) = FALSE_VALUE;\
        } \
    } \
    else \
    { \
        vm_invokemethod(a, opstring, 1); \
    }

#define vm_invokeoperation(ignoring) \
    uint8_t argc = tin_vmexec_readbyte(&est); \
    TinString* mthname = tin_vmexec_readstringlong(&est); \
    TinValue receiver = tin_vmexec_peek(fiber, argc); \
    if(tin_value_isnull(receiver)) \
    { \
        vmexec_raiseerrorfmt("cannot index a null value with '%s'", mthname->chars); \
    } \
    tin_vmexec_writeframe(&est, est.ip); \
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
            fiber->stack_top[-argc - 1] = vmmethvalue; \
            vm_callvalue(vmmethvalue, argc); \
            tin_vmexec_readframe(fiber, &est); \
            continue; \
        } \
        vmexec_advinvokefromclass(instance->klass, mthname, argc, true, methods, ignoring, receiver); \
    } \
    else \
    { \
        TinClass* type = tin_state_getclassfor(state, receiver); \
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
TIN_VM_INLINE TinValue tin_vmexec_peek(TinFiber *fiber, short distance);
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
bool tin_vm_callvalue(TinVM *vm, TinFiber* fiber, TinExecState* est, TinValue callee, uint8_t argc);
TinUpvalue *tin_execvm_captureupvalue(TinState *state, TinValue *local);
void tin_vm_closeupvalues(TinVM *vm, const TinValue *last);
TinInterpretResult tin_vm_execmodule(TinState *state, TinModule *module);
TinInterpretResult tin_vm_execfiber(TinState *state, TinFiber *fiber);
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

TIN_VM_INLINE TinValue tin_vmexec_peek(TinFiber* fiber, short distance)
{
    int ofs;
    ofs = ((-1) - distance);
    if(ofs < 0)
    {
        //return NULL_VALUE;
    }
    return fiber->stack_top[ofs];
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

const char* tin_vmexec_funcnamefromvalue(TinVM* vm, TinExecState* est, TinValue v)
{
    TinValue vn;
    (void)est;
    vn = tin_function_getname(vm, v);
    if(!tin_value_isnull(vn))
    {
        return tin_value_ascstring(vn);
    }
    return "unknown";
}

bool tin_vm_callvalue(TinVM* vm, TinFiber* fiber, TinExecState* est, TinValue callee, uint8_t argc)
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
                    return tin_vm_callcallable(vm, tin_value_asfunction(callee), NULL, argc);
                }
                break;
            case TINTYPE_CLOSURE:
                {
                    closure = tin_value_asclosure(callee);
                    return tin_vm_callcallable(vm, closure->function, closure, argc);
                }
                break;
            case TINTYPE_NATIVE_FUNCTION:
                {
                    vm_pushgc(vm->state, false);
                    result = tin_value_asnativefunction(callee)->function(vm, argc, vm->fiber->stack_top - argc);
                    vm->fiber->stack_top -= argc + 1;
                    tin_vm_push(vm, result);
                    vm_popgc(vm->state);
                    return false;
                }
                break;
            case TINTYPE_NATIVE_PRIMITIVE:
                {
                    vm_pushgc(vm->state, false);
                    fiber = vm->fiber;
                    bres = tin_value_asnativeprimitive(callee)->function(vm, argc, fiber->stack_top - argc);
                    if(bres)
                    {
                        fiber->stack_top -= argc;
                    }
                    vm_popgc(vm->state);
                    return bres;
                }
                break;
            case TINTYPE_NATIVE_METHOD:
                {
                    vm_pushgc(vm->state, false);
                    mthobj = tin_value_asnativemethod(callee);
                    fiber = vm->fiber;
                    result = mthobj->method(vm, *(vm->fiber->stack_top - argc - 1), argc, vm->fiber->stack_top - argc);
                    vm->fiber->stack_top -= argc + 1;
                    //if(!tin_value_isnull(result))
                    {
                        if(!vm->fiber->abort)
                        {
                            tin_vm_push(vm, result);
                        }
                    }
                    vm_popgc(vm->state);
                    return false;
                }
                break;
            case TINTYPE_PRIMITIVE_METHOD:
                {
                    vm_pushgc(vm->state, false);
                    fiber = vm->fiber;
                    bres = tin_value_asprimitivemethod(callee)->method(vm, *(fiber->stack_top - argc - 1), argc, fiber->stack_top - argc);
                    if(bres)
                    {
                        fiber->stack_top -= argc;
                    }
                    vm_popgc(vm->state);
                    return bres;
                }
                break;
            case TINTYPE_CLASS:
                {
                    klass = tin_value_asclass(callee);
                    instance = tin_create_instance(vm->state, klass);
                    vm->fiber->stack_top[-argc - 1] = tin_value_fromobject(instance);
                    if(klass->init_method != NULL)
                    {
                        return tin_vm_callvalue(vm, fiber, est, tin_value_fromobject(klass->init_method), argc);
                    }
                    // Remove the arguments, so that they don't mess up the stack
                    // (default constructor has no arguments)
                    for(i = 0; i < argc; i++)
                    {
                        tin_vm_pop(vm);
                    }
                    return false;
                }
                break;
            case TINTYPE_BOUND_METHOD:
                {
                    boundmethod = tin_value_asboundmethod(callee);
                    mthval = boundmethod->method;
                    if(tin_value_isnatmethod(mthval))
                    {
                        vm_pushgc(vm->state, false);
                        result = tin_value_asnativemethod(mthval)->method(vm, boundmethod->receiver, argc, vm->fiber->stack_top - argc);
                        vm->fiber->stack_top -= argc + 1;
                        tin_vm_push(vm, result);
                        vm_popgc(vm->state);
                        return false;
                    }
                    else if(tin_value_isprimmethod(mthval))
                    {
                        fiber = vm->fiber;
                        vm_pushgc(vm->state, false);
                        if(tin_value_asprimitivemethod(mthval)->method(vm, boundmethod->receiver, argc, fiber->stack_top - argc))
                        {
                            fiber->stack_top -= argc;
                            return true;
                        }
                        vm_popgc(vm->state);
                        return false;
                    }
                    else
                    {
                        vm->fiber->stack_top[-argc - 1] = boundmethod->receiver;
                        return tin_vm_callcallable(vm, tin_value_asfunction(mthval), NULL, argc);
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
        fromval = tin_vmexec_peek(fiber, 0);
    }
    fname = "unknown";
    fprintf(stderr, "fromval type=%d %s\n", tin_value_type(fromval), tin_tostring_typename(fromval));
    if(tin_value_isfunction(fromval))
    {
        fname = tin_vmexec_funcnamefromvalue(vm, est, fromval);
    }
    else if(tin_value_isstring(fromval))
    {
        fname = tin_value_ascstring(fromval);
    }
    if(tin_value_isnull(callee))
    {
        tin_vm_raiseerror(vm, "attempt to call '%s' which is null", fname);
    }
    else
    {
        tin_vm_raiseerror(vm, "attempt to call '%s' which is neither function nor class, but is %s", fname, tin_tostring_typename(callee));
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

TinInterpretResult tin_vm_execfiber(TinState* state, TinFiber* fiber)
{
    //vartop
    uint8_t instruction;
    uint8_t nowinstr;
    TinValue peeked;
    TinValList* values;
    TinExecState est;
    TinVM* vm;
    vm = state->vm;
    vm_pushgc(state, true);
    vm->fiber = fiber;
    fiber->abort = false;
    est.frame = &fiber->frames[fiber->frame_count - 1];
    est.currentchunk = &est.frame->function->chunk;
    fiber->module = est.frame->function->module;
    est.ip = est.frame->ip;
    est.slots = est.frame->slots;
    est.privates = fiber->module->privates;
    est.upvalues = est.frame->closure == NULL ? NULL : est.frame->closure->upvalues;

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
    vm_traceframe(fiber);
#endif

    while(true)
    {
#ifdef TIN_TRACE_STACK
        tin_vm_tracestack(vm);
#endif

#ifdef TIN_CHECK_STACK_SIZE
        if((fiber->stack_top - est.frame->slots) > fiber->stack_capacity)
        {
            vmexec_raiseerrorfmt("fiber stack too small (%i > %i)", (int)(fiber->stack_top - est.frame->slots),
                               fiber->stack_capacity);
        }
#endif

        #ifdef TIN_USE_COMPUTEDGOTO
            instruction = *est.ip++;
            nowinstr = instruction;
            #ifdef TIN_TRACE_EXECUTION
                tin_disassemble_instruction(state, est.currentchunk, (size_t)(est.ip - est.currentchunk->code - 1), NULL);
            #endif
            goto* dispatchtable[instruction];
        #else
            instruction = *est.ip++;
            nowinstr = instruction;
            #ifdef TIN_TRACE_EXECUTION
                tin_disassemble_instruction(state, est.currentchunk, (size_t)(est.ip - est.currentchunk->code - 1), NULL);
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
                tin_vmexec_drop(fiber);
                continue;
            }
            op_case(OP_RETURN)
            {
                size_t argc;
                TinValue result;
                TinFiber* parent;
                result = tin_vmexec_pop(fiber);
                tin_vm_closeupvalues(vm, est.slots);
                tin_vmexec_writeframe(&est, est.ip);
                fiber->frame_count--;
                if(est.frame->return_to_c)
                {
                    est.frame->return_to_c = false;
                    fiber->module->return_value = result;
                    fiber->stack_top = est.frame->slots;
                    return (TinInterpretResult){ TINRESULT_OK, result };
                }
                if(fiber->frame_count == 0)
                {
                    fiber->module->return_value = result;
                    if(fiber->parent == NULL)
                    {
                        tin_vmexec_drop(fiber);
                        state->gcallow = wasallowed;
                        return (TinInterpretResult){ TINRESULT_OK, result };
                    }
                    argc = fiber->arg_count;
                    parent = fiber->parent;
                    fiber->parent = NULL;
                    vm->fiber = fiber = parent;
                    tin_vmexec_readframe(fiber, &est);
                    vm_traceframe(fiber);
                    fiber->stack_top -= argc;
                    fiber->stack_top[-1] = result;
                    continue;
                }
                fiber->stack_top = est.frame->slots;
                if(est.frame->result_ignored)
                {
                    fiber->stack_top++;
                    est.frame->result_ignored = false;
                }
                else
                {
                    tin_vmexec_push(fiber, result);
                }
                tin_vmexec_readframe(fiber, &est);
                vm_traceframe(fiber);
                continue;
            }
            op_case(OP_CONSTANT)
            {
                tin_vmexec_push(fiber, tin_vmexec_readconstant(&est));
                continue;
            }
            op_case(OP_CONSTANT_LONG)
            {
                tin_vmexec_push(fiber, tin_vmexec_readconstantlong(&est));
                continue;
            }
            op_case(OP_TRUE)
            {
                tin_vmexec_push(fiber, TRUE_VALUE);
                continue;
            }
            op_case(OP_FALSE)
            {
                tin_vmexec_push(fiber, FALSE_VALUE);
                continue;
            }
            op_case(OP_NULL)
            {
                tin_vmexec_push(fiber, tin_value_makenull(vm->state));
                continue;
            }
            op_case(OP_ARRAY)
            {
                tin_vmexec_push(fiber, tin_value_fromobject(tin_create_array(state)));
                continue;
            }
            op_case(OP_OBJECT)
            {
                // TODO: use object, or map for literal '{...}' constructs?
                // objects would be more general-purpose, but don't implement anything map-like.
                //tin_vmexec_push(fiber, tin_value_fromobject(tin_create_instance(state, state->primobjectclass)));
                tin_vmexec_push(fiber, tin_value_fromobject(tin_create_map(state)));
                continue;
            }
            op_case(OP_RANGE)
            {
                TinValue vala;
                TinValue valb;
                vala = tin_vmexec_pop(fiber);
                valb = tin_vmexec_pop(fiber);
                if(!tin_value_isnumber(vala) || !tin_value_isnumber(valb))
                {
                    vmexec_raiseerror("range fields must be numbers");
                }
                tin_vmexec_push(fiber, tin_value_fromobject(tin_object_makerange(state, tin_value_asnumber(vala), tin_value_asnumber(valb))));
                continue;
            }
            op_case(OP_NEGATE)
            {
                TinValue tmpval;
                TinValue popped;
                if(!tin_value_isnumber(tin_vmexec_peek(fiber, 0)))
                {
                    vmexec_raiseerror("operand must be a number");
                }
                popped = tin_vmexec_pop(fiber);
                if(popped.isfixednumber)
                {
                    tmpval = tin_value_makefixednumber(vm->state, -tin_value_asfixednumber(popped));
                }
                else
                {
                    tmpval = tin_value_makefloatnumber(vm->state, -tin_value_asfloatnumber(popped));
                }
                tin_vmexec_push(fiber, tmpval);
                continue;
            }
            op_case(OP_NOT)
            {
                TinValue tmpval;
                TinValue popped;
                if(tin_value_isinstance(tin_vmexec_peek(fiber, 0)))
                {
                    tin_vmexec_writeframe(&est, est.ip);
                    vmexec_invokefromclass(tin_value_asinstance(tin_vmexec_peek(fiber, 0))->klass, tin_string_copyconst(state, "!"), 0, false, methods, false);
                    continue;
                }
                popped = tin_vmexec_pop(fiber);
                tmpval = tin_value_makebool(vm->state, tin_value_isfalsey(popped));
                tin_vmexec_push(fiber, tmpval);
                continue;
            }
            op_case(OP_BNOT)
            {
                int64_t ival;
                TinValue tmpval;
                TinValue popped;
                if(!tin_value_isnumber(tin_vmexec_peek(fiber, 0)))
                {
                    vmexec_raiseerror("Operand must be a number");
                }
                popped = tin_vmexec_pop(fiber);
                ival = (int)tin_value_asnumber(popped);
                tmpval = tin_value_makefixednumber(vm->state, ~ival);
                tin_vmexec_push(fiber, tmpval);
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
                vala = tin_vmexec_peek(fiber, 1);
                valb = tin_vmexec_peek(fiber, 0);
                if(tin_value_isnumber(vala) && tin_value_isnumber(valb))
                {
                    tin_vmexec_drop(fiber);
                    *(fiber->stack_top - 1) = tin_value_makefloatnumber(vm->state, pow(tin_value_asnumber(vala), tin_value_asnumber(valb)));
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
                vala = tin_vmexec_peek(fiber, 1);
                valb = tin_vmexec_peek(fiber, 0);
                if(tin_value_isnumber(vala) && tin_value_isnumber(valb))
                {
                    tin_vmexec_drop(fiber);
                    *(fiber->stack_top - 1) = tin_value_makefloatnumber(vm->state, floor(tin_value_asnumber(vala) / tin_value_asnumber(valb)));
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
                vala = tin_vmexec_peek(fiber, 1);
                valb = tin_vmexec_peek(fiber, 0);
                if(tin_value_isnumber(vala) && tin_value_isnumber(valb))
                {
                    tin_vmexec_drop(fiber);
                    *(fiber->stack_top - 1) = tin_value_makefloatnumber(vm->state, fmod(tin_value_asnumber(vala), tin_value_asnumber(valb)));
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
                name = tin_vmexec_readstringlong(&est);
                tin_table_set(state, &vm->globals->values, name, tin_vmexec_peek(fiber, 0));
                continue;
            }
            op_case(OP_GET_GLOBAL)
            {
                TinValue setval;
                TinString* name;
                name = tin_vmexec_readstringlong(&est);
                //fprintf(stderr, "GET_GLOBAL: %s\n", name->chars);
                if(!tin_table_get(&vm->globals->values, name, &setval))
                {
                    tin_vmexec_push(fiber, tin_value_makenull(vm->state));
                }
                else
                {
                    tin_vmexec_push(fiber, setval);
                }
                continue;
            }
            op_case(OP_SET_LOCAL)
            {
                uint8_t index;
                index = tin_vmexec_readbyte(&est);
                est.slots[index] = tin_vmexec_peek(fiber, 0);
                continue;
            }
            op_case(OP_GET_LOCAL)
            {
                tin_vmexec_push(fiber, est.slots[tin_vmexec_readbyte(&est)]);
                continue;
            }
            op_case(OP_SET_LOCAL_LONG)
            {
                uint8_t index;
                index = tin_vmexec_readshort(&est);
                est.slots[index] = tin_vmexec_peek(fiber, 0);
                continue;
            }
            op_case(OP_GET_LOCAL_LONG)
            {
                tin_vmexec_push(fiber, est.slots[tin_vmexec_readshort(&est)]);
                continue;
            }
            op_case(OP_SET_PRIVATE)
            {
                uint8_t index;
                index = tin_vmexec_readbyte(&est);
                est.privates[index] = tin_vmexec_peek(fiber, 0);
                continue;
            }
            op_case(OP_GET_PRIVATE)
            {
                tin_vmexec_push(fiber, est.privates[tin_vmexec_readbyte(&est)]);
                continue;
            }
            op_case(OP_SET_PRIVATE_LONG)
            {
                uint8_t index;
                index = tin_vmexec_readshort(&est);
                est.privates[index] = tin_vmexec_peek(fiber, 0);
                continue;
            }
            op_case(OP_GET_PRIVATE_LONG)
            {
                tin_vmexec_push(fiber, est.privates[tin_vmexec_readshort(&est)]);
                continue;
            }
            op_case(OP_SET_UPVALUE)
            {
                uint8_t index;
                index = tin_vmexec_readbyte(&est);
                *est.upvalues[index]->location = tin_vmexec_peek(fiber, 0);
                continue;
            }
            op_case(OP_GET_UPVALUE)
            {
                tin_vmexec_push(fiber, *est.upvalues[tin_vmexec_readbyte(&est)]->location);
                continue;
            }
            op_case(OP_JUMP_IF_FALSE)
            {
                uint16_t offset;
                TinValue popped;
                offset = tin_vmexec_readshort(&est);
                popped = tin_vmexec_pop(fiber);
                if(tin_value_isfalsey(popped))
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(OP_JUMP_IF_NULL)
            {
                uint16_t offset;
                offset = tin_vmexec_readshort(&est);
                if(tin_value_isnull(tin_vmexec_peek(fiber, 0)))
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(OP_JUMP_IF_NULL_POPPING)
            {
                uint16_t offset;
                TinValue popped;
                offset = tin_vmexec_readshort(&est);
                popped = tin_vmexec_pop(fiber);
                if(tin_value_isnull(popped))
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(OP_JUMP)
            {
                uint16_t offset;
                offset = tin_vmexec_readshort(&est);
                est.ip += offset;
                continue;
            }
            op_case(OP_JUMP_BACK)
            {
                uint16_t offset;
                offset = tin_vmexec_readshort(&est);
                est.ip -= offset;
                continue;
            }
            op_case(OP_AND)
            {
                uint16_t offset;
                offset = tin_vmexec_readshort(&est);
                if(tin_value_isfalsey(tin_vmexec_peek(fiber, 0)))
                {
                    est.ip += offset;
                }
                else
                {
                    tin_vmexec_drop(fiber);
                }
                continue;
            }
            op_case(OP_OR)
            {
                uint16_t offset;
                offset = tin_vmexec_readshort(&est);
                if(tin_value_isfalsey(tin_vmexec_peek(fiber, 0)))
                {
                    tin_vmexec_drop(fiber);
                }
                else
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(OP_NULL_OR)
            {
                uint16_t offset;
                offset = tin_vmexec_readshort(&est);
                if(tin_value_isnull(tin_vmexec_peek(fiber, 0)))
                {
                    tin_vmexec_drop(fiber);
                }
                else
                {
                    est.ip += offset;
                }
                continue;
            }
            op_case(OP_CALL)
            {
                size_t argc;
                argc = tin_vmexec_readbyte(&est);
                tin_vmexec_writeframe(&est, est.ip);
                peeked = tin_vmexec_peek(fiber, argc);
                vm_callvalue(peeked, argc);
                continue;
            }
            op_case(OP_CLOSURE)
            {
                size_t i;
                uint8_t index;
                uint8_t islocal;
                TinClosure* closure;
                TinFunction* function;
                function = tin_value_asfunction(tin_vmexec_readconstantlong(&est));
                closure = tin_object_makeclosure(state, function);
                tin_vmexec_push(fiber, tin_value_fromobject(closure));
                for(i = 0; i < closure->upvalue_count; i++)
                {
                    islocal = tin_vmexec_readbyte(&est);
                    index = tin_vmexec_readbyte(&est);
                    if(islocal)
                    {
                        closure->upvalues[i] = tin_execvm_captureupvalue(state, est.frame->slots + index);
                    }
                    else
                    {
                        closure->upvalues[i] = est.upvalues[index];
                    }
                }
                continue;
            }
            op_case(OP_CLOSE_UPVALUE)
            {
                tin_vm_closeupvalues(vm, fiber->stack_top - 1);
                tin_vmexec_drop(fiber);
                continue;
            }
            op_case(OP_CLASS)
            {
                TinString* name;
                TinClass* klassobj;
                name = tin_vmexec_readstringlong(&est);
                klassobj = tin_create_class(state, name);
                tin_vmexec_push(fiber, tin_value_fromobject(klassobj));
                klassobj->super = state->primobjectclass;
                tin_table_add_all(state, &klassobj->super->methods, &klassobj->methods);
                tin_table_add_all(state, &klassobj->super->static_fields, &klassobj->static_fields);
                tin_table_set(state, &vm->globals->values, name, tin_value_fromobject(klassobj));
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
                object = tin_vmexec_peek(fiber, 1);
                name = tin_value_asstring(tin_vmexec_peek(fiber, 0));
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
                                tin_vmexec_drop(fiber);
                                tin_vmexec_writeframe(&est, est.ip);
                                field = tin_value_asfield(getval);
                                tmpval =tin_value_fromobject(field->getter);
                                vm_callvalue(tmpval, 0);
                                tin_vmexec_readframe(fiber, &est);
                                continue;
                            }
                            else
                            {
                                getval = tin_value_fromobject(tin_object_makeboundmethod(state, tin_value_fromobject(instobj), getval));
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
                            getval = tin_value_fromobject(tin_object_makeboundmethod(state, tin_value_fromobject(klassobj), getval));
                        }
                        else if(tin_value_isfield(getval))
                        {
                            field = tin_value_asfield(getval);
                            if(field->getter == NULL)
                            {
                                vmexec_raiseerrorfmt("class %s does not have a getter for field '%s'", klassobj->name->chars,
                                                   name->chars);
                            }
                            tin_vmexec_drop(fiber);
                            tin_vmexec_writeframe(&est, est.ip);
                            tmpval = tin_value_fromobject(field->getter);
                            vm_callvalue(tmpval, 0);
                            tin_vmexec_readframe(fiber, &est);
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
                    klassobj = tin_state_getclassfor(state, object);
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
                            tin_vmexec_drop(fiber);
                            tin_vmexec_writeframe(&est, est.ip);
                            tmpval = tin_value_fromobject(tin_value_asfield(getval)->getter);
                            vm_callvalue(tmpval, 0);
                            tin_vmexec_readframe(fiber, &est);
                            continue;
                        }
                        else if(tin_value_isnatmethod(getval) || tin_value_isprimmethod(getval))
                        {
                            getval = tin_value_fromobject(tin_object_makeboundmethod(state, object, getval));
                        }
                    }
                    else
                    {
                        getval = tin_value_makenull(vm->state);
                    }
                }
                tin_vmexec_drop(fiber);// Pop field name
                fiber->stack_top[-1] = getval;
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
                instval = tin_vmexec_peek(fiber, 2);
                value = tin_vmexec_peek(fiber, 1);
                fieldname = tin_value_asstring(tin_vmexec_peek(fiber, 0));
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

                        tin_vmexec_dropn(fiber, 2);
                        tin_vmexec_push(fiber, value);
                        tin_vmexec_writeframe(&est, est.ip);
                        tmpval = tin_value_fromobject(field->setter);
                        vm_callvalue(tmpval, 1);
                        tin_vmexec_readframe(fiber, &est);
                        continue;
                    }
                    if(tin_value_isnull(value))
                    {
                        tin_table_delete(&klassobj->static_fields, fieldname);
                    }
                    else
                    {
                        tin_table_set(state, &klassobj->static_fields, fieldname, value);
                    }
                    tin_vmexec_dropn(fiber, 2);// Pop field name and the value
                    fiber->stack_top[-1] = value;
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
                        tin_vmexec_dropn(fiber, 2);
                        tin_vmexec_push(fiber, value);
                        tin_vmexec_writeframe(&est, est.ip);
                        tmpval = tin_value_fromobject(field->setter);
                        vm_callvalue(tmpval, 1);
                        tin_vmexec_readframe(fiber, &est);
                        continue;
                    }
                    if(tin_value_isnull(value))
                    {
                        tin_table_delete(&instobj->fields, fieldname);
                    }
                    else
                    {
                        tin_table_set(state, &instobj->fields, fieldname, value);
                    }
                    tin_vmexec_dropn(fiber, 2);// Pop field name and the value
                    fiber->stack_top[-1] = value;
                }
                else
                {
                    klassobj = tin_state_getclassfor(state, instval);
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
                        tin_vmexec_dropn(fiber, 2);
                        tin_vmexec_push(fiber, value);
                        tin_vmexec_writeframe(&est, est.ip);
                        tmpval = tin_value_fromobject(field->setter);
                        vm_callvalue(tmpval, 1);
                        tin_vmexec_readframe(fiber, &est);
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
                vm_invokemethod(tin_vmexec_peek(fiber, 1), "[]", 1);
                continue;
            }
            op_case(OP_SUBSCRIPT_SET)
            {
                vm_invokemethod(tin_vmexec_peek(fiber, 2), "[]", 2);
                continue;
            }
            op_case(OP_PUSH_ARRAY_ELEMENT)
            {
                size_t arindex;
                values = &tin_value_asarray(tin_vmexec_peek(fiber, 1))->list;
                arindex = tin_vallist_count(values);
                tin_vallist_ensuresize(state, values, arindex + 1);
                tin_vallist_set(values, arindex, tin_vmexec_peek(fiber, 0));
                tin_vmexec_drop(fiber);
                continue;
            }
            op_case(OP_PUSH_OBJECT_FIELD)
            {
                TinValue operand;
                operand = tin_vmexec_peek(fiber, 2);
                if(tin_value_ismap(operand))
                {
                    tin_table_set(state, &tin_value_asmap(operand)->values, tin_value_asstring(tin_vmexec_peek(fiber, 1)), tin_vmexec_peek(fiber, 0));
                }
                else if(tin_value_isinstance(operand))
                {
                    tin_table_set(state, &tin_value_asinstance(operand)->fields, tin_value_asstring(tin_vmexec_peek(fiber, 1)), tin_vmexec_peek(fiber, 0));
                }
                else
                {
                    vmexec_raiseerrorfmt("cannot set field '%s' on type '%s'", tin_tostring_typename(operand));
                }
                tin_vmexec_dropn(fiber, 2);
                continue;
            }
            op_case(OP_STATIC_FIELD)
            {
                tin_table_set(state, &tin_value_asclass(tin_vmexec_peek(fiber, 1))->static_fields, tin_vmexec_readstringlong(&est), tin_vmexec_peek(fiber, 0));
                tin_vmexec_drop(fiber);
                continue;
            }
            op_case(OP_METHOD)
            {
                TinString* name;
                TinClass* klassobj;
                klassobj = tin_value_asclass(tin_vmexec_peek(fiber, 1));
                name = tin_vmexec_readstringlong(&est);
                if((klassobj->init_method == NULL || (klassobj->super != NULL && klassobj->init_method == ((TinClass*)klassobj->super)->init_method))
                   && tin_string_getlength(name) == 11 && memcmp(name->chars, "constructor", 11) == 0)
                {
                    klassobj->init_method = tin_value_asobject(tin_vmexec_peek(fiber, 0));
                }
                tin_table_set(state, &klassobj->methods, name, tin_vmexec_peek(fiber, 0));
                tin_vmexec_drop(fiber);
                continue;
            }
            op_case(OP_DEFINE_FIELD)
            {
                tin_table_set(state, &tin_value_asclass(tin_vmexec_peek(fiber, 1))->methods, tin_vmexec_readstringlong(&est), tin_vmexec_peek(fiber, 0));
                tin_vmexec_drop(fiber);
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
                argc = tin_vmexec_readbyte(&est);
                mthname = tin_vmexec_readstringlong(&est);
                popped = tin_vmexec_pop(fiber);
                klassobj = tin_value_asclass(popped);
                tin_vmexec_writeframe(&est, est.ip);
                vmexec_invokefromclass(klassobj, mthname, argc, true, methods, false);
                continue;
            }
            op_case(OP_INVOKE_SUPER_IGNORING)
            {
                size_t argc;
                TinValue popped;
                TinClass* klassobj;
                TinString* mthname;
                argc = tin_vmexec_readbyte(&est);
                mthname = tin_vmexec_readstringlong(&est);
                popped = tin_vmexec_pop(fiber);
                klassobj = tin_value_asclass(popped);
                tin_vmexec_writeframe(&est, est.ip);
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
                mthname = tin_vmexec_readstringlong(&est);
                popped = tin_vmexec_pop(fiber);
                klassobj = tin_value_asclass(popped);
                instval = tin_vmexec_pop(fiber);
                if(tin_table_get(&klassobj->methods, mthname, &value))
                {
                    value = tin_value_fromobject(tin_object_makeboundmethod(state, instval, value));
                }
                else
                {
                    value = tin_value_makenull(vm->state);
                }
                tin_vmexec_push(fiber, value);
                continue;
            }
            op_case(OP_INHERIT)
            {
                TinValue super;
                TinClass* superklass;
                TinClass* klassobj;
                super = tin_vmexec_peek(fiber, 1);
                if(!tin_value_isclass(super))
                {
                    vmexec_raiseerror("superclass must be a class");
                }
                klassobj = tin_value_asclass(tin_vmexec_peek(fiber, 0));
                superklass = tin_value_asclass(super);
                klassobj->super = superklass;
                klassobj->init_method = superklass->init_method;
                tin_table_add_all(state, &superklass->methods, &klassobj->methods);
                tin_table_add_all(state, &klassobj->super->static_fields, &klassobj->static_fields);
                continue;
            }
            op_case(OP_IS)
            {
                bool found;
                TinValue klassval;
                TinValue instval;
                TinClass* type;
                TinClass* instanceklass;
                instval = tin_vmexec_peek(fiber, 1);
                if(tin_value_isnull(instval))
                {
                    tin_vmexec_dropn(fiber, 2);
                    tin_vmexec_push(fiber, FALSE_VALUE);

                    continue;
                }
                instanceklass = tin_state_getclassfor(state, instval);
                klassval = tin_vmexec_peek(fiber, 0);
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
                tin_vmexec_dropn(fiber, 2);// Drop the instance and class
                tin_vmexec_push(fiber, tin_value_makebool(vm->state, found));
                continue;
            }
            op_case(OP_POP_LOCALS)
            {
                tin_vmexec_dropn(fiber, tin_vmexec_readshort(&est));
                continue;
            }
            op_case(OP_VARARG)
            {
                size_t i;
                TinValue slot;
                slot = est.slots[tin_vmexec_readbyte(&est)];
                if(!tin_value_isarray(slot))
                {
                    continue;
                }
                values = &tin_value_asarray(slot)->list;
                tin_ensure_fiber_stack(state, fiber, tin_vallist_count(values) + est.frame->function->maxslots + (int)(fiber->stack_top - fiber->stack));
                for(i = 0; i < tin_vallist_count(values); i++)
                {
                    tin_vmexec_push(fiber, tin_vallist_get(values, i));
                }
                // Hot-bytecode patching, increment the amount of arguments to OP_CALL
                est.ip[1] = est.ip[1] + tin_vallist_count(values) - 1;
                continue;
            }

            op_case(OP_REFERENCE_GLOBAL)
            {
                TinString* name;
                TinValue* pval;
                name = tin_vmexec_readstringlong(&est);
                if(tin_table_get_slot(&vm->globals->values, name, &pval))
                {
                    tin_vmexec_push(fiber, tin_value_fromobject(tin_object_makereference(state, pval)));
                }
                else
                {
                    vmexec_raiseerror("attempt to reference a null value");
                }
                continue;
            }
            op_case(OP_REFERENCE_PRIVATE)
            {
                tin_vmexec_push(fiber, tin_value_fromobject(tin_object_makereference(state, &est.privates[tin_vmexec_readshort(&est)])));
                continue;
            }
            op_case(OP_REFERENCE_LOCAL)
            {
                tin_vmexec_push(fiber, tin_value_fromobject(tin_object_makereference(state, &est.slots[tin_vmexec_readshort(&est)])));
                continue;
            }
            op_case(OP_REFERENCE_UPVALUE)
            {
                tin_vmexec_push(fiber, tin_value_fromobject(tin_object_makereference(state, est.upvalues[tin_vmexec_readbyte(&est)]->location)));
                continue;
            }
            op_case(OP_REFERENCE_FIELD)
            {
                TinValue object;
                TinString* name;
                TinValue* pval;
                object = tin_vmexec_peek(fiber, 1);
                if(tin_value_isnull(object))
                {
                    vmexec_raiseerror("attempt to index a null value");
                }
                name = tin_value_asstring(tin_vmexec_peek(fiber, 0));
                if(tin_value_isinstance(object))
                {
                    if(!tin_table_get_slot(&tin_value_asinstance(object)->fields, name, &pval))
                    {
                        vmexec_raiseerror("attempt to reference a null value");
                    }
                }
                else
                {
                    tin_towriter_value(state, &state->debugwriter, object, true);
                    printf("\n");
                    vmexec_raiseerrorfmt("cannot reference field '%s' of a non-instance", name->chars);
                }
                tin_vmexec_drop(fiber);// Pop field name
                fiber->stack_top[-1] = tin_value_fromobject(tin_object_makereference(state, pval));
                continue;
            }
            op_case(OP_SET_REFERENCE)
            {
                TinValue reference;
                reference = tin_vmexec_pop(fiber);
                if(!tin_value_isreference(reference))
                {
                    vmexec_raiseerror("cannot set reference value of a non-reference");
                }
                *tin_value_asreference(reference)->slot = tin_vmexec_peek(fiber, 0);
                continue;
            }
            vm_default()
            {
                vmexec_raiseerrorfmt("unknown VM op code '%d'", *est.ip);
                break;
            }
        }
    }

    vm_returnerror();
}

void tin_vmutil_callexitjump()
{
    longjmp(jumpbuffer, 1);
}

bool tin_vmutil_setexitjump()
{
    return setjmp(jumpbuffer);
}


