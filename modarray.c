
#include "priv.h"
#include "sds.h"

void lit_datalist_init(LitDataList* dl, size_t typsz)
{
    dl->values = NULL;
    dl->capacity = 0;
    dl->count = 0;
    dl->rawelemsz = typsz;
    dl->elemsz = dl->rawelemsz + sizeof(intptr_t);
}

void lit_datalist_destroy(LitState* state, LitDataList* dl)
{
    LIT_FREE_ARRAY(state, dl->elemsz, dl->values, dl->capacity);
    lit_datalist_init(dl, dl->rawelemsz);
}

size_t lit_datalist_count(LitDataList* dl)
{
    return dl->count;
}

size_t lit_datalist_size(LitDataList* dl)
{
    return dl->count;
}

size_t lit_datalist_capacity(LitDataList* dl)
{
    return dl->capacity;
}

void lit_datalist_clear(LitDataList* dl)
{
    dl->count = 0;
}

void lit_datalist_setcount(LitDataList* dl, size_t nc)
{
    dl->count = nc;
}

void lit_datalist_deccount(LitDataList* dl)
{
    dl->count--;
}

intptr_t lit_datalist_get(LitDataList* dl, size_t idx)
{
    return dl->values[idx];
}

intptr_t lit_datalist_set(LitDataList* dl, size_t idx, intptr_t val)
{
    dl->values[idx] = val;
    return val;
}

void lit_datalist_push(LitState* state, LitDataList* dl, intptr_t value)
{
    size_t oldcapacity;
    if(dl->capacity < (dl->count + 1))
    {
        oldcapacity = dl->capacity;
        dl->capacity = LIT_GROW_CAPACITY(oldcapacity);
        dl->values = LIT_GROW_ARRAY(state, dl->values, dl->elemsz, oldcapacity, dl->capacity);
    }
    dl->values[dl->count] = value;
    dl->count++;
}

void lit_datalist_ensuresize(LitState* state, LitDataList* dl, size_t size)
{
    size_t i;
    size_t oldcapacity;
    if(dl->capacity < size)
    {
        oldcapacity = dl->capacity;
        dl->capacity = size;
        dl->values = LIT_GROW_ARRAY(state, dl->values, dl->elemsz, oldcapacity, size);
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

void lit_vallist_init(LitValList* vl)
{
    vl->values = NULL;
    vl->capacity = 0;
    vl->count = 0;
}

void lit_vallist_destroy(LitState* state, LitValList* vl)
{
    LIT_FREE_ARRAY(state, sizeof(LitValue), vl->values, vl->capacity);
    lit_vallist_init(vl);
}

size_t lit_vallist_size(LitValList* vl)
{
    return vl->count;
}

size_t lit_vallist_count(LitValList* vl)
{
    return vl->count;
}

size_t lit_vallist_capacity(LitValList* vl)
{
    return vl->capacity;
}

void lit_vallist_setcount(LitValList* vl, size_t nc)
{
    vl->count = nc;
}

void lit_vallist_clear(LitValList* vl)
{
    vl->count = 0;
}

void lit_vallist_deccount(LitValList* vl)
{
    vl->count--;
}

void lit_vallist_ensuresize(LitState* state, LitValList* vl, size_t size)
{
        size_t i;
        size_t oldcapacity;
        if(vl->capacity < size)
        {
            oldcapacity = vl->capacity;
            vl->capacity = size;
            vl->values = LIT_GROW_ARRAY(state, vl->values, sizeof(LitValue), oldcapacity, size);
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


LitValue lit_vallist_set(LitValList* vl, size_t idx, LitValue val)
{
    vl->values[idx] = val;
    return val;
}

LitValue lit_vallist_get(LitValList* vl, size_t idx)
{
    return vl->values[idx];
}

void lit_vallist_push(LitState* state, LitValList* vl, LitValue value)
{
        size_t oldcapacity;
        if(vl->capacity < vl->count + 1)
        {
            oldcapacity = vl->capacity;
            vl->capacity = LIT_GROW_CAPACITY(oldcapacity);
            vl->values = LIT_GROW_ARRAY(state, vl->values, sizeof(LitValue), oldcapacity, vl->capacity);
        }
        vl->values[vl->count] = value;
        vl->count++;
}

/* ---- Array object instance functions */

LitArray* lit_create_array(LitState* state)
{
    LitArray* array;
    array = (LitArray*)lit_gcmem_allocobject(state, sizeof(LitArray), LITTYPE_ARRAY, false);
    lit_vallist_init(&array->list);
    return array;
}


size_t lit_array_count(LitArray* arr)
{
    return lit_vallist_count(&arr->list);
}

LitValue lit_array_pop(LitState* state, LitArray* arr)
{
    LitValue val;
    (void)state;
    if(lit_vallist_count(&arr->list) > 0)
    {
        val = lit_vallist_get(&arr->list, lit_vallist_count(&arr->list) - 1);
        lit_vallist_deccount(&arr->list);
        return val;
    }
    return NULL_VALUE;
}

int lit_array_indexof(LitArray* array, LitValue value)
{
    size_t i;
    LitValue itm;
    for(i = 0; i < lit_vallist_count(&array->list); i++)
    {
        itm = lit_vallist_get(&array->list, i);
        if(&itm == &value)
        {
            return (int)i;
        }
    }
    return -1;
}

LitValue lit_array_removeat(LitArray* array, size_t index)
{
    size_t i;
    size_t count;
    LitValue value;
    LitValList* vl;
    vl = &array->list;
    count = lit_vallist_count(vl);
    if(index >= count)
    {
        return NULL_VALUE;
    }
    value = lit_vallist_get(vl, index);
    if(index == count - 1)
    {
        lit_vallist_set(vl, index, NULL_VALUE);
    }
    else
    {
        for(i = index; i < lit_vallist_count(vl) - 1; i++)
        {
            lit_vallist_set(vl, i, lit_vallist_get(vl, i + 1));
        }
        lit_vallist_set(vl, count - 1, NULL_VALUE);
    }
    lit_vallist_deccount(vl);
    return value;
}

void lit_array_push(LitState* state, LitArray* array, LitValue val)
{
    lit_vallist_push(state, &array->list, val);
}

LitValue lit_array_get(LitState* state, LitArray* array, size_t idx)
{
    (void)state;
    if(idx <= lit_vallist_count(&array->list))
    {
        return lit_vallist_get(&array->list, idx);
    }
    return NULL_VALUE;
}

LitArray* lit_array_splice(LitState* state, LitArray* oa, int from, int to)
{
    size_t i;
    size_t length;
    LitArray* newarr;
    length = lit_array_count(oa);
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
        lit_vm_raiseexitingerror(state->vm, "Array.splice argument 'from' is larger than argument 'to'");
        return NULL;
    }
    from = fmax(from, 0);
    to = fmin(to, (int)length - 1);
    length = fmin(length, to - from + 1);
    newarr = lit_create_array(state);
    for(i = 0; i < length; i++)
    {
        lit_array_push(state, newarr, lit_array_get(state, oa, from + i));
    }
    return newarr;
}

static LitValue objfn_array_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_value_fromobject(lit_create_array(vm->state));
}

static LitValue objfn_array_splice(LitVM* vm, LitArray* array, int from, int to)
{
    LitArray* newarr;
    newarr = lit_array_splice(vm->state, array, from, to);
    return lit_value_fromobject(newarr);
}

static LitValue objfn_array_slice(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int from;
    int to;
    from = lit_value_checknumber(vm, argv, argc, 0);
    to = lit_value_checknumber(vm, argv, argc, 1);
    return objfn_array_splice(vm, lit_value_asarray(instance), from, to);
}

static LitValue objfn_array_subscript(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int index;
    LitRange* range;
    LitValList* vl;
    if(argc == 2)
    {
        if(!lit_value_isnumber(argv[0]))
        {
            lit_vm_raiseexitingerror(vm, "array index must be a number");
        }
        vl = &lit_value_asarray(instance)->list;
        index = lit_value_asnumber(argv[0]);
        if(index < 0)
        {
            index = fmax(0, lit_vallist_count(vl) + index);
        }
        lit_vallist_ensuresize(vm->state, vl, index + 1);
        return lit_vallist_set(vl, index, argv[1]);
    }
    if(!lit_value_isnumber(argv[0]))
    {
        if(lit_value_isrange(argv[0]))
        {
            range = lit_value_asrange(argv[0]);
            return objfn_array_splice(vm, lit_value_asarray(instance), (int)range->from, (int)range->to);
        }
        lit_vm_raiseexitingerror(vm, "array index must be a number");
        return NULL_VALUE;
    }
    vl = &lit_value_asarray(instance)->list;
    index = lit_value_asnumber(argv[0]);
    if(index < 0)
    {
        index = fmax(0, lit_vallist_count(vl) + index);
    }
    if(lit_vallist_capacity(vl) <= (size_t)index)
    {
        return NULL_VALUE;
    }
    return lit_vallist_get(vl, index);
}


static LitValue objfn_array_compare(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    LitArray* self;
    LitArray* other;
    (void)argc;
    fprintf(stderr, "lit_vm_callcallable to objfn_array_compare\n");
    self = lit_value_asarray(instance);
    if(lit_value_isarray(argv[0]))
    {
        other = lit_value_asarray(argv[0]);
        if(lit_vallist_count(&self->list) == lit_vallist_count(&other->list))
        {
            for(i=0; i<lit_vallist_count(&self->list); i++)
            {
                if(!lit_value_compare(vm->state, lit_vallist_get(&self->list, i), lit_vallist_get(&other->list, i)))
                {
                    return FALSE_VALUE;
                }
            }
            return TRUE_VALUE;
        }
        return FALSE_VALUE;
    }
    lit_vm_raiseexitingerror(vm, "can only compare array to another array or null");
    return FALSE_VALUE;
}


static LitValue objfn_array_add(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    for(i=0; i<argc; i++)
    {
        lit_array_push(vm->state, lit_value_asarray(instance), argv[i]);
    }
    return NULL_VALUE;
}


static LitValue objfn_array_insert(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int i;
    int index;
    LitValue value;
    LitValList* vl;
    LIT_ENSURE_ARGS(vm->state, 2);
    vl = &lit_value_asarray(instance)->list;
    index = lit_value_checknumber(vm, argv, argc, 0);
    if(index < 0)
    {
        index = fmax(0, lit_vallist_count(vl) + index);
    }
    value = argv[1];
    if((int)lit_vallist_count(vl) <= index)
    {
        lit_vallist_ensuresize(vm->state, vl, index + 1);
    }
    else
    {
        lit_vallist_ensuresize(vm->state, vl, lit_vallist_count(vl)  + 1);
        for(i = lit_vallist_count(vl) - 1; i > index; i--)
        {
            lit_vallist_set(vl, i, lit_vallist_get(vl, i - 1));
        }
    }
    lit_vallist_set(vl, index, value);
    return NULL_VALUE;
}

static LitValue objfn_array_addall(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    LitArray* array;
    LitArray* toadd;
    LIT_ENSURE_ARGS(vm->state, 1);
    if(!lit_value_isarray(argv[0]))
    {
        lit_vm_raiseexitingerror(vm, "expected array as the argument");
    }
    array = lit_value_asarray(instance);
    toadd = lit_value_asarray(argv[0]);
    for(i = 0; i < lit_vallist_count(&toadd->list); i++)
    {
        lit_array_push(vm->state, array, lit_vallist_get(&toadd->list, i));
    }
    return NULL_VALUE;
}

static LitValue objfn_array_indexof(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(vm->state, 1);
    int index = lit_array_indexof(lit_value_asarray(instance), argv[0]);
    return index == -1 ? NULL_VALUE : lit_value_makenumber(vm->state, index);
}


static LitValue objfn_array_remove(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int index;
    LitArray* array;
    LIT_ENSURE_ARGS(vm->state, 1);
    array = lit_value_asarray(instance);
    index = lit_array_indexof(array, argv[0]);
    if(index != -1)
    {
        return lit_array_removeat(array, (size_t)index);
    }
    return NULL_VALUE;
}

static LitValue objfn_array_removeat(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int index;
    index = lit_value_checknumber(vm, argv, argc, 0);
    if(index < 0)
    {
        return NULL_VALUE;
    }
    return lit_array_removeat(lit_value_asarray(instance), (size_t)index);
}

static LitValue objfn_array_contains(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(vm->state, 1);
    return lit_value_makebool(vm->state, lit_array_indexof(lit_value_asarray(instance), argv[0]) != -1);
}

static LitValue objfn_array_clear(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    lit_vallist_clear(&lit_value_asarray(instance)->list);
    return NULL_VALUE;
}

static LitValue objfn_array_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    int number;
    LitArray* array;
    (void)vm;
    LIT_ENSURE_ARGS(vm->state, 1);
    array = lit_value_asarray(instance);
    number = 0;
    if(lit_value_isnumber(argv[0]))
    {
        number = lit_value_asnumber(argv[0]);
        if(number >= (int)lit_vallist_count(&array->list) - 1)
        {
            return NULL_VALUE;
        }
        number++;
    }
    return lit_vallist_count(&array->list) == 0 ? NULL_VALUE : lit_value_makenumber(vm->state, number);
}

static LitValue objfn_array_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t index;
    LitValList* vl;
    index = lit_value_checknumber(vm, argv, argc, 0);
    vl = &lit_value_asarray(instance)->list;
    if(lit_vallist_count(vl) <= index)
    {
        return NULL_VALUE;
    }
    return lit_vallist_get(vl, index);
}

static LitValue objfn_array_join(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t i;
    size_t jlen;
    size_t index;
    size_t length;
    char* chars;
    LitValList* vl;
    LitString* string;
    LitString* joinee;
    LitString** strings;
    (void)argc;
    (void)argv;
    joinee = NULL;
    length = 0;
    if(argc > 0)
    {
        joinee = lit_value_asstring(argv[0]);
    }
    vl = &lit_value_asarray(instance)->list;
    //LitString* strings[vl->count];
    strings = LIT_ALLOCATE(vm->state, sizeof(LitString*), lit_vallist_count(vl)+1);
    for(i = 0; i < lit_vallist_count(vl); i++)
    {
        string = lit_value_tostring(vm->state, lit_vallist_get(vl, i));
        strings[i] = string;
        length += lit_string_getlength(string);
        if(joinee != NULL)
        {
            length += lit_string_getlength(joinee);
        }
    }
    jlen = 0;
    index = 0;
    chars = sdsempty();
    chars = sdsMakeRoomFor(chars, length + 1);
    if(joinee != NULL)
    {
        jlen = lit_string_getlength(joinee);
    }
    for(i = 0; i < lit_vallist_count(vl); i++)
    {
        string = strings[i];
        memcpy(chars + index, string->chars, lit_string_getlength(string));
        chars = sdscatlen(chars, string->chars, lit_string_getlength(string));
        index += lit_string_getlength(string);
        if(joinee != NULL)
        {
            
            //if((i+1) < vl->count)
            {
                chars = sdscatlen(chars, joinee->chars, jlen);
            }
            index += jlen;
        }
    }
    LIT_FREE(vm->state, sizeof(LitString*), strings);
    return lit_value_fromobject(lit_string_take(vm->state, chars, length, true));
}

static LitValue objfn_array_sort(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitValList* vl;
    (void)vl;
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    vl = &lit_value_asarray(instance)->list;
    /*
    if(argc == 1 && lit_value_iscallablefunction(argv[0]))
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

static LitValue objfn_array_clone(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    size_t i;
    LitState* state;
    LitValList* vl;
    LitArray* array;
    LitValList* newvl;
    state = vm->state;
    vl = &lit_value_asarray(instance)->list;
    array = lit_create_array(state);
    newvl = &array->list;
    lit_vallist_ensuresize(state, newvl, lit_vallist_count(vl));
    // lit_vallist_ensuresize sets the count to max of previous count (0 in this case) and new count, so we have to reset it
    lit_vallist_setcount(newvl, 0);
    for(i = 0; i < lit_vallist_count(vl); i++)
    {
        lit_vallist_push(state, newvl, lit_vallist_get(vl, i));
    }
    return lit_value_fromobject(array);
}

static LitValue objfn_array_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitArray* self;
    LitWriter wr;
    self = lit_value_asarray(instance);
    lit_writer_init_string(vm->state, &wr);
    lit_towriter_array(vm->state, &wr, self, lit_array_count(self));
    return lit_value_fromobject(lit_writer_get_string(&wr));
}

static LitValue objfn_array_pop(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitArray* self;
    self = lit_value_asarray(instance);
    return lit_array_pop(vm->state, self);
}


static LitValue objfn_array_length(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_value_makenumber(vm->state, lit_vallist_count(&lit_value_asarray(instance)->list));
}

void lit_open_array_library(LitState* state)
{
    LitClass* klass;
    klass = lit_create_classobject(state, "Array");
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
        lit_class_bindconstructor(state, klass, objfn_array_constructor);
        lit_class_bindmethod(state, klass, "[]", objfn_array_subscript);
        lit_class_bindmethod(state, klass, "==", objfn_array_compare);
        lit_class_bindmethod(state, klass, "add", objfn_array_add);
        lit_class_bindmethod(state, klass, "push", objfn_array_add);
        lit_class_bindmethod(state, klass, "insert", objfn_array_insert);
        lit_class_bindmethod(state, klass, "slice", objfn_array_slice);
        lit_class_bindmethod(state, klass, "addAll", objfn_array_addall);
        lit_class_bindmethod(state, klass, "remove", objfn_array_remove);
        lit_class_bindmethod(state, klass, "removeAt", objfn_array_removeat);
        lit_class_bindmethod(state, klass, "indexOf", objfn_array_indexof);
        lit_class_bindmethod(state, klass, "contains", objfn_array_contains);
        lit_class_bindmethod(state, klass, "clear", objfn_array_clear);
        lit_class_bindmethod(state, klass, "iterator", objfn_array_iterator);
        lit_class_bindmethod(state, klass, "iteratorValue", objfn_array_iteratorvalue);
        lit_class_bindmethod(state, klass, "join", objfn_array_join);
        lit_class_bindmethod(state, klass, "sort", objfn_array_sort);
        lit_class_bindmethod(state, klass, "clone", objfn_array_clone);
        lit_class_bindmethod(state, klass, "toString", objfn_array_tostring);
        lit_class_bindmethod(state, klass, "pop", objfn_array_pop);
        lit_class_bindgetset(state, klass, "length", objfn_array_length, NULL, false);
        state->arrayvalue_class = klass;
    }
    lit_state_setglobal(state, klass->name, lit_value_fromobject(klass));
    if(klass->super == NULL)
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
    }
}


