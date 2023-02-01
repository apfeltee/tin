
#include <stdarg.h>
#include <stdio.h>
#include "priv.h"

LitValue lit_value_makeobject_actual(LitObject* obj)
{
    LitValue val;
    val.type = LITVAL_OBJECT;
    val.isfixednumber = false;
    val.obj = obj;
    return val;
}

bool lit_value_isobject(LitValue v)
{
    return v.type == LITVAL_OBJECT;
}

LitObject* lit_value_asobject(LitValue v)
{
    return v.obj;
}

LitValue lit_value_makebool(LitState* state, bool b) 
{
    (void)state;
    if(!b)
    {
        return FALSE_VALUE;
    }
    return TRUE_VALUE;
}

bool lit_value_isbool(LitValue v)
{
    return v.type == LITVAL_BOOL;
}

bool lit_value_isfalsey(LitValue v)
{
    return (lit_value_isbool(v) && !v.boolval) || lit_value_isnull(v) || (lit_value_isnumber(v) && lit_value_asnumber(v) == 0);
}

bool lit_value_asbool(LitValue v)
{
    return v.boolval;
}

bool lit_value_isnull(LitValue v)
{
    return (v.type == LITVAL_NULL);
}

LitObjType lit_value_type(LitValue v)
{
    LitObject* o;
    if(lit_value_isobject(v))
    {
        o = lit_value_asobject(v);
        if(o == NULL)
        {
            return LITTYPE_UNDEFINED;
        }
        return o->type;
    }
    return LITTYPE_UNDEFINED;
}

double lit_value_asfloatnumber(LitValue v)
{
    if(v.isfixednumber)
    {
        return v.numfixedval;
    }
    return v.numfloatval;
}

int64_t lit_value_asfixednumber(LitValue v)
{
    if(!v.isfixednumber)
    {
        return v.numfloatval;
    }
    return v.numfixedval;
}

LitValue lit_value_makenumber(LitState* state, double num)
{
    return lit_value_makefloatnumber(state, num);
}

LitValue lit_value_makefloatnumber(LitState* state, double num)
{
    (void)state;
    LitValue v;
    v.type = LITVAL_NUMBER;
    v.isfixednumber = false;
    v.numfloatval = num;
    return v;
}


LitValue lit_value_makefixednumber(LitState* state, int64_t num)
{
    (void)state;
    LitValue v;
    v.type = LITVAL_NUMBER;
    v.isfixednumber = true;
    v.numfixedval = num;
    return v;
}

bool lit_value_isnumber(LitValue v)
{
    return v.type == LITVAL_NUMBER;
}

bool lit_valcompare_object(LitState* state, const LitValue a, const LitValue b)
{
    switch(a.obj->type)
    {
        default:
            {
                fprintf(stderr, "missing equality comparison for type '%s'", lit_tostring_typename(a));
            }
            break;
    }
    return false;
}

bool lit_value_compare(LitState* state, const LitValue a, const LitValue b)
{
    LitValType t1;
    LitValType t2;
    double n1;
    double n2;
    LitInterpretResult inret;
    LitValue args[3];
    if(lit_value_isinstance(a))
    {
        args[0] = b;
        inret = lit_state_callinstancemethod(state, a, lit_string_copyconst(state, "=="), args, 1);
        if(inret.type == LITRESULT_OK)
        {
            /*
            if(lit_value_makebool(state, inret.result.) == TRUE_VALUE)
            {
                return true;
            }
            */
            return false;
        }
    }
    t1 = a.type;
    t2 = b.type;
    fprintf(stderr, "compare: t1=%d t2=%d\n", t1, t2);
    if(t1 == t2)
    {
        switch(t1)
        {
            case LITVAL_NUMBER:
                {
                    return (lit_value_asnumber(a) == lit_value_asnumber(b));
                }
                break;
            case LITVAL_NULL:
                {
                    return true;
                }
                break;
            case LITVAL_BOOL:
                {
                    return a.boolval == b.boolval;
                }
                break;
            case LITVAL_OBJECT:
                {
                    return lit_valcompare_object(state, a, b);
                }
                break;
        }
    }
    return false;
}

LitString* lit_value_tostring(LitState* state, LitValue object)
{
    LitValue* slot;
    LitVM* vm;
    LitFiber* fiber;
    LitFunction* function;
    LitChunk* chunk;
    LitCallFrame* frame;
    LitInterpretResult result;
    if(lit_value_isstring(object))
    {
        return lit_value_asstring(object);
    }
    else if(!lit_value_isobject(object))
    {
        if(lit_value_isnull(object))
        {
            return lit_string_copyconst(state, "null");
        }
        else if(lit_value_isnumber(object))
        {
            return lit_value_asstring(lit_string_numbertostring(state, lit_value_asnumber(object)));
        }
        else if(lit_value_isbool(object))
        {
            return lit_string_copyconst(state, lit_value_asbool(object) ? "true" : "false");
        }
    }
    else if(lit_value_isreference(object))
    {
        slot = lit_value_asreference(object)->slot;

        if(slot == NULL)
        {
            return lit_string_copyconst(state, "null");
        }
        return lit_value_tostring(state, *slot);
    }
    vm = state->vm;
    fiber = vm->fiber;
    if(lit_state_ensurefiber(vm, fiber))
    {
        return lit_string_copyconst(state, "null");
    }
    function = state->api_function;
    if(function == NULL)
    {
        function = state->api_function = lit_create_function(state, fiber->module);
        function->chunk.has_line_info = false;
        function->name = state->api_name;
        chunk = &function->chunk;
        chunk->count = 0;
        lit_vallist_setcount(&chunk->constants, 0);
        function->max_slots = 3;
        lit_chunk_push(state, chunk, OP_INVOKE, 1);
        lit_chunk_emitbyte(state, chunk, 0);
        lit_chunk_emitshort(state, chunk, lit_chunk_addconst(state, chunk, lit_value_makestring(state, "toString")));
        lit_chunk_emitbyte(state, chunk, OP_RETURN);
    }
    lit_ensure_fiber_stack(state, fiber, function->max_slots + (int)(fiber->stack_top - fiber->stack));
    frame = &fiber->frames[fiber->frame_count++];
    frame->ip = function->chunk.code;
    frame->closure = NULL;
    frame->function = function;
    frame->slots = fiber->stack_top;
    frame->result_ignored = false;
    frame->return_to_c = true;
    PUSH(lit_value_makeobject(function));
    PUSH(object);
    result = lit_vm_execfiber(state, fiber);
    if(result.type != LITRESULT_OK)
    {
        return lit_string_copyconst(state, "null");
    }
    return lit_value_asstring(result.result);
}


double lit_value_checknumber(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !lit_value_isnumber(args[id]))
    {
        lit_vm_raiseexitingerror(vm, "expected a number as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_tostring_typename(args[id]));
    }
    return lit_value_asnumber(args[id]);
}

double lit_value_getnumber(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, double def)
{
    (void)vm;
    if(arg_count <= id || !lit_value_isnumber(args[id]))
    {
        return def;
    }
    return lit_value_asnumber(args[id]);
}

bool lit_value_checkbool(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !lit_value_isbool(args[id]))
    {
        lit_vm_raiseexitingerror(vm, "expected a boolean as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_tostring_typename(args[id]));
    }

    return lit_value_asbool(args[id]);
}

bool lit_value_getbool(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, bool def)
{
    (void)vm;
    if(arg_count <= id || !lit_value_isbool(args[id]))
    {
        return def;
    }
    return lit_value_asbool(args[id]);
}

const char* lit_value_checkstring(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !lit_value_isstring(args[id]))
    {
        lit_vm_raiseexitingerror(vm, "expected a string as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_tostring_typename(args[id]));
    }

    return lit_value_asstring(args[id])->chars;
}

const char* lit_value_getstring(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, const char* def)
{
    (void)vm;
    if(arg_count <= id || !lit_value_isstring(args[id]))
    {
        return def;
    }

    return lit_value_asstring(args[id])->chars;
}

LitString* lit_value_checkobjstring(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !lit_value_isstring(args[id]))
    {
        lit_vm_raiseexitingerror(vm, "expected a string as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_tostring_typename(args[id]));
    }

    return lit_value_asstring(args[id]);
}

LitInstance* lit_value_checkinstance(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !lit_value_isinstance(args[id]))
    {
        lit_vm_raiseexitingerror(vm, "expected an instance as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_tostring_typename(args[id]));
    }

    return lit_value_asinstance(args[id]);
}

LitValue* lit_value_checkreference(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !lit_value_isreference(args[id]))
    {
        lit_vm_raiseexitingerror(vm, "expected a reference as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_tostring_typename(args[id]));
    }

    return lit_value_asreference(args[id])->slot;
}

void lit_value_ensurebool(LitVM* vm, LitValue value, const char* lit_emitter_raiseerror)
{
    if(!lit_value_isbool(value))
    {
        lit_vm_raiseexitingerror(vm, lit_emitter_raiseerror);
    }
}

void lit_value_ensurestring(LitVM* vm, LitValue value, const char* lit_emitter_raiseerror)
{
    if(!lit_value_isstring(value))
    {
        lit_vm_raiseexitingerror(vm, lit_emitter_raiseerror);
    }
}

void lit_value_ensurenumber(LitVM* vm, LitValue value, const char* lit_emitter_raiseerror)
{
    if(!lit_value_isnumber(value))
    {
        lit_vm_raiseexitingerror(vm, lit_emitter_raiseerror);
    }
}

void lit_value_ensureobjtype(LitVM* vm, LitValue value, LitObjType type, const char* lit_emitter_raiseerror)
{
    if(!lit_value_isobject(value) || lit_value_type(value) != type)
    {
        lit_vm_raiseexitingerror(vm, lit_emitter_raiseerror);
    }
}

LitValue lit_value_callnew(LitVM* vm, const char* name, LitValue* args, size_t argc, bool ignfiber)
{
    LitValue value;
    LitClass* klass;
    if(!lit_table_get(&vm->globals->values, lit_string_copyconst(vm->state, name), &value))
    {
        lit_vm_raiseerror(vm, "failed to create instance of class %s: class not found", name);
        return lit_value_makenull(vm->state);
    }
    klass = lit_value_asclass(value);
    if(klass->init_method == NULL)
    {
        return lit_value_makeobject(lit_create_instance(vm->state, klass));
    }
    return lit_state_callmethod(vm->state, value, value, args, argc, ignfiber).result;
}

