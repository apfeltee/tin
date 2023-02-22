
#include "priv.h"

static TinValue access_private(TinVM* vm, TinMap* map, TinString* name, TinValue* val)
{
    int index;
    TinValue value;
    TinString* id;
    TinModule* module;
    id = tin_string_copyconst(vm->state, "_module");
    if(!tin_table_get(&map->values, id, &value) || !tin_value_ismodule(value))
    {
        return NULL_VALUE;
    }
    module = tin_value_asmodule(value);

    if(id == name)
    {
        return tin_value_fromobject(module);
    }

    if(tin_table_get(&module->private_names->values, name, &value))
    {
        index = (int)tin_value_asnumber(value);
        if(index > -1 && index < (int)module->private_count)
        {
            if(val != NULL)
            {
                module->privates[index] = *val;
                return *val;
            }
            return module->privates[index];
        }
    }
    return NULL_VALUE;
}


static TinValue objfn_module_privates(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinModule* module;
    TinMap* map;
    (void)argc;
    (void)argv;
    module = tin_value_ismodule(instance) ? tin_value_asmodule(instance) : vm->fiber->module;
    map = module->private_names;
    if(map->index_fn == NULL)
    {
        map->index_fn = access_private;
        tin_table_set(vm->state, &map->values, tin_string_copyconst(vm->state, "_module"), tin_value_fromobject(module));
    }
    return tin_value_fromobject(map);
}

static TinValue objfn_module_current(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return tin_value_fromobject(vm->fiber->module);
}

static TinValue objfn_module_tostring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    return tin_string_format(vm->state, "Module @", tin_value_fromobject(tin_value_asmodule(instance)->name));
}

static TinValue objfn_module_name(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return tin_value_fromobject(tin_value_asmodule(instance)->name);
}

void tin_open_module_library(TinState* state)
{
    TinClass* klass;
    klass = tin_create_classobject(state, "Module");
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
        tin_class_bindconstructor(state, klass, util_invalid_constructor);
        tin_class_setstaticfield(state, klass, "loaded", tin_value_fromobject(state->vm->modules));
        tin_class_bindgetset(state, klass, "privates", objfn_module_privates, NULL, true);
        tin_class_bindgetset(state, klass, "current", objfn_module_current, NULL, true);
        tin_class_bindmethod(state, klass, "toString", objfn_module_tostring);
        tin_class_bindgetset(state, klass, "name", objfn_module_name, NULL, false);
        tin_class_bindgetset(state, klass, "privates", objfn_module_privates, NULL, false);
        state->primmoduleclass = klass;
    }
    tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
    if(klass->super == NULL)
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
    };
}

