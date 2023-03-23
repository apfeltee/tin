
#include "priv.h"

TinFunction* tin_object_makefunction(TinState* state, TinModule* module)
{
    TinFunction* function;
    function = (TinFunction*)tin_gcmem_allocobject(state, sizeof(TinFunction), TINTYPE_FUNCTION, false);
    tin_chunk_init(&function->chunk);
    function->name = NULL;
    function->arg_count = 0;
    function->upvalue_count = 0;
    function->maxslots = 0;
    function->module = module;
    function->vararg = false;
    return function;
}

TinClosure* tin_object_makeclosure(TinState* state, TinFunction* function)
{
    size_t i;
    TinClosure* closure;
    TinUpvalue** upvalues;
    closure = (TinClosure*)tin_gcmem_allocobject(state, sizeof(TinClosure), TINTYPE_CLOSURE, false);
    tin_state_pushroot(state, (TinObject*)closure);
    upvalues = (TinUpvalue**)TIN_ALLOCATE(state, sizeof(TinUpvalue*), function->upvalue_count);
    tin_state_poproot(state);
    for(i = 0; i < function->upvalue_count; i++)
    {
        upvalues[i] = NULL;
    }
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

TinNativeFunction* tin_object_makenativefunction(TinState* state, TinNativeFunctionFn function, TinString* name)
{
    TinNativeFunction* native;
    native = (TinNativeFunction*)tin_gcmem_allocobject(state, sizeof(TinNativeFunction), TINTYPE_NATIVEFUNCTION, false);
    native->function = function;
    native->name = name;
    return native;
}

TinNativePrimFunction* tin_object_makenativeprimitive(TinState* state, TinNativePrimitiveFn function, TinString* name)
{
    TinNativePrimFunction* native;
    native = (TinNativePrimFunction*)tin_gcmem_allocobject(state, sizeof(TinNativePrimFunction), TINTYPE_NATIVEPRIMITIVE, false);
    native->function = function;
    native->name = name;
    return native;
}

TinNativeMethod* tin_object_makenativemethod(TinState* state, TinNativeMethodFn method, TinString* name)
{
    TinNativeMethod* native;
    native = (TinNativeMethod*)tin_gcmem_allocobject(state, sizeof(TinNativeMethod), TINTYPE_NATIVEMETHOD, false);
    native->method = method;
    native->name = name;
    return native;
}

TinPrimitiveMethod* tin_object_makeprimitivemethod(TinState* state, TinPrimitiveMethodFn method, TinString* name)
{
    TinPrimitiveMethod* native;
    native = (TinPrimitiveMethod*)tin_gcmem_allocobject(state, sizeof(TinPrimitiveMethod), TINTYPE_PRIMITIVEMETHOD, false);
    native->method = method;
    native->name = name;
    return native;
}

TinBoundMethod* tin_object_makeboundmethod(TinState* state, TinValue receiver, TinValue method)
{
    TinBoundMethod* boundmethod;
    boundmethod = (TinBoundMethod*)tin_gcmem_allocobject(state, sizeof(TinBoundMethod), TINTYPE_BOUNDMETHOD, false);
    boundmethod->receiver = receiver;
    boundmethod->method = method;
    return boundmethod;
}

bool tin_value_iscallablefunction(TinValue value)
{
    if(tin_value_isobject(value))
    {
        TinObjType type = tin_value_type(value);
        return (
            (type == TINTYPE_CLOSURE) ||
            (type == TINTYPE_FUNCTION) ||
            (type == TINTYPE_NATIVEFUNCTION) ||
            (type == TINTYPE_NATIVEPRIMITIVE) ||
            (type == TINTYPE_NATIVEMETHOD) ||
            (type == TINTYPE_PRIMITIVEMETHOD) ||
            (type == TINTYPE_BOUNDMETHOD)
        );
    }

    return false;
}


static TinValue objfn_function_tostring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinWriter wr;
    (void)argc;
    (void)argv;
    tin_writer_init_string(vm->state, &wr);
    tin_towriter_value(vm->state, &wr, instance, false);
    return tin_value_fromobject(tin_writer_get_string(&wr));
}


static TinValue objfn_function_name(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    return tin_function_getname(vm, instance);
}

void tin_state_openfunctionlibrary(TinState* state)
{
    TinClass* klass;
    klass = tin_object_makeclassname(state, "Function");
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
        tin_class_bindconstructor(state, klass, util_invalid_constructor);
        tin_class_bindmethod(state, klass, "toString", objfn_function_tostring);
        tin_class_bindgetset(state, klass, "name", objfn_function_name, NULL, false);
        state->primfunctionclass = klass;
    }
    tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
    if(klass->super == NULL)
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
    };
}
