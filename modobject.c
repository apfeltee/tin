
#include <memory.h>
#include <math.h>
#include "priv.h"
#include "sds.h"

TinUpvalue* tin_object_makeupvalue(TinState* state, TinValue* slot)
{
    TinUpvalue* upvalue;
    upvalue = (TinUpvalue*)tin_gcmem_allocobject(state, sizeof(TinUpvalue), TINTYPE_UPVALUE, false);
    upvalue->location = slot;
    upvalue->closed = NULL_VALUE;
    upvalue->next = NULL;
    return upvalue;
}

TinModule* tin_object_makemodule(TinState* state, TinString* name)
{
    TinModule* module;
    module = (TinModule*)tin_gcmem_allocobject(state, sizeof(TinModule), TINTYPE_MODULE, false);
    module->name = name;
    module->return_value = NULL_VALUE;
    module->main_function = NULL;
    module->privates = NULL;
    module->ran = false;
    module->main_fiber = NULL;
    module->private_count = 0;
    module->private_names = tin_object_makemap(state);
    return module;
}

TinUserdata* tin_object_makeuserdata(TinState* state, size_t size, bool ispointeronly)
{
    TinUserdata* userdata;
    userdata = (TinUserdata*)tin_gcmem_allocobject(state, sizeof(TinUserdata), TINTYPE_USERDATA, false);
    userdata->data = NULL;
    if(size > 0)
    {
        if(!ispointeronly)
        {
            userdata->data = tin_gcmem_memrealloc(state, NULL, 0, size);
        }
    }
    userdata->size = size;
    userdata->cleanup_fn = NULL;
    userdata->canfree = true;
    return userdata;
}

TinRange* tin_object_makerange(TinState* state, double from, double to)
{
    TinRange* range;
    range = (TinRange*)tin_gcmem_allocobject(state, sizeof(TinRange), TINTYPE_RANGE, false);
    range->from = from;
    range->to = to;
    return range;
}

TinReference* tin_object_makereference(TinState* state, TinValue* slot)
{
    TinReference* reference;
    reference = (TinReference*)tin_gcmem_allocobject(state, sizeof(TinReference), TINTYPE_REFERENCE, false);
    reference->slot = slot;
    return reference;
}

void tin_object_destroy(TinState* state, TinObject* object)
{
    TinString* string;
    TinFunction* function;
    TinFiber* fiber;
    TinModule* module;
    TinClosure* closure;
    if(!object->mustfree)
    {
        return;
    }
#ifdef TIN_LOG_ALLOCATION
    printf("(");
    tin_towriter_value(tin_value_fromobject(object));
    printf(") %p free %s\n", (void*)object, tin_tostring_typename(object->type));
#endif

    switch(object->type)
    {
        case TINTYPE_NUMBER:
            {
                if(object->mustfree)
                {
                    TIN_FREE(state, sizeof(TinNumber), object);
                }
            }
            break;
        case TINTYPE_STRING:
            {
                string = (TinString*)object;
                //TIN_FREE_ARRAY(state, sizeof(char), string->chars, string->length + 1);
                sds_destroy(string->chars);
                string->chars = NULL;
                TIN_FREE(state, sizeof(TinString), object);
            }
            break;

        case TINTYPE_FUNCTION:
            {
                function = (TinFunction*)object;
                tin_chunk_destroy(state, &function->chunk);
                TIN_FREE(state, sizeof(TinFunction), object);
            }
            break;
        case TINTYPE_NATIVEFUNCTION:
            {
                TIN_FREE(state, sizeof(TinNativeFunction), object);
            }
            break;
        case TINTYPE_NATIVEPRIMITIVE:
            {
                TIN_FREE(state, sizeof(TinNativePrimFunction), object);
            }
            break;
        case TINTYPE_NATIVEMETHOD:
            {
                TIN_FREE(state, sizeof(TinNativeMethod), object);
            }
            break;
        case TINTYPE_PRIMITIVEMETHOD:
            {
                TIN_FREE(state, sizeof(TinPrimitiveMethod), object);
            }
            break;
        case TINTYPE_FIBER:
            {
                fiber = (TinFiber*)object;
                TIN_FREE_ARRAY(state, sizeof(TinCallFrame), fiber->frames, fiber->frame_capacity);
                TIN_FREE_ARRAY(state, sizeof(TinValue), fiber->stack, fiber->stack_capacity);
                TIN_FREE(state, sizeof(TinFiber), object);
            }
            break;
        case TINTYPE_MODULE:
            {
                module = (TinModule*)object;
                TIN_FREE_ARRAY(state, sizeof(TinValue), module->privates, module->private_count);
                TIN_FREE(state, sizeof(TinModule), object);
            }
            break;
        case TINTYPE_CLOSURE:
            {
                closure = (TinClosure*)object;
                TIN_FREE_ARRAY(state, sizeof(TinUpvalue*), closure->upvalues, closure->upvalue_count);
                TIN_FREE(state, sizeof(TinClosure), object);
            }
            break;
        case TINTYPE_UPVALUE:
            {
                TIN_FREE(state, sizeof(TinUpvalue), object);
            }
            break;
        case TINTYPE_CLASS:
            {
                TinClass* klass = (TinClass*)object;
                tin_table_destroy(state, &klass->methods);
                tin_table_destroy(state, &klass->static_fields);
                TIN_FREE(state, sizeof(TinClass), object);
            }
            break;

        case TINTYPE_INSTANCE:
            {
                tin_table_destroy(state, &((TinInstance*)object)->fields);
                TIN_FREE(state, sizeof(TinInstance), object);
            }
            break;
        case TINTYPE_BOUNDMETHOD:
            {
                TIN_FREE(state, sizeof(TinBoundMethod), object);
            }
            break;
        case TINTYPE_ARRAY:
            {
                tin_vallist_destroy(state, &((TinArray*)object)->list);
                TIN_FREE(state, sizeof(TinArray), object);
            }
            break;
        case TINTYPE_MAP:
            {
                tin_table_destroy(state, &((TinMap*)object)->values);
                TIN_FREE(state, sizeof(TinMap), object);
            }
            break;
        case TINTYPE_USERDATA:
            {
                TinUserdata* data = (TinUserdata*)object;
                if(data->cleanup_fn != NULL)
                {
                    data->cleanup_fn(state, data, false);
                }
                if(data->size > 0)
                {
                    if(data->canfree)
                    {
                        tin_gcmem_memrealloc(state, data->data, data->size, 0);
                    }
                }
                TIN_FREE(state, sizeof(TinUserdata), data);
                //free(data);
            }
            break;
        case TINTYPE_RANGE:
            {
                TIN_FREE(state, sizeof(TinRange), object);
            }
            break;
        case TINTYPE_FIELD:
            {
                TIN_FREE(state, sizeof(TinField), object);
            }
            break;
        case TINTYPE_REFERENCE:
            {
                TIN_FREE(state, sizeof(TinReference), object);
            }
            break;
        default:
            {
                fprintf(stderr, "internal error: trying to free something else!\n");
                UNREACHABLE
            }
            break;
    }
}

void tin_object_destroylistof(TinState* state, TinObject* objects)
{
    TinObject* obj;
    TinObject* next;
    obj = objects;
    while(obj != NULL)
    {
        next = obj->next;
        tin_object_destroy(state, obj);
        obj = next;
    }
    free(state->vm->gcgraystack);
    state->vm->gcgraycapacity = 0;
}

TinValue tin_function_getname(TinVM* vm, TinValue instance)
{
    TinString* name;
    TinField* field;
    name = NULL;
    switch(tin_value_type(instance))
    {
        case TINTYPE_FUNCTION:
            {
                name = tin_value_asfunction(instance)->name;
            }
            break;
        case TINTYPE_CLOSURE:
            {
                name = tin_value_asclosure(instance)->function->name;
            }
            break;
        case TINTYPE_FIELD:
            {
                field = tin_value_asfield(instance);
                if(field->getter != NULL)
                {
                    return tin_function_getname(vm, tin_value_fromobject(field->getter));
                }
                return tin_function_getname(vm, tin_value_fromobject(field->setter));
            }
            break;
        case TINTYPE_NATIVEPRIMITIVE:
            {
                name = tin_value_asnativeprimitive(instance)->name;
            }
            break;
        case TINTYPE_NATIVEFUNCTION:
            {
                name = tin_value_asnativefunction(instance)->name;
            }
            break;
        case TINTYPE_NATIVEMETHOD:
            {
                name = tin_value_asnativemethod(instance)->name;
            }
            break;
        case TINTYPE_PRIMITIVEMETHOD:
            {
                name = tin_value_asprimitivemethod(instance)->name;
            }
            break;
        case TINTYPE_BOUNDMETHOD:
            {
                return tin_function_getname(vm, tin_value_asboundmethod(instance)->method);
            }
            break;
        default:
            {
                //return NULL_VALUE;
            }
            break;
    }
    if(name == NULL)
    {
        if(tin_value_isobject(instance))
        {
            return tin_string_format(vm->state, "function #", *((double*)tin_value_asobject(instance)));
        }
    }
    return tin_string_format(vm->state, "function @", tin_value_fromobject(name));
}

static TinValue objfn_instance_class(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    return tin_value_fromobject(tin_state_getclassfor(vm->state, instance));
}

static TinValue objfn_instance_super(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    TinClass* cl;
    cl = tin_state_getclassfor(vm->state, instance)->super;
    if(cl == NULL)
    {
        return NULL_VALUE;
    }
    return tin_value_fromobject(cl);
}

static TinValue objfn_instance_tostring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{

    TinWriter wr;
    (void)argc;
    (void)argv;
    #if 0
        return tin_string_format(vm->state, "@ instance", tin_value_fromobject(tin_state_getclassfor(vm->state, instance)->name));
    #else
        tin_writer_init_string(vm->state, &wr);
        tin_towriter_value(vm->state, &wr, instance, true);
        return tin_value_fromobject(tin_writer_get_string(&wr));
    #endif
}

static void fillmap(TinState* state, TinMap* destmap, TinTable* fromtbl, bool includenullkeys)
{
    size_t i;
    TinString* key;
    TinValue val;
    (void)includenullkeys;
    for(i=0; i<(size_t)(fromtbl->count); i++)
    {
        key = fromtbl->entries[i].key;
        if(key != NULL)
        {
            val = fromtbl->entries[i].value;
            tin_map_set(state, destmap, key, val);
        }
    }
}

static TinValue objfn_instance_tomap(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    TinMap* map;
    TinMap* minst;
    TinMap* mclass;
    TinMap* mclstatics;
    TinMap* mclmethods;
    TinInstance* inst;
    mclass = NULL;
    if(!tin_value_isinstance(instance))
    {
        tin_vm_raiseexitingerror(vm, "toMap() can only be used on instances");
    }
    inst = tin_value_asinstance(instance);
    map = tin_object_makemap(vm->state);
    {
        minst = tin_object_makemap(vm->state);
        fillmap(vm->state, minst, &(inst->fields), true);
    }
    {
        mclass = tin_object_makemap(vm->state);
        {
            mclstatics = tin_object_makemap(vm->state);
            fillmap(vm->state, mclstatics, &(inst->klass->static_fields), false);
        }
        {
            mclmethods = tin_object_makemap(vm->state);
            fillmap(vm->state, mclmethods, &(inst->klass->methods), false);
        }
        tin_map_set(vm->state, mclass, tin_string_copyconst(vm->state, "statics"), tin_value_fromobject(mclstatics));
        tin_map_set(vm->state, mclass, tin_string_copyconst(vm->state, "methods"), tin_value_fromobject(mclmethods));
    }
    tin_map_set(vm->state, map, tin_string_copyconst(vm->state, "instance"), tin_value_fromobject(minst));
    tin_map_set(vm->state, map, tin_string_copyconst(vm->state, "class"), tin_value_fromobject(mclass));
    return tin_value_fromobject(map);
}

static TinValue objfn_instance_subscript(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    TinValue value;
    TinInstance* inst;
    if(!tin_value_isinstance(instance))
    {
        tin_vm_raiseexitingerror(vm, "cannot modify built-in types");
    }
    inst = tin_value_asinstance(instance);
    if(argc == 2)
    {
        if(!tin_value_isstring(argv[0]))
        {
            tin_vm_raiseexitingerror(vm, "object index must be a string");
        }

        tin_table_set(vm->state, &inst->fields, tin_value_asstring(argv[0]), argv[1]);
        return argv[1];
    }
    if(!tin_value_isstring(argv[0]))
    {
        tin_vm_raiseexitingerror(vm, "object index must be a string");
    }
    if(tin_table_get(&inst->fields, tin_value_asstring(argv[0]), &value))
    {
        return value;
    }
    if(tin_table_get(&inst->klass->static_fields, tin_value_asstring(argv[0]), &value))
    {
        return value;
    }
    if(tin_table_get(&inst->klass->methods, tin_value_asstring(argv[0]), &value))
    {
        return value;
    }
    return NULL_VALUE;
}

static TinValue objfn_instance_iterator(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    int value;
    int index;
    TinInstance* self;
    TIN_ENSURE_ARGS(vm->state, 1);
    self = tin_value_asinstance(instance);
    index = tin_value_isnull(argv[0]) ? -1 : tin_value_asnumber(argv[0]);
    value = util_table_iterator(&self->fields, index);
    if(value == -1)
    {
        return NULL_VALUE;
    }
    return tin_value_makefixednumber(vm->state, value);
}


static TinValue objfn_instance_iteratorvalue(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t index;
    TinInstance* self;
    index = tin_value_checknumber(vm, argv, argc, 0);
    self = tin_value_asinstance(instance);
    return util_table_iterator_key(&self->fields, index);
}

void tin_state_openobjectlibrary(TinState* state)
{
    TinClass* klass;
    klass = tin_object_makeclassname(state, "Object");
    {
        tin_class_inheritfrom(state, klass, state->primclassclass);
        tin_class_bindgetset(state, klass, "class", objfn_instance_class, NULL, false);
        tin_class_bindgetset(state, klass, "super", objfn_instance_super, NULL, false);
        tin_class_bindmethod(state, klass, "[]", objfn_instance_subscript);
        #if 0
        tin_class_bindmethod(state, klass, "hasMethod", objfn_instance_hasmethod);
        #endif
        tin_class_bindmethod(state, klass, "toString", objfn_instance_tostring);
        tin_class_bindmethod(state, klass, "toMap", objfn_instance_tomap);
        tin_class_bindmethod(state, klass, "iterator", objfn_instance_iterator);
        tin_class_bindmethod(state, klass, "iteratorValue", objfn_instance_iteratorvalue);
        state->primobjectclass = klass;
        state->primobjectclass->super = state->primclassclass;
    }
    tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
    if(klass->super == NULL)
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
    };
}

