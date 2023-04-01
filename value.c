
#include <stdarg.h>
#include <stdio.h>
#include "priv.h"

TinValue tin_value_fromobject_actual(TinObject* obj)
{
    TinValue val;
    val.type = TINVAL_OBJECT;
    val.isfixednumber = false;
    val.obj = obj;
    return val;
}


TinObject* tin_value_asobject(TinValue v)
{
    return v.obj;
}


TinObjType tin_value_type(TinValue v)
{
    TinObject* o;
    if(tin_value_isobject(v))
    {
        o = tin_value_asobject(v);
        if(o == NULL)
        {
            return TINTYPE_UNDEFINED;
        }
        return o->type;
    }
    return TINTYPE_UNDEFINED;
}

TinValue tin_value_makenumber(TinState* state, double num)
{
    return tin_value_makefloatnumber(state, num);
}

TinValue tin_value_makefloatnumber(TinState* state, double num)
{
    (void)state;
    TinValue v;
    v.type = TINVAL_NUMBER;
    v.isfixednumber = false;
    v.numfloatval = num;
    return v;
}


TinValue tin_value_makefixednumber(TinState* state, int64_t num)
{
    (void)state;
    TinValue v;
    v.type = TINVAL_NUMBER;
    v.isfixednumber = true;
    v.numfixedval = num;
    return v;
}

bool tin_valcompare_object(TinState* state, const TinValue a, const TinValue b)
{
    (void)state;
    (void)b;
    switch(a.obj->type)
    {
        default:
            {
                fprintf(stderr, "missing equality comparison for type '%s'", tin_tostring_typename(a));
            }
            break;
    }
    return false;
}

bool tin_value_compare(TinState* state, const TinValue a, const TinValue b)
{
    TinValType t1;
    TinValType t2;
    TinInterpretResult inret;
    TinValue args[3];
    if(tin_value_isinstance(a))
    {
        args[0] = b;
        inret = tin_state_callinstancemethod(state, a, tin_string_copyconst(state, "=="), args, 1);
        if(inret.type == TINSTATE_OK)
        {
            return false;
        }
    }
    t1 = a.type;
    t2 = b.type;
    //fprintf(stderr, "compare: t1=%d t2=%d\n", t1, t2);
    if(t1 == t2)
    {
        switch(t1)
        {
            case TINVAL_NUMBER:
                {
                    if(a.isfixednumber && b.isfixednumber)
                    {
                        return (tin_value_asfixednumber(a) == tin_value_asfixednumber(b));
                    }
                    else if(!a.isfixednumber && !b.isfixednumber)
                    {
                        return (tin_value_asfloatnumber(a) == tin_value_asfloatnumber(b));
                    }
                    return (tin_value_asnumber(a) == tin_value_asnumber(b));
                }
                break;
            case TINVAL_NULL:
                {
                    return true;
                }
                break;
            case TINVAL_BOOL:
                {
                    return a.boolval == b.boolval;
                }
                break;
            case TINVAL_OBJECT:
                {
                    return tin_valcompare_object(state, a, b);
                }
                break;
        }
    }
    return false;
}

TinString* tin_value_tostring(TinState* state, TinValue object)
{
    TinValue* slot;
    TinVM* vm;
    TinFiber* fiber;
    TinFunction* function;
    TinChunk* chunk;
    TinCallFrame* frame;
    TinInterpretResult result;
    if(tin_value_isstring(object))
    {
        return tin_value_asstring(object);
    }
    else if(!tin_value_isobject(object))
    {
        if(tin_value_isnull(object))
        {
            return tin_string_copyconst(state, "null");
        }
        else if(tin_value_isnumber(object))
        {
            return tin_value_asstring(tin_string_numbertostring(state, tin_value_asnumber(object)));
        }
        else if(tin_value_isbool(object))
        {
            return tin_string_copyconst(state, tin_value_asbool(object) ? "true" : "false");
        }
    }
    else if(tin_value_isreference(object))
    {
        slot = tin_value_asreference(object)->slot;
        if(slot == NULL)
        {
            return tin_string_copyconst(state, "null");
        }
        return tin_value_tostring(state, *slot);
    }
    vm = state->vm;
    fiber = vm->fiber;
    if(tin_state_ensurefiber(vm, fiber))
    {
        return tin_string_copyconst(state, "null");
    }
    function = state->capifunction;
    if(function == NULL)
    {
        function = state->capifunction = tin_object_makefunction(state, fiber->module);
        function->chunk.haslineinfo = false;
        function->name = state->capiname;
        chunk = &function->chunk;
        chunk->count = 0;
        tin_vallist_setcount(&chunk->constants, 0);
        function->maxslots = 3;
        tin_chunk_push(state, chunk, OP_INVOKEMETHOD, 1);
        tin_chunk_emitbyte(state, chunk, 0);
        tin_chunk_emitshort(state, chunk, tin_chunk_addconst(state, chunk, tin_value_makestring(state, "toString")));
        tin_chunk_emitbyte(state, chunk, OP_RETURN);
    }
    tin_fiber_ensurestack(state, fiber, function->maxslots + (int)(fiber->stacktop - fiber->stackvalues));
    frame = &fiber->framevalues[fiber->framecount++];
    frame->ip = function->chunk.code;
    frame->closure = NULL;
    frame->function = function;
    frame->slots = fiber->stacktop;
    frame->ignresult = false;
    frame->returntonative = true;
    tin_vm_push(state->vm, tin_value_fromobject(function));
    tin_vm_push(state->vm, object);
    result = tin_vm_execfiber(state, fiber);
    if(result.type != TINSTATE_OK)
    {
        return tin_string_copyconst(state, "null");
    }
    return tin_value_asstring(result.result);
}


double tin_args_checknumber(TinVM* vm, TinValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !tin_value_isnumber(args[id]))
    {
        tin_vm_raiseexitingerror(vm, "expected a number as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : tin_tostring_typename(args[id]));
    }
    return tin_value_asnumber(args[id]);
}

double tin_value_getnumber(TinVM* vm, TinValue* args, uint8_t arg_count, uint8_t id, double def)
{
    (void)vm;
    if(arg_count <= id || !tin_value_isnumber(args[id]))
    {
        return def;
    }
    return tin_value_asnumber(args[id]);
}

bool tin_args_checkbool(TinVM* vm, TinValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !tin_value_isbool(args[id]))
    {
        tin_vm_raiseexitingerror(vm, "expected a boolean as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : tin_tostring_typename(args[id]));
    }
    return tin_value_asbool(args[id]);
}

bool tin_value_getbool(TinVM* vm, TinValue* args, uint8_t arg_count, uint8_t id, bool def)
{
    (void)vm;
    if(arg_count <= id || !tin_value_isbool(args[id]))
    {
        return def;
    }
    return tin_value_asbool(args[id]);
}

const char* tin_args_checkstring(TinVM* vm, TinValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !tin_value_isstring(args[id]))
    {
        tin_vm_raiseexitingerror(vm, "expected a string as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : tin_tostring_typename(args[id]));
    }
    return tin_value_asstring(args[id])->data;
}

const char* tin_value_getstring(TinVM* vm, TinValue* args, uint8_t arg_count, uint8_t id, const char* def)
{
    (void)vm;
    if(arg_count <= id || !tin_value_isstring(args[id]))
    {
        return def;
    }
    return tin_value_asstring(args[id])->data;
}

TinString* tin_args_checkobjstring(TinVM* vm, TinValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !tin_value_isstring(args[id]))
    {
        tin_vm_raiseexitingerror(vm, "expected a string as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : tin_tostring_typename(args[id]));
    }

    return tin_value_asstring(args[id]);
}

TinInstance* tin_args_checkinstance(TinVM* vm, TinValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !tin_value_isinstance(args[id]))
    {
        tin_vm_raiseexitingerror(vm, "expected an instance as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : tin_tostring_typename(args[id]));
    }
    return tin_value_asinstance(args[id]);
}

TinValue* tin_args_checkreference(TinVM* vm, TinValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !tin_value_isreference(args[id]))
    {
        tin_vm_raiseexitingerror(vm, "expected a reference as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : tin_tostring_typename(args[id]));
    }
    return tin_value_asreference(args[id])->slot;
}

void tin_value_ensurebool(TinVM* vm, TinValue value, const char* emsg)
{
    if(!tin_value_isbool(value))
    {
        tin_vm_raiseexitingerror(vm, emsg);
    }
}

void tin_value_ensurestring(TinVM* vm, TinValue value, const char* emsg)
{
    if(!tin_value_isstring(value))
    {
        tin_vm_raiseexitingerror(vm, emsg);
    }
}

void tin_value_ensurenumber(TinVM* vm, TinValue value, const char* emsg)
{
    if(!tin_value_isnumber(value))
    {
        tin_vm_raiseexitingerror(vm, emsg);
    }
}

void tin_value_ensureobjtype(TinVM* vm, TinValue value, TinObjType type, const char* emsg)
{
    if(!tin_value_isobject(value) || tin_value_type(value) != type)
    {
        tin_vm_raiseexitingerror(vm, emsg);
    }
}

TinValue tin_value_callnew(TinVM* vm, const char* name, TinValue* args, size_t argc, bool ignfiber)
{
    TinValue value;
    TinClass* klass;
    if(!tin_table_get(&vm->globals->values, tin_string_copyconst(vm->state, name), &value))
    {
        tin_vm_raiseerror(vm, "failed to create instance of class %s: class not found", name);
        return tin_value_makenull(vm->state);
    }
    klass = tin_value_asclass(value);
    if(klass->initmethod == NULL)
    {
        return tin_value_fromobject(tin_object_makeinstance(vm->state, klass));
    }
    return tin_state_callmethod(vm->state, value, value, args, argc, ignfiber).result;
}

