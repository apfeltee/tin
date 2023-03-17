
#include "priv.h"
#include "sds.h"

void tin_datalist_init(TinDataList* dl, size_t typsz)
{
    dl->values = NULL;
    dl->capacity = 0;
    dl->count = 0;
    dl->rawelemsz = typsz;
    dl->elemsz = dl->rawelemsz + sizeof(intptr_t);
}

void tin_datalist_destroy(TinState* state, TinDataList* dl)
{
    TIN_FREE_ARRAY(state, dl->elemsz, dl->values, dl->capacity);
    tin_datalist_init(dl, dl->rawelemsz);
}

size_t tin_datalist_count(TinDataList* dl)
{
    return dl->count;
}

size_t tin_datalist_size(TinDataList* dl)
{
    return dl->count;
}

size_t tin_datalist_capacity(TinDataList* dl)
{
    return dl->capacity;
}

void tin_datalist_clear(TinDataList* dl)
{
    dl->count = 0;
}

void tin_datalist_setcount(TinDataList* dl, size_t nc)
{
    dl->count = nc;
}

void tin_datalist_deccount(TinDataList* dl)
{
    dl->count--;
}

intptr_t tin_datalist_get(TinDataList* dl, size_t idx)
{
    return dl->values[idx];
}

intptr_t tin_datalist_set(TinDataList* dl, size_t idx, intptr_t val)
{
    dl->values[idx] = val;
    return val;
}

void tin_datalist_push(TinState* state, TinDataList* dl, intptr_t value)
{
    size_t oldcapacity;
    if(dl->capacity < (dl->count + 1))
    {
        oldcapacity = dl->capacity;
        dl->capacity = TIN_GROW_CAPACITY(oldcapacity);
        dl->values = TIN_GROW_ARRAY(state, dl->values, dl->elemsz, oldcapacity, dl->capacity);
    }
    dl->values[dl->count] = value;
    dl->count++;
}

void tin_datalist_ensuresize(TinState* state, TinDataList* dl, size_t size)
{
    size_t i;
    size_t oldcapacity;
    if(dl->capacity < size)
    {
        oldcapacity = dl->capacity;
        dl->capacity = size;
        dl->values = TIN_GROW_ARRAY(state, dl->values, dl->elemsz, oldcapacity, size);
        for(i = oldcapacity; i < size; i++)
        {
            dl->values[i] = 0;
        }
    }
    if(dl->count < size)
    {
        dl->count = size;
    }
}

/* -------------------------*/

void tin_vallist_init(TinValList* vl)
{
    vl->values = NULL;
    vl->capacity = 0;
    vl->count = 0;
}

void tin_vallist_destroy(TinState* state, TinValList* vl)
{
    TIN_FREE_ARRAY(state, sizeof(TinValue), vl->values, vl->capacity);
    tin_vallist_init(vl);
}

size_t tin_vallist_size(TinValList* vl)
{
    return vl->count;
}

size_t tin_vallist_count(TinValList* vl)
{
    return vl->count;
}

size_t tin_vallist_capacity(TinValList* vl)
{
    return vl->capacity;
}

void tin_vallist_setcount(TinValList* vl, size_t nc)
{
    vl->count = nc;
}

void tin_vallist_clear(TinValList* vl)
{
    vl->count = 0;
}

void tin_vallist_deccount(TinValList* vl)
{
    vl->count--;
}

void tin_vallist_ensuresize(TinState* state, TinValList* vl, size_t size)
{
        size_t i;
        size_t oldcapacity;
        if(vl->capacity < size)
        {
            oldcapacity = vl->capacity;
            vl->capacity = size;
            vl->values = TIN_GROW_ARRAY(state, vl->values, sizeof(TinValue), oldcapacity, size);
            for(i = oldcapacity; i < size; i++)
            {
                vl->values[i] = NULL_VALUE;
            }
        }
        if(vl->count < size)
        {
            vl->count = size;
        }
}


TinValue tin_vallist_set(TinValList* vl, size_t idx, TinValue val)
{
    vl->values[idx] = val;
    return val;
}

TinValue tin_vallist_get(TinValList* vl, size_t idx)
{
    return vl->values[idx];
}

void tin_vallist_push(TinState* state, TinValList* vl, TinValue value)
{
        size_t oldcapacity;
        if(vl->capacity < vl->count + 1)
        {
            oldcapacity = vl->capacity;
            vl->capacity = TIN_GROW_CAPACITY(oldcapacity);
            vl->values = TIN_GROW_ARRAY(state, vl->values, sizeof(TinValue), oldcapacity, vl->capacity);
        }
        vl->values[vl->count] = value;
        vl->count++;
}

/* ---- Array object instance functions */

TinArray* tin_object_makearray(TinState* state)
{
    TinArray* array;
    array = (TinArray*)tin_gcmem_allocobject(state, sizeof(TinArray), TINTYPE_ARRAY, false);
    tin_vallist_init(&array->list);
    return array;
}


size_t tin_array_count(TinArray* arr)
{
    return tin_vallist_count(&arr->list);
}

TinValue tin_array_pop(TinState* state, TinArray* arr)
{
    TinValue val;
    (void)state;
    if(tin_vallist_count(&arr->list) > 0)
    {
        val = tin_vallist_get(&arr->list, tin_vallist_count(&arr->list) - 1);
        tin_vallist_deccount(&arr->list);
        return val;
    }
    return NULL_VALUE;
}

int tin_array_indexof(TinArray* array, TinValue value)
{
    size_t i;
    TinValue itm;
    for(i = 0; i < tin_vallist_count(&array->list); i++)
    {
        itm = tin_vallist_get(&array->list, i);
        if(&itm == &value)
        {
            return (int)i;
        }
    }
    return -1;
}

TinValue tin_array_removeat(TinArray* array, size_t index)
{
    size_t i;
    size_t count;
    TinValue value;
    TinValList* vl;
    vl = &array->list;
    count = tin_vallist_count(vl);
    if(index >= count)
    {
        return NULL_VALUE;
    }
    value = tin_vallist_get(vl, index);
    if(index == count - 1)
    {
        tin_vallist_set(vl, index, NULL_VALUE);
    }
    else
    {
        for(i = index; i < tin_vallist_count(vl) - 1; i++)
        {
            tin_vallist_set(vl, i, tin_vallist_get(vl, i + 1));
        }
        tin_vallist_set(vl, count - 1, NULL_VALUE);
    }
    tin_vallist_deccount(vl);
    return value;
}

void tin_array_push(TinState* state, TinArray* array, TinValue val)
{
    tin_vallist_push(state, &array->list, val);
}

TinValue tin_array_get(TinState* state, TinArray* array, size_t idx)
{
    (void)state;
    if(idx <= tin_vallist_count(&array->list))
    {
        return tin_vallist_get(&array->list, idx);
    }
    return NULL_VALUE;
}

TinArray* tin_array_splice(TinState* state, TinArray* oa, int from, int to)
{
    size_t i;
    size_t length;
    TinArray* newarr;
    length = tin_array_count(oa);
    if(from < 0)
    {
        from = (int)length + from;
    }
    if(to < 0)
    {
        to = (int)length + to;
    }
    if(from > to)
    {
        tin_vm_raiseexitingerror(state->vm, "Array.splice argument 'from' is larger than argument 'to'");
        return NULL;
    }
    from = fmax(from, 0);
    to = fmin(to, (int)length - 1);
    length = fmin(length, to - from + 1);
    newarr = tin_object_makearray(state);
    for(i = 0; i < length; i++)
    {
        tin_array_push(state, newarr, tin_array_get(state, oa, from + i));
    }
    return newarr;
}

static TinValue objfn_array_constructor(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return tin_value_fromobject(tin_object_makearray(vm->state));
}

static TinValue objfn_array_splice(TinVM* vm, TinArray* array, int from, int to)
{
    TinArray* newarr;
    newarr = tin_array_splice(vm->state, array, from, to);
    return tin_value_fromobject(newarr);
}

static TinValue objfn_array_slice(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int from;
    int to;
    from = tin_value_checknumber(vm, argv, argc, 0);
    to = tin_value_checknumber(vm, argv, argc, 1);
    return objfn_array_splice(vm, tin_value_asarray(instance), from, to);
}

static TinValue objfn_array_subscript(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int index;
    TinRange* range;
    TinValList* vl;
    if(argc == 2)
    {
        if(!tin_value_isnumber(argv[0]))
        {
            tin_vm_raiseexitingerror(vm, "array index must be a number");
        }
        vl = &tin_value_asarray(instance)->list;
        index = tin_value_asnumber(argv[0]);
        if(index < 0)
        {
            index = fmax(0, tin_vallist_count(vl) + index);
        }
        tin_vallist_ensuresize(vm->state, vl, index + 1);
        return tin_vallist_set(vl, index, argv[1]);
    }
    if(!tin_value_isnumber(argv[0]))
    {
        if(tin_value_isrange(argv[0]))
        {
            range = tin_value_asrange(argv[0]);
            return objfn_array_splice(vm, tin_value_asarray(instance), (int)range->from, (int)range->to);
        }
        tin_vm_raiseexitingerror(vm, "array index must be a number");
        return NULL_VALUE;
    }
    vl = &tin_value_asarray(instance)->list;
    index = tin_value_asnumber(argv[0]);
    if(index < 0)
    {
        index = fmax(0, tin_vallist_count(vl) + index);
    }
    if(tin_vallist_capacity(vl) <= (size_t)index)
    {
        return NULL_VALUE;
    }
    return tin_vallist_get(vl, index);
}


static TinValue objfn_array_compare(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t i;
    TinArray* self;
    TinArray* other;
    (void)argc;
    //fprintf(stderr, "tin_vm_callcallable to objfn_array_compare\n");
    self = tin_value_asarray(instance);
    if(tin_value_isarray(argv[0]))
    {
        other = tin_value_asarray(argv[0]);
        if(tin_vallist_count(&self->list) == tin_vallist_count(&other->list))
        {
            for(i=0; i<tin_vallist_count(&self->list); i++)
            {
                if(!tin_value_compare(vm->state, tin_vallist_get(&self->list, i), tin_vallist_get(&other->list, i)))
                {
                    return FALSE_VALUE;
                }
            }
            return TRUE_VALUE;
        }
        return FALSE_VALUE;
    }
    tin_vm_raiseexitingerror(vm, "can only compare array to another array or null");
    return FALSE_VALUE;
}


static TinValue objfn_array_add(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t i;
    for(i=0; i<argc; i++)
    {
        tin_array_push(vm->state, tin_value_asarray(instance), argv[i]);
    }
    return NULL_VALUE;
}


static TinValue objfn_array_insert(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int i;
    int index;
    TinValue value;
    TinValList* vl;
    TIN_ENSURE_ARGS(vm->state, 2);
    vl = &tin_value_asarray(instance)->list;
    index = tin_value_checknumber(vm, argv, argc, 0);
    if(index < 0)
    {
        index = fmax(0, tin_vallist_count(vl) + index);
    }
    value = argv[1];
    if((int)tin_vallist_count(vl) <= index)
    {
        tin_vallist_ensuresize(vm->state, vl, index + 1);
    }
    else
    {
        tin_vallist_ensuresize(vm->state, vl, tin_vallist_count(vl)  + 1);
        for(i = tin_vallist_count(vl) - 1; i > index; i--)
        {
            tin_vallist_set(vl, i, tin_vallist_get(vl, i - 1));
        }
    }
    tin_vallist_set(vl, index, value);
    return NULL_VALUE;
}

static TinValue objfn_array_addall(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t i;
    TinArray* array;
    TinArray* toadd;
    TIN_ENSURE_ARGS(vm->state, 1);
    if(!tin_value_isarray(argv[0]))
    {
        tin_vm_raiseexitingerror(vm, "expected array as the argument");
    }
    array = tin_value_asarray(instance);
    toadd = tin_value_asarray(argv[0]);
    for(i = 0; i < tin_vallist_count(&toadd->list); i++)
    {
        tin_array_push(vm->state, array, tin_vallist_get(&toadd->list, i));
    }
    return NULL_VALUE;
}

static TinValue objfn_array_indexof(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TIN_ENSURE_ARGS(vm->state, 1);
    int index = tin_array_indexof(tin_value_asarray(instance), argv[0]);
    if(index == -1)
    {
        return NULL_VALUE;
    }
    return tin_value_makefixednumber(vm->state, index);
}

static TinValue objfn_array_remove(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int index;
    TinArray* array;
    TIN_ENSURE_ARGS(vm->state, 1);
    array = tin_value_asarray(instance);
    index = tin_array_indexof(array, argv[0]);
    if(index != -1)
    {
        return tin_array_removeat(array, (size_t)index);
    }
    return NULL_VALUE;
}

static TinValue objfn_array_removeat(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int index;
    index = tin_value_checknumber(vm, argv, argc, 0);
    if(index < 0)
    {
        return NULL_VALUE;
    }
    return tin_array_removeat(tin_value_asarray(instance), (size_t)index);
}

static TinValue objfn_array_contains(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TIN_ENSURE_ARGS(vm->state, 1);
    return tin_value_makebool(vm->state, tin_array_indexof(tin_value_asarray(instance), argv[0]) != -1);
}

static TinValue objfn_array_clear(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    tin_vallist_clear(&tin_value_asarray(instance)->list);
    return NULL_VALUE;
}

static TinValue objfn_array_iterator(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int number;
    TinArray* array;
    (void)vm;
    TIN_ENSURE_ARGS(vm->state, 1);
    array = tin_value_asarray(instance);
    number = 0;
    if(tin_value_isnumber(argv[0]))
    {
        number = tin_value_asnumber(argv[0]);
        if(number >= (int)tin_vallist_count(&array->list) - 1)
        {
            return NULL_VALUE;
        }
        number++;
    }
    if(tin_vallist_count(&array->list) == 0)
    {
        return NULL_VALUE;
    }
    return tin_value_makefixednumber(vm->state, number);
}

static TinValue objfn_array_iteratorvalue(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t index;
    TinValList* vl;
    index = tin_value_checknumber(vm, argv, argc, 0);
    vl = &tin_value_asarray(instance)->list;
    if(tin_vallist_count(vl) <= index)
    {
        return NULL_VALUE;
    }
    return tin_vallist_get(vl, index);
}

static TinValue objfn_array_join(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t i;
    size_t jlen;
    size_t index;
    size_t length;
    char* chars;
    TinValList* vl;
    TinString* string;
    TinString* joinee;
    TinString** strings;
    (void)argc;
    (void)argv;
    joinee = NULL;
    length = 0;
    if(argc > 0)
    {
        joinee = tin_value_asstring(argv[0]);
    }
    vl = &tin_value_asarray(instance)->list;
    //TinString* strings[vl->count];
    strings = TIN_ALLOCATE(vm->state, sizeof(TinString*), tin_vallist_count(vl)+1);
    for(i = 0; i < tin_vallist_count(vl); i++)
    {
        string = tin_value_tostring(vm->state, tin_vallist_get(vl, i));
        strings[i] = string;
        length += tin_string_getlength(string);
        if(joinee != NULL)
        {
            length += tin_string_getlength(joinee);
        }
    }
    jlen = 0;
    index = 0;
    chars = sds_makeempty();
    chars = sds_allocroomfor(chars, length + 1);
    if(joinee != NULL)
    {
        jlen = tin_string_getlength(joinee);
    }
    for(i = 0; i < tin_vallist_count(vl); i++)
    {
        string = strings[i];
        memcpy(chars + index, string->chars, tin_string_getlength(string));
        chars = sds_appendlen(chars, string->chars, tin_string_getlength(string));
        index += tin_string_getlength(string);
        if(joinee != NULL)
        {
            
            //if((i+1) < vl->count)
            {
                chars = sds_appendlen(chars, joinee->chars, jlen);
            }
            index += jlen;
        }
    }
    TIN_FREE(vm->state, sizeof(TinString*), strings);
    return tin_value_fromobject(tin_string_take(vm->state, chars, length, true));
}

static TinValue objfn_array_sort(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinValList* vl;
    (void)vl;
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    vl = &tin_value_asarray(instance)->list;
    /*
    if(argc == 1 && tin_value_iscallablefunction(argv[0]))
    {
        util_custom_quick_sort(vm, vl->values, vl->count, argv[0]);
    }
    else
    {
        util_basic_quick_sort(vm->state, vl->values, vl->count);
    }
    */
    return instance;
}

static TinValue objfn_array_clone(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    size_t i;
    TinState* state;
    TinValList* vl;
    TinArray* array;
    TinValList* newvl;
    state = vm->state;
    vl = &tin_value_asarray(instance)->list;
    array = tin_object_makearray(state);
    newvl = &array->list;
    tin_vallist_ensuresize(state, newvl, tin_vallist_count(vl));
    // tin_vallist_ensuresize sets the count to max of previous count (0 in this case) and new count, so we have to reset it
    tin_vallist_setcount(newvl, 0);
    for(i = 0; i < tin_vallist_count(vl); i++)
    {
        tin_vallist_push(state, newvl, tin_vallist_get(vl, i));
    }
    return tin_value_fromobject(array);
}

static TinValue objfn_array_tostring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    TinArray* self;
    TinWriter wr;
    self = tin_value_asarray(instance);
    tin_writer_init_string(vm->state, &wr);
    tin_towriter_array(vm->state, &wr, self, tin_array_count(self));
    return tin_value_fromobject(tin_writer_get_string(&wr));
}

static TinValue objfn_array_pop(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    TinArray* self;
    self = tin_value_asarray(instance);
    return tin_array_pop(vm->state, self);
}


static TinValue objfn_array_length(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return tin_value_makefixednumber(vm->state, tin_vallist_count(&tin_value_asarray(instance)->list));
}

void tin_open_array_library(TinState* state)
{
    TinClass* klass;
    klass = tin_object_makeclassname(state, "Array");
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
        tin_class_bindconstructor(state, klass, objfn_array_constructor);
        tin_class_bindmethod(state, klass, "[]", objfn_array_subscript);
        tin_class_bindmethod(state, klass, "==", objfn_array_compare);
        tin_class_bindmethod(state, klass, "add", objfn_array_add);
        tin_class_bindmethod(state, klass, "push", objfn_array_add);
        tin_class_bindmethod(state, klass, "insert", objfn_array_insert);
        tin_class_bindmethod(state, klass, "slice", objfn_array_slice);
        tin_class_bindmethod(state, klass, "addAll", objfn_array_addall);
        tin_class_bindmethod(state, klass, "remove", objfn_array_remove);
        tin_class_bindmethod(state, klass, "removeAt", objfn_array_removeat);
        tin_class_bindmethod(state, klass, "indexOf", objfn_array_indexof);
        tin_class_bindmethod(state, klass, "contains", objfn_array_contains);
        tin_class_bindmethod(state, klass, "clear", objfn_array_clear);
        tin_class_bindmethod(state, klass, "iterator", objfn_array_iterator);
        tin_class_bindmethod(state, klass, "iteratorValue", objfn_array_iteratorvalue);
        tin_class_bindmethod(state, klass, "join", objfn_array_join);
        tin_class_bindmethod(state, klass, "sort", objfn_array_sort);
        tin_class_bindmethod(state, klass, "clone", objfn_array_clone);
        tin_class_bindmethod(state, klass, "toString", objfn_array_tostring);
        tin_class_bindmethod(state, klass, "pop", objfn_array_pop);
        tin_class_bindgetset(state, klass, "length", objfn_array_length, NULL, false);
        state->primarrayclass = klass;
    }
    tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
    if(klass->super == NULL)
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
    }
}


