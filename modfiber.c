
#include "priv.h"

TinFiber* tin_object_makefiber(TinState* state, TinModule* module, TinFunction* function)
{
    size_t stackcap;
    TinValue* stack;
    TinCallFrame* frame;
    TinCallFrame* frames;
    TinFiber* fiber;
    // Allocate in advance, just in case GC is triggered
    stackcap = function == NULL ? 1 : (size_t)tin_util_closestpowof2(function->maxslots + 1);
    stack = (TinValue*)tin_gcmem_allocate(state, sizeof(TinValue), stackcap);
    frames = (TinCallFrame*)tin_gcmem_allocate(state, sizeof(TinCallFrame), TIN_INITIAL_CALL_FRAMES);
    fiber = (TinFiber*)tin_object_allocobject(state, sizeof(TinFiber), TINTYPE_FIBER, false);
    if(module != NULL)
    {
        if(module->mainfiber == NULL)
        {
            module->mainfiber = fiber;
        }
    }
    fiber->stackvalues = stack;
    fiber->stackcap = stackcap;
    fiber->stacktop = fiber->stackvalues;
    fiber->framevalues = frames;
    fiber->framecap = TIN_INITIAL_CALL_FRAMES;
    fiber->parent = NULL;
    fiber->framecount = 1;
    fiber->funcargcount = 0;
    fiber->module = module;
    fiber->catcher = false;
    fiber->errorval = tin_value_makenull(state);
    fiber->openupvalues = NULL;
    fiber->abort = false;
    frame = &fiber->framevalues[0];
    frame->closure = NULL;
    frame->function = function;
    frame->slots = fiber->stackvalues;
    frame->ignresult = false;
    frame->returntonative = false;
    if(function != NULL)
    {
        frame->ip = function->chunk.code;
    }
    return fiber;
}

void tin_fiber_ensurestack(TinState* state, TinFiber* fiber, size_t needed)
{
    size_t i;
    size_t capacity;
    TinValue* old_stack;
    TinUpvalue* upvalue;
    TinCallFrame* frame;
    if(fiber->stackcap >= needed)
    {
        return;
    }
    capacity = (size_t)tin_util_closestpowof2((int)needed);
    old_stack = fiber->stackvalues;
    fiber->stackvalues = (TinValue*)tin_gcmem_memrealloc(state, fiber->stackvalues, sizeof(TinValue) * fiber->stackcap, sizeof(TinValue) * capacity);
    fiber->stackcap = capacity;
    if(fiber->stackvalues != old_stack)
    {
        for(i = 0; i < fiber->framecap; i++)
        {
            frame = &fiber->framevalues[i];
            frame->slots = fiber->stackvalues + (frame->slots - old_stack);
        }
        for(upvalue = fiber->openupvalues; upvalue != NULL; upvalue = upvalue->next)
        {
            upvalue->location = fiber->stackvalues + (upvalue->location - old_stack);
        }
        fiber->stacktop = fiber->stackvalues + (fiber->stacktop - old_stack);
    }
}

static TinValue objfn_fiber_constructor(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinFiber* fiber;
    TinModule* module;
    TinFunction* function;
    (void)instance;
    if(argc < 1 || !tin_value_isfunction(argv[0]))
    {
        tin_vm_raiseexitingerror(vm, "Fiber constructor expects a function as its argument");
    }
    function = tin_value_asfunction(argv[0]);
    module = vm->fiber->module;
    fiber = tin_object_makefiber(vm->state, module, function);
    fiber->parent = vm->fiber;
    return tin_value_fromobject(fiber);
}

static TinValue objfn_fiber_done(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return tin_value_makebool(vm->state, util_is_fiber_done(tin_value_asfiber(instance)));
}

static TinValue objfn_fiber_error(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return tin_value_asfiber(instance)->errorval;
}

static TinValue objfn_fiber_current(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return tin_value_fromobject(vm->fiber);
}

static bool objfn_fiber_run(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    util_run_fiber(vm, tin_value_asfiber(instance), argv, argc, false);
    return true;
}

static bool objfn_fiber_try(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    util_run_fiber(vm, tin_value_asfiber(instance), argv, argc, true);
    return true;
}

static bool objfn_fiber_yield(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinFiber* fiber;
    (void)instance;
    if(vm->fiber->parent == NULL)
    {
        tin_vm_handleruntimeerror(vm, argc == 0 ? tin_string_copyconst(vm->state, "Fiber was yielded") :
        tin_value_tostring(vm->state, argv[0]));
        return true;
    }
    fiber = vm->fiber;
    vm->fiber = vm->fiber->parent;
    vm->fiber->stacktop -= fiber->funcargcount;
    vm->fiber->stacktop[-1] = argc == 0 ? tin_value_makenull(vm->state) : tin_value_fromobject(tin_value_tostring(vm->state, argv[0]));
    argv[-1] = tin_value_makenull(vm->state);
    return true;
}


static bool objfn_fiber_yeet(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinFiber* fiber;
    (void)instance;
    if(vm->fiber->parent == NULL)
    {
        tin_vm_handleruntimeerror(vm, argc == 0 ? tin_string_copyconst(vm->state, "Fiber was yeeted") :
        tin_value_tostring(vm->state, argv[0]));
        return true;
    }
    fiber = vm->fiber;
    vm->fiber = vm->fiber->parent;
    vm->fiber->stacktop -= fiber->funcargcount;
    vm->fiber->stacktop[-1] = argc == 0 ? tin_value_makenull(vm->state) : tin_value_fromobject(tin_value_tostring(vm->state, argv[0]));
    argv[-1] = tin_value_makenull(vm->state);
    return true;
}

static bool objfn_fiber_abort(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    tin_vm_handleruntimeerror(vm, argc == 0 ? tin_string_copyconst(vm->state, "Fiber was aborted") :
    tin_value_tostring(vm->state, argv[0]));
    argv[-1] = tin_value_makenull(vm->state);
    return true;
}

static TinValue objfn_fiber_tostring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    return tin_string_format(vm->state, "fiber@%p", &instance);

}

void tin_open_fiber_library(TinState* state)
{
    TinClass* klass;
    klass = tin_object_makeclassname(state, "Fiber");
    {
        tin_class_bindconstructor(state, klass, objfn_fiber_constructor);
        tin_class_bindmethod(state, klass, "toString", objfn_fiber_tostring);
        tin_class_bindprimitive(state, klass, "run", objfn_fiber_run);
        tin_class_bindprimitive(state, klass, "try", objfn_fiber_try);
        tin_class_bindgetset(state, klass, "done", objfn_fiber_done, NULL, false);
        tin_class_bindgetset(state, klass, "error", objfn_fiber_error, NULL, false);
        tin_class_bindstaticprimitive(state, klass, "yield", objfn_fiber_yield);
        tin_class_bindstaticprimitive(state, klass, "yeet", objfn_fiber_yeet);
        tin_class_bindstaticprimitive(state, klass, "abort", objfn_fiber_abort);
        tin_class_bindgetset(state, klass, "current", objfn_fiber_current, NULL, true);
        state->primfiberclass = klass;
    }
    tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
}
