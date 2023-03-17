
#include "priv.h"

TinFiber* tin_object_makefiber(TinState* state, TinModule* module, TinFunction* function)
{
    size_t stack_capacity;
    TinValue* stack;
    TinCallFrame* frame;
    TinCallFrame* frames;
    TinFiber* fiber;
    // Allocate in advance, just in case GC is triggered
    stack_capacity = function == NULL ? 1 : (size_t)tin_util_closestpowof2(function->maxslots + 1);
    stack = TIN_ALLOCATE(state, sizeof(TinValue), stack_capacity);
    frames = TIN_ALLOCATE(state, sizeof(TinCallFrame), TIN_INITIAL_CALL_FRAMES);
    fiber = (TinFiber*)tin_gcmem_allocobject(state, sizeof(TinFiber), TINTYPE_FIBER, false);
    if(module != NULL)
    {
        if(module->main_fiber == NULL)
        {
            module->main_fiber = fiber;
        }
    }
    fiber->stack = stack;
    fiber->stack_capacity = stack_capacity;
    fiber->stack_top = fiber->stack;
    fiber->frames = frames;
    fiber->frame_capacity = TIN_INITIAL_CALL_FRAMES;
    fiber->parent = NULL;
    fiber->frame_count = 1;
    fiber->arg_count = 0;
    fiber->module = module;
    fiber->catcher = false;
    fiber->errorval = NULL_VALUE;
    fiber->open_upvalues = NULL;
    fiber->abort = false;
    frame = &fiber->frames[0];
    frame->closure = NULL;
    frame->function = function;
    frame->slots = fiber->stack;
    frame->result_ignored = false;
    frame->return_to_c = false;
    if(function != NULL)
    {
        frame->ip = function->chunk.code;
    }
    return fiber;
}

void tin_ensure_fiber_stack(TinState* state, TinFiber* fiber, size_t needed)
{
    size_t i;
    size_t capacity;
    TinValue* old_stack;
    TinUpvalue* upvalue;
    if(fiber->stack_capacity >= needed)
    {
        return;
    }
    capacity = (size_t)tin_util_closestpowof2((int)needed);
    old_stack = fiber->stack;
    fiber->stack = (TinValue*)tin_gcmem_memrealloc(state, fiber->stack, sizeof(TinValue) * fiber->stack_capacity, sizeof(TinValue) * capacity);
    fiber->stack_capacity = capacity;
    if(fiber->stack != old_stack)
    {
        for(i = 0; i < fiber->frame_capacity; i++)
        {
            TinCallFrame* frame = &fiber->frames[i];
            frame->slots = fiber->stack + (frame->slots - old_stack);
        }
        for(upvalue = fiber->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
        {
            upvalue->location = fiber->stack + (upvalue->location - old_stack);
        }
        fiber->stack_top = fiber->stack + (fiber->stack_top - old_stack);
    }
}

static TinValue objfn_fiber_constructor(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    if(argc < 1 || !tin_value_isfunction(argv[0]))
    {
        tin_vm_raiseexitingerror(vm, "Fiber constructor expects a function as its argument");
    }

    TinFunction* function = tin_value_asfunction(argv[0]);
    TinModule* module = vm->fiber->module;
    TinFiber* fiber = tin_object_makefiber(vm->state, module, function);

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
    (void)instance;
    if(vm->fiber->parent == NULL)
    {
        tin_vm_handleruntimeerror(vm, argc == 0 ? tin_string_copyconst(vm->state, "Fiber was yielded") :
        tin_value_tostring(vm->state, argv[0]));
        return true;
    }

    TinFiber* fiber = vm->fiber;

    vm->fiber = vm->fiber->parent;
    vm->fiber->stack_top -= fiber->arg_count;
    vm->fiber->stack_top[-1] = argc == 0 ? NULL_VALUE : tin_value_fromobject(tin_value_tostring(vm->state, argv[0]));

    argv[-1] = NULL_VALUE;
    return true;
}


static bool objfn_fiber_yeet(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    if(vm->fiber->parent == NULL)
    {
        tin_vm_handleruntimeerror(vm, argc == 0 ? tin_string_copyconst(vm->state, "Fiber was yeeted") :
        tin_value_tostring(vm->state, argv[0]));
        return true;
    }

    TinFiber* fiber = vm->fiber;

    vm->fiber = vm->fiber->parent;
    vm->fiber->stack_top -= fiber->arg_count;
    vm->fiber->stack_top[-1] = argc == 0 ? NULL_VALUE : tin_value_fromobject(tin_value_tostring(vm->state, argv[0]));

    argv[-1] = NULL_VALUE;
    return true;
}


static bool objfn_fiber_abort(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    tin_vm_handleruntimeerror(vm, argc == 0 ? tin_string_copyconst(vm->state, "Fiber was aborted") :
    tin_value_tostring(vm->state, argv[0]));
    argv[-1] = NULL_VALUE;
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
        tin_class_inheritfrom(state, klass, state->primobjectclass);
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
    if(klass->super == NULL)
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
    };
}
