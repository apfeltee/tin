
#include "priv.h"

TinRange* tin_object_makerange(TinState* state, double from, double to)
{
    TinRange* range;
    range = (TinRange*)tin_gcmem_allocobject(state, sizeof(TinRange), TINTYPE_RANGE, false);
    range->from = from;
    range->to = to;
    return range;
}

static TinValue objfn_range_iterator(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int number;
    TinRange* range;
    (void)vm;
    (void)argc;
    TIN_ENSURE_ARGS(vm->state, 1);
    range = tin_value_asrange(instance);
    number = range->from;
    if(tin_value_isnumber(argv[0]))
    {
        number = tin_value_asnumber(argv[0]);
        if((range->to > range->from) ? (number >= range->to) : (number >= range->from))
        {
            return tin_value_makenull(vm->state);
        }
        number += (((range->from - range->to) > 0) ? -1 : 1);
    }
    return tin_value_makefloatnumber(vm->state, number);
}

static TinValue objfn_range_iteratorvalue(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TIN_ENSURE_ARGS(vm->state, 1);
    (void)vm;
    (void)instance;
    return argv[0];
}

static TinValue objfn_range_tostring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinRange* range;
    (void)argc;
    (void)argv;
    range = tin_value_asrange(instance);
    return tin_string_format(vm->state, "Range(#, #)", range->from, range->to);
}

static TinValue objfn_range_from(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argv;
    (void)argc;
    return tin_value_makefloatnumber(vm->state, tin_value_asrange(instance)->from);
}

static TinValue objfn_range_set_from(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    tin_value_asrange(instance)->from = tin_value_asnumber(argv[0]);
    return argv[0];
}

static TinValue objfn_range_to(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return tin_value_makefloatnumber(vm->state, tin_value_asrange(instance)->to);
}

static TinValue objfn_range_set_to(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    tin_value_asrange(instance)->to = tin_value_asnumber(argv[0]);
    return argv[0];
}

static TinValue objfn_range_length(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinRange* range;
    (void)vm;
    (void)argc;
    (void)argv;
    range = tin_value_asrange(instance);
    return tin_value_makefloatnumber(vm->state, range->to - range->from);
}

void tin_open_range_library(TinState* state)
{
    TinClass* klass;
    klass = tin_object_makeclassname(state, "Range");
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
        tin_class_bindconstructor(state, klass, util_invalid_constructor);
        tin_class_bindmethod(state, klass, "iterator", objfn_range_iterator);
        tin_class_bindmethod(state, klass, "iteratorValue", objfn_range_iteratorvalue);
        tin_class_bindmethod(state, klass, "toString", objfn_range_tostring);
        tin_class_bindgetset(state, klass, "from", objfn_range_from, objfn_range_set_from, false);
        tin_class_bindgetset(state, klass, "to", objfn_range_to, objfn_range_set_to, false);
        tin_class_bindgetset(state, klass, "length", objfn_range_length, NULL, false);
        state->primrangeclass = klass;
    }
    tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
    if(klass->super == NULL)
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
    };
}

