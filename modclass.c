
#include "priv.h"

TinClass* tin_object_makeclasswithparent(TinState* state, TinString* name, TinClass* parentclass)
{
    TinClass* klass;
    klass = (TinClass*)tin_object_allocobject(state, sizeof(TinClass), TINTYPE_CLASS, false);
    klass->name = name;
    klass->initmethod = NULL;
    klass->parentclass = parentclass;
    tin_table_init(state, &klass->methods);
    tin_table_init(state, &klass->staticfields);
    if(parentclass != NULL)
    {
        tin_class_inheritfrom(state, klass, parentclass);
    }
    return klass;
}

TinClass* tin_object_makeclass(TinState* state, TinString* name)
{
    return tin_object_makeclasswithparent(state, name, state->primobjectclass);
}

TinClass* tin_object_makeclassnamewithparent(TinState* state, const char* name, TinClass* parentklass)
{
    TinString* nm;
    TinClass* cl;
    nm = tin_string_copy(state, name, strlen(name));
    cl = tin_object_makeclasswithparent(state, nm, parentklass);
    cl->name = nm;
    return cl;
}

TinClass* tin_object_makeclassname(TinState* state, const char* name)
{
    return tin_object_makeclassnamewithparent(state, name, NULL);
}

TinField* tin_object_makefield(TinState* state, TinObject* getter, TinObject* setter)
{
    TinField* field;
    field = (TinField*)tin_object_allocobject(state, sizeof(TinField), TINTYPE_FIELD, false);
    field->getter = getter;
    field->setter = setter;
    return field;
}

TinInstance* tin_object_makeinstance(TinState* state, TinClass* klass)
{
    TinInstance* inst;
    inst = (TinInstance*)tin_object_allocobject(state, sizeof(TinInstance), TINTYPE_INSTANCE, false);
    inst->klass = klass;
    tin_table_init(state, &inst->fields);
    //inst->fields.count = 0;
    return inst;
}

void tin_class_bindconstructor(TinState* state, TinClass* cl, TinNativeMethodFn fn)
{
    TinNativeMethod* mth;
    mth = tin_class_bindmethod(state, cl, TIN_VALUE_CTORNAME, fn);
    cl->initmethod = (TinObject*)mth;
}

TinNativeMethod* tin_class_bindmethod(TinState* state, TinClass* cl, const char* name, TinNativeMethodFn fn)
{
    TinString* nm;
    TinNativeMethod* mth;
    nm = tin_string_copy(state, name, strlen(name));
    mth = tin_object_makenativemethod(state, fn, nm);
    tin_table_set(state, &cl->methods, nm, tin_value_fromobject(mth));
    return mth;
}

TinPrimitiveMethod* tin_class_bindprimitive(TinState* state, TinClass* cl, const char* name, TinPrimitiveMethodFn fn)
{
    TinString* nm;
    TinPrimitiveMethod* mth;
    nm = tin_string_copy(state, name, strlen(name));
    mth = tin_object_makeprimitivemethod(state, fn, nm);
    tin_table_set(state, &cl->methods, nm, tin_value_fromobject(mth));
    return mth;
}

TinNativeMethod* tin_class_bindstaticmethod(TinState* state, TinClass* cl, const char* name, TinNativeMethodFn fn)
{
    TinString* nm;
    TinNativeMethod* mth;
    nm = tin_string_copy(state, name, strlen(name));
    mth = tin_object_makenativemethod(state, fn, nm);
    tin_table_set(state, &cl->staticfields, nm, tin_value_fromobject(mth));
    return mth;
}

TinPrimitiveMethod* tin_class_bindstaticprimitive(TinState* state, TinClass* cl, const char* name, TinPrimitiveMethodFn fn)
{
    TinString* nm;
    TinPrimitiveMethod* mth;
    nm = tin_string_copy(state, name, strlen(name));
    mth = tin_object_makeprimitivemethod(state, fn, nm);
    tin_table_set(state, &cl->staticfields, nm, tin_value_fromobject(mth));
    return mth;
}

void tin_class_setstaticfield(TinState* state, TinClass* cl, const char* name, TinValue val)
{
    TinString* nm;
    nm = tin_string_copy(state, name, strlen(name));
    tin_table_set(state, &cl->staticfields, nm, val);
}

TinField* tin_class_bindgetset(TinState* state, TinClass* cl, const char* name, TinNativeMethodFn getfn, TinNativeMethodFn setfn, bool isstatic)
{
    TinTable* tbl;
    TinField* field;
    TinString* nm;
    TinNativeMethod* mthset;
    TinNativeMethod* mthget;
    tbl = &cl->methods;
    mthset = NULL;
    mthget = NULL;

    nm = tin_string_copy(state, name, strlen(name));
    if(getfn != NULL)
    {
        mthget = tin_object_makenativemethod(state, getfn, nm);
    }
    if(setfn != NULL)
    {
        mthset = tin_object_makenativemethod(state, setfn, nm);
    }
    if(isstatic)
    {
        tbl = &cl->staticfields;
    }
    field = tin_object_makefield(state, (TinObject*)mthget, (TinObject*)mthset);
    tin_table_set(state, tbl, nm, tin_value_fromobject(field)); 
    return field;
}


/*

    #define TIN_INHERIT_CLASS(superklass)                                \
        klass->parentclass = (TinClass*)superklass;                            \
        if(klass->initmethod == NULL)                                    \
        {                                                                 \
            klass->initmethod = superklass->initmethod;                \
        }                                                                 \
        tin_table_add_all(state, &superklass->methods, &klass->methods); \
        tin_table_add_all(state, &superklass->staticfields, &klass->staticfields);
*/

void tin_class_inheritfrom(TinState* state, TinClass* current, TinClass* other)
{
    current->parentclass = (TinClass*)other;
    if(current->initmethod == NULL)
    {
        current->initmethod = other->initmethod;
    }
    tin_table_add_all(state, &other->methods, &current->methods); \
    tin_table_add_all(state, &other->staticfields, &current->staticfields);
}

static TinValue objfn_class_tostring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinWriter wr;
    (void)argc;
    (void)argv;
    tin_writer_init_string(vm->state, &wr);
    tin_towriter_value(vm->state, &wr, instance, true);
    return tin_value_fromobject(tin_writer_get_string(&wr));
}

static TinValue objfn_class_iterator(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    bool fields;
    int value;
    int index;
    int mthcap;
    TinClass* klass;
    (void)argc;
    if(!tin_args_ensure(vm->state, argc, 1))
    {
        return tin_value_makenull(vm->state);
    }
    klass = tin_value_asclass(instance);
    index = tin_value_isnull(argv[0]) ? -1 : tin_value_asnumber(argv[0]);
    mthcap = (int)tin_table_getcapacity(&klass->methods);
    fields = index >= mthcap;
    value = util_table_iterator(fields ? &klass->staticfields : &klass->methods, fields ? index - mthcap : index);
    if(value == -1)
    {
        if(fields)
        {
            return tin_value_makenull(vm->state);
        }
        index++;
        fields = true;
        value = util_table_iterator(&klass->staticfields, index - mthcap);
    }
    if(value == -1)
    {
        return tin_value_makenull(vm->state);
    }
    return tin_value_makefixednumber(vm->state, fields ? value + mthcap : value);
}


static TinValue objfn_class_iteratorvalue(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    bool fields;
    size_t index;
    size_t mthcap;
    TinClass* klass;
    index = tin_args_checknumber(vm, argv, argc, 0);
    klass = tin_value_asclass(instance);
    mthcap = tin_table_getcapacity(&klass->methods);
    fields = index >= mthcap;
    return util_table_iterator_key(fields ? &klass->staticfields : &klass->methods, fields ? index - mthcap : index);
}


static TinValue objfn_class_super(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinClass* super;
    (void)vm;
    (void)argc;
    (void)argv;
    super = NULL;
    if(tin_value_isinstance(instance))
    {
        super = tin_value_asinstance(instance)->klass->parentclass;
    }
    else
    {
        super = tin_value_asclass(instance)->parentclass;
    }
    if(super == NULL)
    {
        return tin_value_makenull(vm->state);
    }
    return tin_value_fromobject(super);
}

static TinValue objfn_class_subscript(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinClass* klass;    
    TinValue value;
    (void)argc;
    klass = tin_value_asclass(instance);
    if(argc == 2)
    {
        if(!tin_value_isstring(argv[0]))
        {
            tin_vm_raiseexitingerror(vm, "class index must be a string");
        }

        tin_table_set(vm->state, &klass->staticfields, tin_value_asstring(argv[0]), argv[1]);
        return argv[1];
    }
    if(!tin_value_isstring(argv[0]))
    {
        tin_vm_raiseexitingerror(vm, "class index must be a string");
    }
    if(tin_table_get(&klass->staticfields, tin_value_asstring(argv[0]), &value))
    {
        return value;
    }
    if(tin_table_get(&klass->methods, tin_value_asstring(argv[0]), &value))
    {
        return value;
    }
    return tin_value_makenull(vm->state);
}

static TinValue objfn_class_compare(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinClass* selfclass;
    TinClass* otherclass;
    (void)vm;
    (void)argc;
    if(tin_value_isclass(argv[0]))
    {
        selfclass = tin_value_asclass(instance);
        otherclass = tin_value_asclass(argv[0]);
        if(tin_string_equal(vm->state, selfclass->name, otherclass->name))
        {
            if(selfclass == otherclass)
            {
                return tin_value_makebool(vm->state, true);
            }
        }
    }
    return tin_value_makebool(vm->state, false);
}

static TinValue objfn_class_name(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return tin_value_fromobject(tin_value_asclass(instance)->name);
}

void tin_open_class_library(TinState* state)
{
    TinClass* klass;
    klass = tin_object_makeclassname(state, "Class");
    {
        tin_class_bindmethod(state, klass, "[]", objfn_class_subscript);
        tin_class_bindmethod(state, klass, "==", objfn_class_compare);
        tin_class_bindmethod(state, klass, "toString", objfn_class_tostring);
        tin_class_bindstaticmethod(state, klass, "toString", objfn_class_tostring);
        tin_class_bindstaticmethod(state, klass, "iterator", objfn_class_iterator);
        tin_class_bindstaticmethod(state, klass, "iteratorValue", objfn_class_iteratorvalue);
        tin_class_bindgetset(state, klass, "super", objfn_class_super, NULL, false);
        tin_class_bindgetset(state, klass, "super", objfn_class_super, NULL, true);
        tin_class_bindgetset(state, klass, "name", objfn_class_name, NULL, true);
        state->primclassclass = klass;
    }
    tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
}


