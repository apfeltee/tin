
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "priv.h"


#define TIN_GCSTATE_GROWCAPACITY(cap) \
    (((cap) < 8) ? (8) : ((cap) * 2))


static bool measurecompilationtime;
static double lastsourcetime = 0;

TinString* tin_vformat_error(TinState* state, size_t line, const char* fmt, va_list args)
{
    size_t buffersize;
    char* buffer;
    TinString* rt;
    va_list argscopy;
    va_copy(argscopy, args);
    buffersize = vsnprintf(NULL, 0, fmt, argscopy) + 1;
    va_end(argscopy);
    buffer = (char*)tin_gcmem_allocate(state, sizeof(char), buffersize+1);
    vsnprintf(buffer, buffersize, fmt, args);
    buffer[buffersize - 1] = '\0';
    if(line != 0)
    {
        rt = tin_value_asstring(tin_string_format(state, "[line #]: $", (double)line, (const char*)buffer));
    }
    else
    {
        rt = tin_value_asstring(tin_string_format(state, "$", (const char*)buffer));
    }
    free(buffer);
    return rt;
}

TinString* tin_format_error(TinState* state, size_t line, const char* fmt, ...)
{
    va_list args;
    TinString* result;
    va_start(args, fmt);
    result = tin_vformat_error(state, line, fmt, args);
    va_end(args);
    return result;
}

void tin_enable_compilation_time_measurement()
{
    measurecompilationtime = true;
}

static void tin_util_default_error(TinState* state, const char* message)
{
    (void)state;
    fflush(stdout);
    fprintf(stderr, "%s%s%s\n", COLOR_RED, message, COLOR_RESET);
    fflush(stderr);
}

static void tin_util_default_printf(TinState* state, const char* message)
{
    (void)state;
    printf("%s", message);
}

TinState* tin_make_state()
{
    TinState* state;
    state = (TinState*)malloc(sizeof(TinState));
    {
        state->config.dumpbytecode = false;
        state->config.dumpast = false;
        state->config.runafterdump = true;
    }
    {
        state->primclassclass = NULL;
        state->primobjectclass = NULL;
        state->primnumberclass = NULL;
        state->primstringclass = NULL;
        state->primboolclass = NULL;
        state->primfunctionclass = NULL;
        state->primfiberclass = NULL;
        state->primmoduleclass = NULL;
        state->primarrayclass = NULL;
        state->primmapclass = NULL;
        state->primrangeclass = NULL;
    }
    state->gcbytescount = 0;
    state->gcnext = 256 * 1024;
    state->gcallow = false;
    /* io stuff */
    {
        state->errorfn = tin_util_default_error;
        state->printfn = tin_util_default_printf;
        tin_writer_init_file(state, &state->stdoutwriter, stdout, true);
    }
    tin_vallist_init(state, &state->gclightobjects);
    state->haderror = false;
    state->gcroots = NULL;
    state->gcrootcount = 0;
    state->gcrootcapacity = 0;
    state->lastmodule = NULL;
    tin_writer_init_file(state, &state->debugwriter, stdout, true);
    state->scanner = (TinAstScanner*)malloc(sizeof(TinAstScanner));
    state->parser = (TinAstParser*)malloc(sizeof(TinAstParser));
    tin_astparser_init(state, (TinAstParser*)state->parser);
    state->emitter = (TinAstEmitter*)malloc(sizeof(TinAstEmitter));
    tin_astemit_init(state, state->emitter);
    state->optimizer = (TinAstOptimizer*)malloc(sizeof(TinAstOptimizer));
    tin_astopt_init(state, state->optimizer);
    state->vm = (TinVM*)malloc(sizeof(TinVM));
    tin_vm_init(state, state->vm);
    tin_api_init(state);
    tin_open_core_library(state);
    return state;
}

int64_t tin_destroy_state(TinState* state)
{
    int64_t amount;
    if(state->gcroots != NULL)
    {
        free(state->gcroots);
        state->gcroots = NULL;
    }
    tin_api_destroy(state);
    free(state->scanner);
    tin_astparser_destroy(state->parser);
    free(state->parser);
    tin_astemit_destroy(state->emitter);
    free(state->emitter);
    free(state->optimizer);
    tin_vm_destroy(state->vm);
    free(state->vm);
    amount = state->gcbytescount;
    free(state);
    return amount;
}

void tin_api_init(TinState* state)
{
    const char* apiname;
    apiname = "__native__";
    state->capiname = tin_string_copy(state, apiname, strlen(apiname));
    state->capifunction = NULL;
    state->capifiber = NULL;
}

void tin_api_destroy(TinState* state)
{
    state->capiname = NULL;
    state->capifunction = NULL;
    state->capifiber = NULL;
}

TinValue tin_state_getglobalvalue(TinState* state, TinString* name)
{
    TinValue global;
    if(!tin_table_get(&state->vm->globals->values, name, &global))
    {
        return tin_value_makenull(state);
    }
    return global;
}

TinFunction* tin_state_getglobalfunction(TinState* state, TinString* name)
{
    TinValue function = tin_state_getglobalvalue(state, name);
    if(tin_value_isfunction(function))
    {
        return tin_value_asfunction(function);
    }
    return NULL;
}

void tin_state_setglobal(TinState* state, TinString* name, TinValue value)
{
    /*
    tin_state_pushroot(state, (TinObject*)name);
    tin_state_pushvalueroot(state, value);
    tin_table_set(state, &state->vm->globals->values, name, value);
    tin_state_poproots(state, 2);
    */
    tin_table_set(state, &state->vm->globals->values, name, value);

}

bool tin_state_hasglobal(TinState* state, TinString* name)
{
    TinValue global;
    return tin_table_get(&state->vm->globals->values, name, &global);
}

void tin_state_defnativefunc(TinState* state, const char* name, TinNativeFunctionFn native)
{
    TinObject* tobj;
    TinString* ts;
    ts = tin_string_copyconst(state, name);
    tobj =(TinObject*)tin_object_makenativefunction(state, native, ts);
    tin_state_setglobal(state, ts, tin_value_fromobject(tobj));
}

void tin_state_defnativeprimitive(TinState* state, const char* name, TinNativePrimitiveFn native)
{
    tin_state_pushroot(state, (TinObject*)tin_string_copyconst(state, name));
    tin_state_pushroot(state, (TinObject*)tin_object_makenativeprimitive(state, native, tin_value_asstring(tin_state_peekroot(state, 0))));
    tin_table_set(state, &state->vm->globals->values, tin_value_asstring(tin_state_peekroot(state, 1)), tin_state_peekroot(state, 0));
    tin_state_poproots(state, 2);
}

TinValue tin_state_getinstancemethod(TinState* state, TinValue callee, TinString* mthname)
{
    TinValue mthval;
    TinClass* klass;
    klass = tin_state_getclassfor(state, callee);
    if((tin_value_isinstance(callee) && tin_table_get(&tin_value_asinstance(callee)->fields, mthname, &mthval)) || tin_table_get(&klass->methods, mthname, &mthval))
    {
        return mthval;
    }
    return tin_value_makenull(state);
}

TinInterpretResult tin_state_callinstancemethod(TinState* state, TinValue callee, TinString* mthname, TinValue* argv, size_t argc)
{
    TinValue mthval;
    mthval = tin_state_getinstancemethod(state, callee, mthname);
    if(!tin_value_isnull(mthval))
    {
        return tin_state_callvalue(state, mthval, argv, argc, false);
    }
    return INTERPRET_RUNTIME_FAIL(state);    
}


TinValue tin_state_getfield(TinState* state, TinTable* table, const char* name)
{
    TinValue value;

    if(!tin_table_get(table, tin_string_copyconst(state, name), &value))
    {
        value = tin_value_makenull(state);
    }

    return value;
}

TinValue tin_state_getmapfield(TinState* state, TinMap* map, const char* name)
{
    TinValue value;

    if(!tin_table_get(&map->values, tin_string_copyconst(state, name), &value))
    {
        value = tin_value_makenull(state);
    }

    return value;
}

void tin_state_setfield(TinState* state, TinTable* table, const char* name, TinValue value)
{
    tin_table_set(state, table, tin_string_copyconst(state, name), value);
}

void tin_state_setmapfield(TinState* state, TinMap* map, const char* name, TinValue value)
{
    tin_table_set(state, &map->values, tin_string_copyconst(state, name), value);
}

bool tin_state_ensurefiber(TinVM* vm, TinFiber* fiber)
{
    size_t newcapacity;
    size_t osize;
    size_t newsize;
    if(fiber == NULL)
    {
        tin_vm_raiseerror(vm, "no fiber to run on");
        return true;
    }
    if(fiber->framecount == TIN_CALL_FRAMES_MAX)
    {
        tin_vm_raiseerror(vm, "fiber frame overflow");
        return true;
    }
    if(fiber->framecount + 1 > fiber->framecap)
    {
        //newcapacity = fmin(TIN_CALL_FRAMES_MAX, fiber->framecap * 2);
        newcapacity = (fiber->framecap * 2) + 1;
        osize = (sizeof(TinCallFrame) * fiber->framecap);
        newsize = (sizeof(TinCallFrame) * newcapacity);
        fiber->framevalues = (TinCallFrame*)tin_gcmem_memrealloc(vm->state, fiber->framevalues, osize, newsize);
        fiber->framecap = newcapacity;
    }

    return false;
}

static inline TinCallFrame* setup_call(TinState* state, TinFunction* callee, TinValue* argv, uint8_t argc, bool ignfiber)
{
    bool vararg;
    int amount;
    size_t i;
    size_t varargc;
    size_t functionargcount;
    TinVM* vm;
    TinFiber* fiber;
    TinCallFrame* frame;
    TinArray* array;
    (void)argc;
    (void)varargc;
    vm = state->vm;
    fiber = vm->fiber;
    if(callee == NULL)
    {
        tin_vm_raiseerror(vm, "attempt to call a null value");
        return NULL;
    }
    if(ignfiber)
    {
        if(fiber == NULL)
        {
            fiber = state->capifiber;
        }
    }
    if(!ignfiber)
    {
        if(tin_state_ensurefiber(vm, fiber))
        {
            return NULL;
        }        
    }
    tin_fiber_ensurestack(state, fiber, callee->maxslots + (int)(fiber->stacktop - fiber->stackvalues));
    frame = &fiber->framevalues[fiber->framecount++];
    frame->slots = fiber->stacktop;
    tin_vm_push(state->vm, tin_value_fromobject(callee));
    for(i = 0; i < argc; i++)
    {
        tin_vm_push(state->vm, argv[i]);
    }
    functionargcount = callee->argcount;
    if(argc != functionargcount)
    {
        vararg = callee->vararg;
        if(argc < functionargcount)
        {
            amount = (int)functionargcount - argc - (vararg ? 1 : 0);
            for(i = 0; i < (size_t)amount; i++)
            {
                tin_vm_push(state->vm, tin_value_makenull(state));
            }
            if(vararg)
            {
                tin_vm_push(state->vm, tin_value_fromobject(tin_object_makearray(vm->state)));
            }
        }
        else if(callee->vararg)
        {
            array = tin_object_makearray(vm->state);
            varargc = argc - functionargcount + 1;
            tin_vallist_ensuresize(vm->state, &array->list, varargc);
            for(i = 0; i < varargc; i++)
            {
                tin_vallist_set(vm->state, &array->list, i, fiber->stacktop[(int)i - (int)varargc]);
            }

            fiber->stacktop -= varargc;
            tin_vm_push(vm, tin_value_fromobject(array));
        }
        else
        {
            fiber->stacktop -= (argc - functionargcount);
        }
    }
    else if(callee->vararg)
    {
        array = tin_object_makearray(vm->state);
        varargc = argc - functionargcount + 1;
        tin_vallist_push(vm->state, &array->list, *(fiber->stacktop - 1));
        *(fiber->stacktop - 1) = tin_value_fromobject(array);
    }
    frame->ip = callee->chunk.code;
    frame->closure = NULL;
    frame->function = callee;
    frame->ignresult = false;
    frame->returntonative = true;
    return frame;
}

static inline TinInterpretResult execute_call(TinState* state, TinCallFrame* frame)
{
    TinFiber* fiber;
    TinInterpretResult result;
    if(frame == NULL)
    {
        RETURN_RUNTIME_ERROR(state);
    }
    fiber = state->vm->fiber;
    result = tin_vm_execfiber(state, fiber);
    if(!tin_value_isnull(fiber->errorval))
    {
        result.result = fiber->errorval;
    }
    return result;
}

TinInterpretResult tin_state_callfunction(TinState* state, TinFunction* callee, TinValue* argv, uint8_t argc, bool ignfiber)
{
    return execute_call(state, setup_call(state, callee, argv, argc, ignfiber));
}

TinInterpretResult tin_state_callclosure(TinState* state, TinClosure* callee, TinValue* argv, uint8_t argc, bool ignfiber)
{
    TinCallFrame* frame;
    frame = setup_call(state, callee->function, argv, argc, ignfiber);
    if(frame == NULL)
    {
        RETURN_RUNTIME_ERROR(state);
    }
    frame->closure = callee;
    return execute_call(state, frame);
}

TinInterpretResult tin_state_callmethod(TinState* state, TinValue instance, TinValue callee, TinValue* argv, uint8_t argc, bool ignfiber)
{
    uint8_t i;
    TinVM* vm;
    TinInterpretResult lir;
    TinObjType type;
    TinClass* klass;
    TinFiber* fiber;
    TinValue* slot;
    TinNativeMethod* natmethod;
    TinBoundMethod* boundmethod;
    TinValue mthval;
    TinValue result;
    lir.result = tin_value_makenull(state);
    lir.type = TINSTATE_OK;
    vm = state->vm;
    if(tin_value_isobject(callee))
    {
        if(tin_vm_setexitjump(state->vm))
        {
            RETURN_RUNTIME_ERROR(state);
        }
        type = tin_value_type(callee);

        if(type == TINTYPE_FUNCTION)
        {
            return tin_state_callfunction(state, tin_value_asfunction(callee), argv, argc, ignfiber);
        }
        else if(type == TINTYPE_CLOSURE)
        {
            return tin_state_callclosure(state, tin_value_asclosure(callee), argv, argc, ignfiber);
        }
        fiber = vm->fiber;
        if(ignfiber)
        {
            if(fiber == NULL)
            {
                fiber = state->capifiber;
            }
        }
        if(!ignfiber)
        {
            if(tin_state_ensurefiber(vm, fiber))
            {
                RETURN_RUNTIME_ERROR(state);
            }
        }
        tin_fiber_ensurestack(state, fiber, 3 + argc + (int)(fiber->stacktop - fiber->stackvalues));
        slot = fiber->stacktop;
        tin_vm_push(state->vm, instance);
        if(type != TINTYPE_CLASS)
        {
            for(i = 0; i < argc; i++)
            {
                tin_vm_push(state->vm, argv[i]);
            }
        }
        switch(type)
        {
            case TINTYPE_NATIVEFUNCTION:
                {
                    result = tin_value_asnativefunction(callee)->function(vm, argc, fiber->stacktop - argc);
                    fiber->stacktop = slot;
                    RETURN_OK(result);
                }
                break;
            case TINTYPE_NATIVEPRIMITIVE:
                {
                    tin_value_asnativeprimitive(callee)->function(vm, argc, fiber->stacktop - argc);
                    fiber->stacktop = slot;
                    RETURN_OK(tin_value_makenull(state));
                }
                break;
            case TINTYPE_NATIVEMETHOD:
                {
                    natmethod = tin_value_asnativemethod(callee);
                    result = natmethod->method(vm, *(fiber->stacktop - argc - 1), argc, fiber->stacktop - argc);
                    fiber->stacktop = slot;
                    RETURN_OK(result);
                }
                break;
            case TINTYPE_CLASS:
                {
                    klass = tin_value_asclass(callee);
                    *slot = tin_value_fromobject(tin_object_makeinstance(vm->state, klass));
                    if(klass->initmethod != NULL)
                    {
                        lir = tin_state_callmethod(state, *slot, tin_value_fromobject(klass->initmethod), argv, argc, ignfiber);
                    }
                    // TODO: when should this return *slot instead of lir?
                    fiber->stacktop = slot;
                    //RETURN_OK(*slot);
                    return lir;
                }
                break;
            case TINTYPE_BOUNDMETHOD:
                {
                    boundmethod = tin_value_asboundmethod(callee);
                    mthval = boundmethod->method;
                    *slot = boundmethod->receiver;
                    if(tin_value_isnatmethod(mthval))
                    {
                        result = tin_value_asnativemethod(mthval)->method(vm, boundmethod->receiver, argc, fiber->stacktop - argc);
                        fiber->stacktop = slot;
                        RETURN_OK(result);
                    }
                    else if(tin_value_isprimmethod(mthval))
                    {
                        tin_value_asprimitivemethod(mthval)->method(vm, boundmethod->receiver, argc, fiber->stacktop - argc);

                        fiber->stacktop = slot;
                        RETURN_OK(tin_value_makenull(state));
                    }
                    else
                    {
                        fiber->stacktop = slot;
                        return tin_state_callfunction(state, tin_value_asfunction(mthval), argv, argc, ignfiber);
                    }
                }
                break;
            case TINTYPE_PRIMITIVEMETHOD:
                {
                    tin_value_asprimitivemethod(callee)->method(vm, *(fiber->stacktop - argc - 1), argc, fiber->stacktop - argc);
                    fiber->stacktop = slot;
                    RETURN_OK(tin_value_makenull(state));
                }
                break;
            default:
                {
                }
                break;
        }
    }
    if(tin_value_isnull(callee))
    {
        tin_vm_raiseerror(vm, "attempt to call a null value");
    }
    else
    {
        tin_vm_raiseerror(vm, "can only call functions and classes");
    }

    RETURN_RUNTIME_ERROR(state);
}

TinInterpretResult tin_state_callvalue(TinState* state, TinValue callee, TinValue* argv, uint8_t argc, bool ignfiber)
{
    return tin_state_callmethod(state, callee, callee, argv, argc, ignfiber);
}

TinInterpretResult tin_state_findandcallmethod(TinState* state, TinValue callee, TinString* mthname, TinValue* argv, uint8_t argc, bool ignfiber)
{
    TinClass* klass;
    TinVM* vm;
    TinFiber* fiber;
    TinValue mthval;
    vm = state->vm;
    fiber = vm->fiber;
    if(fiber == NULL)
    {
        if(!ignfiber)
        {
            tin_vm_raiseerror(vm, "no fiber to run on");
            RETURN_RUNTIME_ERROR(state);
        }
    }
    klass = tin_state_getclassfor(state, callee);
    if((tin_value_isinstance(callee) && tin_table_get(&tin_value_asinstance(callee)->fields, mthname, &mthval)) || tin_table_get(&klass->methods, mthname, &mthval))
    {
        return tin_state_callmethod(state, callee, mthval, argv, argc, ignfiber);
    }
    return TIN_MAKESTATUS(TINSTATE_INVALID, tin_value_makenull(state));
}

void tin_state_pushroot(TinState* state, TinObject* object)
{
    tin_state_pushvalueroot(state, tin_value_fromobject(object));
}

void tin_state_pushvalueroot(TinState* state, TinValue value)
{
    if(state->gcrootcount + 1 >= state->gcrootcapacity)
    {
        state->gcrootcapacity = TIN_GCSTATE_GROWCAPACITY(state->gcrootcapacity);
        state->gcroots = (TinValue*)realloc(state->gcroots, state->gcrootcapacity * sizeof(TinValue));
    }
    state->gcroots[state->gcrootcount++] = value;
}

TinValue tin_state_peekroot(TinState* state, uint8_t distance)
{
    assert(state->gcrootcount - distance + 1 > 0);
    return state->gcroots[state->gcrootcount - distance - 1];
}

void tin_state_poproot(TinState* state)
{
    state->gcrootcount--;
}

void tin_state_poproots(TinState* state, uint8_t amount)
{
    state->gcrootcount -= amount;
}

TinClass* tin_state_getclassfor(TinState* state, TinValue value)
{
    TinValue* slot;
    TinUpvalue* upvalue;
    if(tin_value_isobject(value))
    {
        switch(tin_value_type(value))
        {
            case TINTYPE_NUMBER:
                {
                    return state->primnumberclass;
                }
                break;
            case TINTYPE_STRING:
                {
                    return state->primstringclass;
                }
                break;
            case TINTYPE_USERDATA:
                {
                    return state->primobjectclass;
                }
                break;
            case TINTYPE_FIELD:
            case TINTYPE_FUNCTION:
            case TINTYPE_CLOSURE:
            case TINTYPE_NATIVEFUNCTION:
            case TINTYPE_NATIVEPRIMITIVE:
            case TINTYPE_BOUNDMETHOD:
            case TINTYPE_PRIMITIVEMETHOD:
            case TINTYPE_NATIVEMETHOD:
                {
                    return state->primfunctionclass;
                }
                break;
            case TINTYPE_FIBER:
                {
                    //fprintf(stderr, "should return fiber class ....\n");
                    return state->primfiberclass;
                }
                break;
            case TINTYPE_MODULE:
                {
                    return state->primmoduleclass;
                }
                break;
            case TINTYPE_UPVALUE:
                {
                    upvalue = tin_value_asupvalue(value);
                    if(upvalue->location == NULL)
                    {
                        return tin_state_getclassfor(state, upvalue->closed);
                    }
                    return tin_state_getclassfor(state, *upvalue->location);
                }
                break;
            case TINTYPE_INSTANCE:
                {
                    return tin_value_asinstance(value)->klass;
                }
                break;
            case TINTYPE_CLASS:
                {
                    return state->primclassclass;
                }
                break;
            case TINTYPE_ARRAY:
                {
                    return state->primarrayclass;
                }
                break;
            case TINTYPE_MAP:
                {
                    return state->primmapclass;
                }
                break;
            case TINTYPE_RANGE:
                {
                    return state->primrangeclass;
                }
                break;
            case TINTYPE_REFERENCE:
                {
                    slot = tin_value_asreference(value)->slot;
                    if(slot != NULL)
                    {
                        return tin_state_getclassfor(state, *slot);
                    }
                    return state->primobjectclass;
                }
                break;
            default:
                {
                }
                break;
        }
    }
    else if(tin_value_isnumber(value))
    {
        return state->primnumberclass;
    }
    else if(tin_value_isbool(value))
    {
        return state->primboolclass;
    }
    //fprintf(stderr, "failed to find class object!\n");
    return NULL;
}

static void free_statements(TinState* state, TinAstExprList* statements)
{
    size_t i;
    for(i = 0; i < statements->count; i++)
    {
        tin_ast_destroyexpression(state, statements->values[i]);
    }
    tin_exprlist_destroy(state, statements);
}


TinModule* tin_state_compilemodule(TinState* state, TinString* module_name, const char* code, size_t len)
{
    clock_t t;
    clock_t total_t;
    bool allowedgc;
    TinModule* module;
    TinAstExprList statements;
    allowedgc = state->gcallow;
    state->gcallow = false;
    state->haderror = false;
    module = NULL;
    // This is a lbc format
    if((code[1] << 8 | code[0]) == TIN_BYTECODE_MAGIC_NUMBER)
    {
        module = tin_ioutil_readmodule(state, code, len);
    }
    else
    {
        t = 0;
        total_t = 0;
        if(measurecompilationtime)
        {
            total_t = t = clock();
        }
        tin_exprlist_init(&statements);
        if(tin_astparser_parsesource(state->parser, module_name->data, code, len, &statements))
        {
            free_statements(state, &statements);
            return NULL;
        }
        if(state->config.dumpast)
        {
            tin_towriter_ast(state, &state->stdoutwriter, &statements);
        }
        if(measurecompilationtime)
        {
            printf("Parsing:        %gms\n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
            t = clock();
        }
        tin_astopt_optast(state->optimizer, &statements);
        if(measurecompilationtime)
        {
            printf("Optimization:   %gms\n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
            t = clock();
        }
        module = tin_astemit_modemit(state->emitter, &statements, module_name);
        free_statements(state, &statements);
        if(measurecompilationtime)
        {
            printf("Emitting:       %gms\n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
            printf("\nTotal:          %gms\n-----------------------\n",
                   (double)(clock() - total_t) / CLOCKS_PER_SEC * 1000 + lastsourcetime);
        }
    }
    state->gcallow = allowedgc;
    return state->haderror ? NULL : module;
}

TinModule* tin_state_getmodule(TinState* state, const char* name)
{
    TinValue value;
    if(tin_table_get(&state->vm->modules->values, tin_string_copyconst(state, name), &value))
    {
        return tin_value_asmodule(value);
    }
    return NULL;
}

TinInterpretResult tin_state_execsource(TinState* state, const char* module_name, const char* code, size_t len)
{
    return tin_state_internexecsource(state, tin_string_copy(state, module_name, strlen(module_name)), code, len);
}


TinInterpretResult tin_state_internexecsource(TinState* state, TinString* module_name, const char* code, size_t len)
{
    intptr_t istack;
    intptr_t itop;
    intptr_t idif;
    TinModule* module;
    TinFiber* fiber;
    TinInterpretResult result;
    module = tin_state_compilemodule(state, module_name, code, len);
    if(module == NULL)
    {
        return TIN_MAKESTATUS(TINSTATE_COMPILEERROR, tin_value_makenull(state));
    }
    
    result = tin_vm_execmodule(state, module);
    fiber = module->mainfiber;
    if(!state->haderror && !fiber->abort && fiber->stacktop != fiber->stackvalues)
    {
        istack = (intptr_t)(fiber->stackvalues);
        itop = (intptr_t)(fiber->stacktop);
        idif = (intptr_t)(fiber->stackvalues - fiber->stacktop);
        /* me fail english. how do i put this better? */
        tin_state_raiseerror(state, RUNTIME_ERROR, "stack should be same as stack top", idif, istack, istack, itop, itop);
    }
    state->lastmodule = module;
    return result;
}


bool tin_state_compileandsave(TinState* state, char* files[], size_t numfiles, const char* outputfile)
{
    size_t i;
    size_t len;
    char* filename;
    char* source;
    FILE* file;
    TinString* module_name;
    TinModule* module;
    TinModule** compiledmodules;
    compiledmodules = (TinModule**)tin_gcmem_allocate(state, sizeof(TinModule*), numfiles+1);
    tin_astopt_setoptlevel(TINOPTLEVEL_EXTREME);
    for(i = 0; i < numfiles; i++)
    {
        filename = tin_util_copystring(files[i]);
        source = tin_util_readfile(filename, &len);
        if(source == NULL)
        {
            tin_state_raiseerror(state, COMPILE_ERROR, "failed to open file '%s' for reading", filename);
            return false;
        }
        filename = tin_util_patchfilename(filename);
        module_name = tin_string_copy(state, filename, strlen(filename));
        module = tin_state_compilemodule(state, module_name, source, len);
        compiledmodules[i] = module;
        free((void*)source);
        free((void*)filename);
        if(module == NULL)
        {
            return false;
        }
    }
    file = fopen(outputfile, "w+b");
    if(file == NULL)
    {
        tin_state_raiseerror(state, COMPILE_ERROR, "failed to open file '%s' for writing", outputfile);
        return false;
    }
    tin_ioutil_writeuint16(file, TIN_BYTECODE_MAGIC_NUMBER);
    tin_ioutil_writeuint8(file, TIN_BYTECODE_VERSION);
    tin_ioutil_writeuint16(file, numfiles);
    for(i = 0; i < numfiles; i++)
    {
        tin_ioutil_writemodule(compiledmodules[i], file);
    }
    tin_ioutil_writeuint16(file, TIN_BYTECODE_END_NUMBER);
    tin_gcmem_free(state, sizeof(TinModule), compiledmodules);
    fclose(file);
    return true;
}

static char* tin_util_readsource(TinState* state, const char* file, char** patchedfilename, size_t* dlen)
{
    clock_t t;
    size_t len;
    char* filename;
    char* source;
    t = 0;
    if(measurecompilationtime)
    {
        t = clock();
    }
    filename = tin_util_copystring(file);
    source = tin_util_readfile(filename, &len);
    if(source == NULL)
    {
        tin_state_raiseerror(state, RUNTIME_ERROR, "failed to open file '%s' for reading", filename);
        return NULL;
    }
    *dlen = len;
    *patchedfilename = tin_util_patchfilename(filename);
    if(measurecompilationtime)
    {
        printf("reading source: %gms\n", lastsourcetime = (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
    }
    return source;
}

TinInterpretResult tin_state_execfile(TinState* state, const char* file)
{
    size_t len;
    char* source;
    char* patchedfilename;
    TinInterpretResult result;
    patchedfilename = NULL;
    source = tin_util_readsource(state, file, &patchedfilename, &len);
    if(source == NULL)
    {
        return INTERPRET_RUNTIME_FAIL(state);
    }
    result = tin_state_execsource(state, patchedfilename, source, len);
    free(patchedfilename);
    free(source);
    return result;
}

TinInterpretResult tin_state_dumpfile(TinState* state, const char* file)
{
    size_t len;
    char* patchedfilename;
    char* source;
    TinInterpretResult result;
    TinString* module_name;
    TinModule* module;
    source = tin_util_readsource(state, file, &patchedfilename, &len);
    if(source == NULL)
    {
        return INTERPRET_RUNTIME_FAIL(state);
    }
    module_name = tin_string_copy(state, patchedfilename, strlen(patchedfilename));
    module = tin_state_compilemodule(state, module_name, source, len);
    if(module == NULL)
    {
        result = INTERPRET_RUNTIME_FAIL(state);
    }
    else
    {
        tin_disassemble_module(state, module, source);
        result = TIN_MAKESTATUS(TINSTATE_OK, tin_value_makenull(state));
    }
    free((void*)source);
    free((void*)patchedfilename);
    return result;
}

void tin_state_raiseerror(TinState* state, TinErrType type, const char* message, ...)
{
    size_t buffersize;
    char* buffer;
    va_list args;
    va_list argscopy;
    (void)type;
    va_start(args, message);
    va_copy(argscopy, args);
    buffersize = vsnprintf(NULL, 0, message, argscopy) + 1;
    va_end(argscopy);
    buffer = (char*)tin_gcmem_allocate(state, sizeof(char), buffersize+1);
    vsnprintf(buffer, buffersize, message, args);
    va_end(args);
    state->errorfn(state, buffer);
    state->haderror = true;
    /* TODO: is this safe? */
    free(buffer);
}

