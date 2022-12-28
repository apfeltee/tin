
#include <string.h>
#include <math.h>
#include "lit.h"

#define PUSH(value) (*fiber->stack_top++ = value)

#define RETURN_OK(r) return (LitInterpretResult){ LITRESULT_OK, r };

#define RETURN_RUNTIME_ERROR() return (LitInterpretResult){ LITRESULT_RUNTIME_ERROR, NULL_VALUE };



void lit_init_api(LitState* state)
{
    state->api_name = lit_string_copy(state, "c", 1);
    state->api_function = NULL;
    state->api_fiber = NULL;
}

void lit_free_api(LitState* state)
{
    state->api_name = NULL;
    state->api_function = NULL;
    state->api_fiber = NULL;
}

LitValue lit_get_global(LitState* state, LitString* name)
{
    LitValue global;
    if(!lit_table_get(&state->vm->globals->values, name, &global))
    {
        return NULL_VALUE;
    }
    return global;
}

LitFunction* lit_get_global_function(LitState* state, LitString* name)
{
    LitValue function = lit_get_global(state, name);
    if(IS_FUNCTION(function))
    {
        return AS_FUNCTION(function);
    }
    return NULL;
}

void lit_set_global(LitState* state, LitString* name, LitValue value)
{
    lit_push_root(state, (LitObject*)name);
    lit_push_value_root(state, value);
    lit_table_set(state, &state->vm->globals->values, name, value);
    lit_pop_roots(state, 2);
}

bool lit_global_exists(LitState* state, LitString* name)
{
    LitValue global;
    return lit_table_get(&state->vm->globals->values, name, &global);
}

void lit_define_native(LitState* state, const char* name, LitNativeFunctionFn native)
{
    lit_push_root(state, (LitObject*)CONST_STRING(state, name));
    lit_push_root(state, (LitObject*)lit_create_native_function(state, native, lit_as_string(lit_peek_root(state, 0))));
    lit_table_set(state, &state->vm->globals->values, lit_as_string(lit_peek_root(state, 1)), lit_peek_root(state, 0));
    lit_pop_roots(state, 2);
}

void lit_define_native_primitive(LitState* state, const char* name, LitNativePrimitiveFn native)
{
    lit_push_root(state, (LitObject*)CONST_STRING(state, name));
    lit_push_root(state, (LitObject*)lit_create_native_primitive(state, native, lit_as_string(lit_peek_root(state, 0))));
    lit_table_set(state, &state->vm->globals->values, lit_as_string(lit_peek_root(state, 1)), lit_peek_root(state, 0));
    lit_pop_roots(state, 2);
}

LitValue lit_instance_get_method(LitState* state, LitValue callee, LitString* mthname)
{
    LitValue mthval;
    LitClass* klass;
    klass = lit_get_class_for(state, callee);
    if((IS_INSTANCE(callee) && lit_table_get(&AS_INSTANCE(callee)->fields, mthname, &mthval)) || lit_table_get(&klass->methods, mthname, &mthval))
    {
        return mthval;
    }
    return NULL_VALUE;
}

LitInterpretResult lit_instance_call_method(LitState* state, LitValue callee, LitString* mthname, LitValue* argv, size_t argc)
{
    LitValue mthval;
    mthval = lit_instance_get_method(state, callee, mthname);
    if(!IS_NULL(mthval))
    {
        return lit_call(state, mthval, argv, argc, false);
    }
    return INTERPRET_RUNTIME_FAIL;    
}


double lit_check_number(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !IS_NUMBER(args[id]))
    {
        lit_runtime_error_exiting(vm, "expected a number as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_get_value_type(args[id]));
    }
    return lit_value_to_number(args[id]);
}

double lit_get_number(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, double def)
{
    (void)vm;
    if(arg_count <= id || !IS_NUMBER(args[id]))
    {
        return def;
    }
    return lit_value_to_number(args[id]);
}

bool lit_check_bool(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !IS_BOOL(args[id]))
    {
        lit_runtime_error_exiting(vm, "expected a boolean as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_get_value_type(args[id]));
    }

    return lit_as_bool(args[id]);
}

bool lit_get_bool(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, bool def)
{
    (void)vm;
    if(arg_count <= id || !IS_BOOL(args[id]))
    {
        return def;
    }
    return lit_as_bool(args[id]);
}

const char* lit_check_string(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !IS_STRING(args[id]))
    {
        lit_runtime_error_exiting(vm, "expected a string as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_get_value_type(args[id]));
    }

    return lit_as_string(args[id])->chars;
}

const char* lit_get_string(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id, const char* def)
{
    (void)vm;
    if(arg_count <= id || !IS_STRING(args[id]))
    {
        return def;
    }

    return lit_as_string(args[id])->chars;
}

LitString* lit_check_object_string(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !IS_STRING(args[id]))
    {
        lit_runtime_error_exiting(vm, "expected a string as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_get_value_type(args[id]));
    }

    return lit_as_string(args[id]);
}

LitInstance* lit_check_instance(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !IS_INSTANCE(args[id]))
    {
        lit_runtime_error_exiting(vm, "expected an instance as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_get_value_type(args[id]));
    }

    return AS_INSTANCE(args[id]);
}

LitValue* lit_check_reference(LitVM* vm, LitValue* args, uint8_t arg_count, uint8_t id)
{
    if(arg_count <= id || !IS_REFERENCE(args[id]))
    {
        lit_runtime_error_exiting(vm, "expected a reference as argument #%i, got a %s", (int)id,
                                  id >= arg_count ? "null" : lit_get_value_type(args[id]));
    }

    return AS_REFERENCE(args[id])->slot;
}

void lit_ensure_bool(LitVM* vm, LitValue value, const char* error)
{
    if(!IS_BOOL(value))
    {
        lit_runtime_error_exiting(vm, error);
    }
}

void lit_ensure_string(LitVM* vm, LitValue value, const char* error)
{
    if(!IS_STRING(value))
    {
        lit_runtime_error_exiting(vm, error);
    }
}

void lit_ensure_number(LitVM* vm, LitValue value, const char* error)
{
    if(!IS_NUMBER(value))
    {
        lit_runtime_error_exiting(vm, error);
    }
}

void lit_ensure_object_type(LitVM* vm, LitValue value, LitObjectType type, const char* error)
{
    if(!IS_OBJECT(value) || OBJECT_TYPE(value) != type)
    {
        lit_runtime_error_exiting(vm, error);
    }
}

LitValue lit_get_field(LitState* state, LitTable* table, const char* name)
{
    LitValue value;

    if(!lit_table_get(table, CONST_STRING(state, name), &value))
    {
        value = NULL_VALUE;
    }

    return value;
}

LitValue lit_get_map_field(LitState* state, LitMap* map, const char* name)
{
    LitValue value;

    if(!lit_table_get(&map->values, CONST_STRING(state, name), &value))
    {
        value = NULL_VALUE;
    }

    return value;
}

void lit_set_field(LitState* state, LitTable* table, const char* name, LitValue value)
{
    lit_table_set(state, table, CONST_STRING(state, name), value);
}

void lit_set_map_field(LitState* state, LitMap* map, const char* name, LitValue value)
{
    lit_table_set(state, &map->values, CONST_STRING(state, name), value);
}

void lit_uintlist_init(LitUInts* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_uintlist_destroy(LitState* state, LitUInts* array)
{
    LIT_FREE_ARRAY(state, size_t, array->values, array->capacity);
    lit_uintlist_init(array);
}

void lit_uintlist_push(LitState* state, LitUInts* array, size_t value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, size_t, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void lit_init_bytes(LitBytes* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_free_bytes(LitState* state, LitBytes* array)
{
    LIT_FREE_ARRAY(state, uint8_t, array->values, array->capacity);
    lit_init_bytes(array);
}

void lit_bytes_write(LitState* state, LitBytes* array, uint8_t value)
{
    if(array->capacity < array->count + 1)
    {
        size_t old_capacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(old_capacity);
        array->values = LIT_GROW_ARRAY(state, array->values, uint8_t, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

static bool ensure_fiber(LitVM* vm, LitFiber* fiber)
{
    size_t newcapacity;
    size_t osize;
    size_t newsize;
    if(fiber == NULL)
    {
        lit_runtime_error(vm, "no fiber to run on");
        return true;
    }
    if(fiber->frame_count == LIT_CALL_FRAMES_MAX)
    {
        lit_runtime_error(vm, "fiber frame overflow");
        return true;
    }
    if(fiber->frame_count + 1 > fiber->frame_capacity)
    {
        //newcapacity = fmin(LIT_CALL_FRAMES_MAX, fiber->frame_capacity * 2);
        newcapacity = (fiber->frame_capacity * 2) + 1;
        osize = (sizeof(LitCallFrame) * fiber->frame_capacity);
        newsize = (sizeof(LitCallFrame) * newcapacity);
        fiber->frames = (LitCallFrame*)lit_reallocate(vm->state, fiber->frames, osize, newsize);
        fiber->frame_capacity = newcapacity;
    }

    return false;
}

static inline LitCallFrame* setup_call(LitState* state, LitFunction* callee, LitValue* argv, uint8_t argc, bool ignfiber)
{
    bool vararg;
    int amount;
    size_t i;
    size_t varargc;
    size_t function_arg_count;
    LitVM* vm;
    LitFiber* fiber;
    LitCallFrame* frame;
    LitArray* array;
    (void)argc;
    (void)varargc;
    vm = state->vm;
    fiber = vm->fiber;
    if(callee == NULL)
    {
        lit_runtime_error(vm, "attempt to call a null value");
        return NULL;
    }
    if(ignfiber)
    {
        if(fiber == NULL)
        {
            fiber = state->api_fiber;
        }
    }
    if(!ignfiber)
    {
        if(ensure_fiber(vm, fiber))
        {
            return NULL;
        }        
    }
    lit_ensure_fiber_stack(state, fiber, callee->max_slots + (int)(fiber->stack_top - fiber->stack));
    frame = &fiber->frames[fiber->frame_count++];
    frame->slots = fiber->stack_top;
    PUSH(OBJECT_VALUE(callee));
    for(i = 0; i < argc; i++)
    {
        PUSH(argv[i]);
    }
    function_arg_count = callee->arg_count;
    if(argc != function_arg_count)
    {
        vararg = callee->vararg;
        if(argc < function_arg_count)
        {
            amount = (int)function_arg_count - argc - (vararg ? 1 : 0);
            for(i = 0; i < (size_t)amount; i++)
            {
                PUSH(NULL_VALUE);
            }
            if(vararg)
            {
                PUSH(OBJECT_VALUE(lit_create_array(vm->state)));
            }
        }
        else if(callee->vararg)
        {
            array = lit_create_array(vm->state);
            varargc = argc - function_arg_count + 1;
            lit_vallist_ensuresize(vm->state, &array->list, varargc);
            for(i = 0; i < varargc; i++)
            {
                array->list.values[i] = fiber->stack_top[(int)i - (int)varargc];
            }

            fiber->stack_top -= varargc;
            lit_push(vm, OBJECT_VALUE(array));
        }
        else
        {
            fiber->stack_top -= (argc - function_arg_count);
        }
    }
    else if(callee->vararg)
    {
        array = lit_create_array(vm->state);
        varargc = argc - function_arg_count + 1;
        lit_vallist_push(vm->state, &array->list, *(fiber->stack_top - 1));
        *(fiber->stack_top - 1) = OBJECT_VALUE(array);
    }
    frame->ip = callee->chunk.code;
    frame->closure = NULL;
    frame->function = callee;
    frame->result_ignored = false;
    frame->return_to_c = true;
    return frame;
}

static inline LitInterpretResult execute_call(LitState* state, LitCallFrame* frame)
{
    LitFiber* fiber;
    LitInterpretResult result;
    if(frame == NULL)
    {
        RETURN_RUNTIME_ERROR();
    }
    fiber = state->vm->fiber;
    result = lit_interpret_fiber(state, fiber);
    if(fiber->error != NULL_VALUE)
    {
        result.result = fiber->error;
    }
    return result;
}

LitInterpretResult lit_call_function(LitState* state, LitFunction* callee, LitValue* argv, uint8_t argc, bool ignfiber)
{
    return execute_call(state, setup_call(state, callee, argv, argc, ignfiber));
}

LitInterpretResult lit_call_closure(LitState* state, LitClosure* callee, LitValue* argv, uint8_t argc, bool ignfiber)
{
    LitCallFrame* frame;
    frame = setup_call(state, callee->function, argv, argc, ignfiber);
    if(frame == NULL)
    {
        RETURN_RUNTIME_ERROR();
    }
    frame->closure = callee;
    return execute_call(state, frame);
}

LitInterpretResult lit_call_method(LitState* state, LitValue instance, LitValue callee, LitValue* argv, uint8_t argc, bool ignfiber)
{
    uint8_t i;
    LitVM* vm;
    LitInterpretResult lir;
    LitObjectType type;
    LitClass* klass;
    LitFiber* fiber;
    LitValue* slot;
    LitNativeMethod* natmethod;
    LitBoundMethod* bound_method;
    LitValue mthval;
    LitValue result;
    lir.result = NULL_VALUE;
    lir.type = LITRESULT_OK;
    vm = state->vm;
    if(IS_OBJECT(callee))
    {
        if(lit_set_native_exit_jump())
        {
            RETURN_RUNTIME_ERROR();
        }
        type = OBJECT_TYPE(callee);

        if(type == LITTYPE_FUNCTION)
        {
            return lit_call_function(state, AS_FUNCTION(callee), argv, argc, ignfiber);
        }
        else if(type == LITTYPE_CLOSURE)
        {
            return lit_call_closure(state, AS_CLOSURE(callee), argv, argc, ignfiber);
        }
        fiber = vm->fiber;
        if(ignfiber)
        {
            if(fiber == NULL)
            {
                fiber = state->api_fiber;
            }
        }
        if(!ignfiber)
        {
            if(ensure_fiber(vm, fiber))
            {
                RETURN_RUNTIME_ERROR();
            }
        }
        lit_ensure_fiber_stack(state, fiber, 3 + argc + (int)(fiber->stack_top - fiber->stack));
        slot = fiber->stack_top;
        PUSH(instance);
        if(type != LITTYPE_CLASS)
        {
            for(i = 0; i < argc; i++)
            {
                PUSH(argv[i]);
            }
        }
        switch(type)
        {
            case LITTYPE_NATIVE_FUNCTION:
                {
                    LitValue result = AS_NATIVE_FUNCTION(callee)->function(vm, argc, fiber->stack_top - argc);
                    fiber->stack_top = slot;
                    RETURN_OK(result);
                }
                break;
            case LITTYPE_NATIVE_PRIMITIVE:
                {
                    AS_NATIVE_PRIMITIVE(callee)->function(vm, argc, fiber->stack_top - argc);
                    fiber->stack_top = slot;
                    RETURN_OK(NULL_VALUE);
                }
                break;
            case LITTYPE_NATIVE_METHOD:
                {
                    natmethod = AS_NATIVE_METHOD(callee);
                    result = natmethod->method(vm, *(fiber->stack_top - argc - 1), argc, fiber->stack_top - argc);
                    fiber->stack_top = slot;
                    RETURN_OK(result);
                }
                break;
            case LITTYPE_CLASS:
                {
                    klass = AS_CLASS(callee);
                    *slot = OBJECT_VALUE(lit_create_instance(vm->state, klass));
                    if(klass->init_method != NULL)
                    {
                        lir = lit_call_method(state, *slot, OBJECT_VALUE(klass->init_method), argv, argc, ignfiber);
                    }
                    // TODO: when should this return *slot instead of lir?
                    fiber->stack_top = slot;
                    //RETURN_OK(*slot);
                    return lir;
                }
                break;
            case LITTYPE_BOUND_METHOD:
                {
                    bound_method = AS_BOUND_METHOD(callee);
                    mthval = bound_method->method;
                    *slot = bound_method->receiver;
                    if(IS_NATIVE_METHOD(mthval))
                    {
                        result = AS_NATIVE_METHOD(mthval)->method(vm, bound_method->receiver, argc, fiber->stack_top - argc);
                        fiber->stack_top = slot;
                        RETURN_OK(result);
                    }
                    else if(IS_PRIMITIVE_METHOD(mthval))
                    {
                        AS_PRIMITIVE_METHOD(mthval)->method(vm, bound_method->receiver, argc, fiber->stack_top - argc);

                        fiber->stack_top = slot;
                        RETURN_OK(NULL_VALUE);
                    }
                    else
                    {
                        fiber->stack_top = slot;
                        return lit_call_function(state, AS_FUNCTION(mthval), argv, argc, ignfiber);
                    }
                }
                break;
            case LITTYPE_PRIMITIVE_METHOD:
                {
                    AS_PRIMITIVE_METHOD(callee)->method(vm, *(fiber->stack_top - argc - 1), argc, fiber->stack_top - argc);
                    fiber->stack_top = slot;
                    RETURN_OK(NULL_VALUE);
                }
                break;
            default:
                {
                }
                break;
        }
    }
    if(IS_NULL(callee))
    {
        lit_runtime_error(vm, "attempt to call a null value");
    }
    else
    {
        lit_runtime_error(vm, "can only call functions and classes");
    }

    RETURN_RUNTIME_ERROR();
}

LitInterpretResult lit_call(LitState* state, LitValue callee, LitValue* argv, uint8_t argc, bool ignfiber)
{
    return lit_call_method(state, callee, callee, argv, argc, ignfiber);
}

LitInterpretResult lit_find_and_call_method(LitState* state, LitValue callee, LitString* method_name, LitValue* argv, uint8_t argc, bool ignfiber)
{
    LitClass* klass;
    LitVM* vm;
    LitFiber* fiber;
    LitValue mthval;
    vm = state->vm;
    fiber = vm->fiber;
    if(fiber == NULL)
    {
        if(!ignfiber)
        {
            lit_runtime_error(vm, "no fiber to run on");
            RETURN_RUNTIME_ERROR();
        }
    }
    klass = lit_get_class_for(state, callee);
    if((IS_INSTANCE(callee) && lit_table_get(&AS_INSTANCE(callee)->fields, method_name, &mthval)) || lit_table_get(&klass->methods, method_name, &mthval))
    {
        return lit_call_method(state, callee, mthval, argv, argc, ignfiber);
    }
    return (LitInterpretResult){ LITRESULT_INVALID, NULL_VALUE };
}

LitString* lit_to_string(LitState* state, LitValue object)
{
    LitValue* slot;
    LitVM* vm;
    LitFiber* fiber;
    LitFunction* function;
    LitChunk* chunk;
    LitCallFrame* frame;
    LitInterpretResult result;
    if(IS_STRING(object))
    {
        return lit_as_string(object);
    }
    else if(!IS_OBJECT(object))
    {
        if(IS_NULL(object))
        {
            return CONST_STRING(state, "null");
        }
        else if(IS_NUMBER(object))
        {
            return lit_as_string(lit_string_number_to_string(state, lit_value_to_number(object)));
        }
        else if(IS_BOOL(object))
        {
            return CONST_STRING(state, lit_as_bool(object) ? "true" : "false");
        }
    }
    else if(IS_REFERENCE(object))
    {
        slot = AS_REFERENCE(object)->slot;

        if(slot == NULL)
        {
            return CONST_STRING(state, "null");
        }
        return lit_to_string(state, *slot);
    }
    vm = state->vm;
    fiber = vm->fiber;
    if(ensure_fiber(vm, fiber))
    {
        return CONST_STRING(state, "null");
    }
    function = state->api_function;
    if(function == NULL)
    {
        function = state->api_function = lit_create_function(state, fiber->module);
        function->chunk.has_line_info = false;
        function->name = state->api_name;
        chunk = &function->chunk;
        chunk->count = 0;
        chunk->constants.count = 0;
        function->max_slots = 3;
        lit_write_chunk(state, chunk, OP_INVOKE, 1);
        lit_emit_byte(state, chunk, 0);
        lit_emit_short(state, chunk, lit_chunk_add_constant(state, chunk, OBJECT_CONST_STRING(state, "toString")));
        lit_emit_byte(state, chunk, OP_RETURN);
    }
    lit_ensure_fiber_stack(state, fiber, function->max_slots + (int)(fiber->stack_top - fiber->stack));
    frame = &fiber->frames[fiber->frame_count++];
    frame->ip = function->chunk.code;
    frame->closure = NULL;
    frame->function = function;
    frame->slots = fiber->stack_top;
    frame->result_ignored = false;
    frame->return_to_c = true;
    PUSH(OBJECT_VALUE(function));
    PUSH(object);
    result = lit_interpret_fiber(state, fiber);
    if(result.type != LITRESULT_OK)
    {
        return CONST_STRING(state, "null");
    }
    return lit_as_string(result.result);
}

LitValue lit_call_new(LitVM* vm, const char* name, LitValue* args, size_t argc, bool ignfiber)
{
    LitValue value;
    LitClass* klass;
    if(!lit_table_get(&vm->globals->values, CONST_STRING(vm->state, name), &value))
    {
        lit_runtime_error(vm, "failed to create instance of class %s: class not found", name);
        return NULL_VALUE;
    }
    klass = AS_CLASS(value);
    if(klass->init_method == NULL)
    {
        return OBJECT_VALUE(lit_create_instance(vm->state, klass));
    }
    return lit_call_method(vm->state, value, value, args, argc, ignfiber).result;
}
