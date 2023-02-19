
#include "priv.h"

void lit_table_init(LitState* state, LitTable* table)
{
    table->state = state;
    table->capacity = -1;
    table->count = 0;
    table->entries = NULL;
}

void lit_table_destroy(LitState* state, LitTable* table)
{
    if(table->capacity > 0)
    {
        LIT_FREE_ARRAY(state, sizeof(LitTableEntry), table->entries, table->capacity + 1);
    }
    lit_table_init(state, table);
}

static LitTableEntry* find_entry(LitTableEntry* entries, int capacity, LitString* key)
{
    uint32_t index;
    LitTableEntry* entry;
    LitTableEntry* tombstone;
    index = key->hash % capacity;
    tombstone = NULL;
    while(true)
    {
        entry = &entries[index];
        if(entry->key == NULL)
        {
            if(lit_value_isnull(entry->value))
            {
                return tombstone != NULL ? tombstone : entry;
            }
            else if(tombstone == NULL)
            {
                tombstone = entry;
            }
        }
        if(entry->key == key)
        {
            return entry;
        }
        index = (index + 1) % capacity;
    }
}

static void adjust_capacity(LitState* state, LitTable* table, int capacity)
{
    int i;
    LitTableEntry* destination;
    LitTableEntry* entries;
    LitTableEntry* entry;
    entries = LIT_ALLOCATE(state, sizeof(LitTableEntry), capacity + 1);
    for(i = 0; i <= capacity; i++)
    {
        entries[i].key = NULL;
        entries[i].value = NULL_VALUE;
    }
    table->count = 0;
    for(i = 0; i <= table->capacity; i++)
    {
        entry = &table->entries[i];
        if(entry->key == NULL)
        {
            continue;
        }
        destination = find_entry(entries, capacity, entry->key);
        destination->key = entry->key;
        destination->value = entry->value;
        table->count++;
    }
    LIT_FREE_ARRAY(state, sizeof(LitTableEntry), table->entries, table->capacity + 1);
    table->capacity = capacity;
    table->entries = entries;
}

bool lit_table_set(LitState* state, LitTable* table, LitString* key, LitValue value)
{
    bool isnew;
    int capacity;
    LitTableEntry* entry;
    if(table->count + 1 > (table->capacity + 1) * TABLE_MAX_LOAD)
    {
        capacity = LIT_GROW_CAPACITY(table->capacity + 1) - 1;
        adjust_capacity(state, table, capacity);
    }
    entry = find_entry(table->entries, table->capacity, key);
    isnew = entry->key == NULL;
    if(isnew && lit_value_isnull(entry->value))
    {
        table->count++;
    }
    entry->key = key;
    entry->value = value;
    return isnew;
}

bool lit_table_get(LitTable* table, LitString* key, LitValue* value)
{
    LitTableEntry* entry;
    if(table->count == 0)
    {
        return false;
    }
    entry = find_entry(table->entries, table->capacity, key);
    if(entry->key == NULL)
    {
        return false;
    }
    *value = entry->value;
    return true;
}

bool lit_table_get_slot(LitTable* table, LitString* key, LitValue** value)
{
    LitTableEntry* entry;
    if(table->count == 0)
    {
        return false;
    }
    entry = find_entry(table->entries, table->capacity, key);
    if(entry->key == NULL)
    {
        return false;
    }
    *value = &entry->value;
    return true;
}

bool lit_table_delete(LitTable* table, LitString* key)
{
    LitTableEntry* entry;
    if(table->count == 0)
    {
        return false;
    }
    entry = find_entry(table->entries, table->capacity, key);
    if(entry->key == NULL)
    {
        return false;
    }
    entry->key = NULL;
    entry->value = lit_value_makebool(table->state, true);
    return true;
}

LitString* lit_table_find_string(LitTable* table, const char* chars, size_t length, uint32_t hash)
{
    uint32_t index;
    LitTableEntry* entry;
    if(table->count == 0)
    {
        return NULL;
    }
    index = hash % table->capacity;
    while(true)
    {
        entry = &table->entries[index];
        if(entry->key == NULL)
        {
            if(lit_value_isnull(entry->value))
            {
                return NULL;
            }
        }
        else if(lit_string_getlength(entry->key) == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0)
        {
            return entry->key;
        }
        index = (index + 1) % table->capacity;
    }
}

void lit_table_add_all(LitState* state, LitTable* from, LitTable* to)
{
    int i;
    LitTableEntry* entry;
    for(i = 0; i <= from->capacity; i++)
    {
        entry = &from->entries[i];
        if(entry->key != NULL)
        {
            lit_table_set(state, to, entry->key, entry->value);
        }
    }
}

void lit_table_removewhite(LitTable* table)
{
    int i;
    LitTableEntry* entry;
    for(i = 0; i <= table->capacity; i++)
    {
        entry = &table->entries[i];
        if(entry->key != NULL && !entry->key->object.marked)
        {
            lit_table_delete(table, entry->key);
        }
    }
}


int util_table_iterator(LitTable* table, int number)
{
    if(table->count == 0)
    {
        return -1;
    }
    if(number >= (int)table->capacity)
    {
        return -1;
    }
    number++;
    for(; number < table->capacity; number++)
    {
        if(table->entries[number].key != NULL)
        {
            return number;
        }
    }

    return -1;
}

LitValue util_table_iterator_key(LitTable* table, int index)
{
    if(table->capacity <= index)
    {
        return NULL_VALUE;
    }
    return lit_value_fromobject(table->entries[index].key);
}

LitMap* lit_create_map(LitState* state)
{
    LitMap* map;
    map = (LitMap*)lit_gcmem_allocobject(state, sizeof(LitMap), LITTYPE_MAP, false);
    lit_table_init(state, &map->values);
    map->index_fn = NULL;
    return map;
}

bool lit_map_set(LitState* state, LitMap* map, LitString* key, LitValue value)
{
    if(lit_value_isnull(value))
    {
        lit_map_delete(map, key);
        return false;
    }
    return lit_table_set(state, &map->values, key, value);
}

bool lit_map_get(LitMap* map, LitString* key, LitValue* value)
{
    return lit_table_get(&map->values, key, value);
}

bool lit_map_delete(LitMap* map, LitString* key)
{
    return lit_table_delete(&map->values, key);
}

void lit_map_add_all(LitState* state, LitMap* from, LitMap* to)
{
    int i;
    LitTableEntry* entry;
    for(i = 0; i <= from->values.capacity; i++)
    {
        entry = &from->values.entries[i];
        if(entry->key != NULL)
        {
            lit_table_set(state, &to->values, entry->key, entry->value);
        }
    }
}

static LitValue objfn_map_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return lit_value_fromobject(lit_create_map(vm->state));
}

static LitValue objfn_map_subscript(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LitValue val;
    LitValue value;
    LitMap* map;
    LitString* index;
    if(!lit_value_isstring(argv[0]))
    {
        lit_vm_raiseexitingerror(vm, "map index must be a string");
    }
    map = lit_value_asmap(instance);
    index = lit_value_asstring(argv[0]);
    if(argc == 2)
    {
        val = argv[1];
        if(map->index_fn != NULL)
        {
            return map->index_fn(vm, map, index, &val);
        }
        lit_map_set(vm->state, map, index, val);
        return val;
    }
    if(map->index_fn != NULL)
    {
        return map->index_fn(vm, map, index, NULL);
    }
    if(!lit_table_get(&map->values, index, &value))
    {
        return NULL_VALUE;
    }
    return value;
}

static LitValue objfn_map_addall(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(vm->state, 1);
    if(!lit_value_ismap(argv[0]))
    {
        lit_vm_raiseexitingerror(vm, "expected map as the argument");
    }
    lit_map_add_all(vm->state, lit_value_asmap(argv[0]), lit_value_asmap(instance));
    return NULL_VALUE;
}


static LitValue objfn_map_clear(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argv;
    (void)argc;
    lit_value_asmap(instance)->values.count = 0;
    return NULL_VALUE;
}

static LitValue objfn_map_iterator(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    LIT_ENSURE_ARGS(vm->state, 1);
    (void)vm;
    int index;
    int value;
    index = lit_value_isnull(argv[0]) ? -1 : lit_value_asnumber(argv[0]);
    value = util_table_iterator(&lit_value_asmap(instance)->values, index);
    return value == -1 ? NULL_VALUE : lit_value_makenumber(vm->state, value);
}

static LitValue objfn_map_iteratorvalue(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    size_t index;
    index = lit_value_checknumber(vm, argv, argc, 0);
    return util_table_iterator_key(&lit_value_asmap(instance)->values, index);
}

static LitValue objfn_map_clone(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    LitState* state;
    LitMap* map;
    state = vm->state;
    map = lit_create_map(state);
    lit_table_add_all(state, &lit_value_asmap(instance)->values, &map->values);
    return lit_value_fromobject(map);
}

static LitValue objfn_map_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    bool haswrapper;
    bool hasmore;
    size_t i;
    size_t index;
    size_t valueamount;
    size_t olength;
    size_t bufferindex;
    char* buffer;
    LitState* state;
    LitMap* map;
    LitTable* values;
    LitTableEntry* entry;
    LitValue field;
    LitString* strobval;
    LitString* key;
    LitString* value;
    LitString** valuesconverted;
    LitString** keys;
    state = vm->state;
    map = lit_value_asmap(instance);
    values = &map->values;
    if(values->count == 0)
    {
        return lit_value_makestring(state, "{}");
    }
    haswrapper = map->index_fn != NULL;
    hasmore = values->count > LIT_CONTAINER_OUTPUT_MAX;
    valueamount = hasmore ? LIT_CONTAINER_OUTPUT_MAX : values->count;
    valuesconverted = LIT_ALLOCATE(vm->state, sizeof(LitString*), valueamount+1);
    keys = LIT_ALLOCATE(vm->state, sizeof(LitString*), valueamount+1);
    olength = 3;
    if(hasmore)
    {
        olength += SINGLE_LINE_MAPS_ENABLED ? 5 : 6;
    }
    i = 0;
    index = 0;
    do
    {
        entry = &values->entries[index++];
        if(entry->key != NULL)
        {
            // Special hidden key
            field = haswrapper ? map->index_fn(vm, map, entry->key, NULL) : entry->value;
            // This check is required to prevent infinite loops when playing with Module.privates and such
            strobval = (lit_value_ismap(field) && lit_value_asmap(field)->index_fn != NULL) ? lit_string_copyconst(state, "map") : lit_value_tostring(state, field);
            lit_state_pushroot(state, (LitObject*)strobval);
            valuesconverted[i] = strobval;
            keys[i] = entry->key;
            olength += (
                lit_string_getlength(entry->key) + 3 + lit_string_getlength(strobval) +
                #ifdef SINGLE_LINE_MAPS
                    (i == valueamount - 1 ? 1 : 2)
                #else
                    (i == valueamount - 1 ? 2 : 3)
                #endif
            );
            i++;
        }
    } while(i < valueamount);
    buffer = LIT_ALLOCATE(vm->state, sizeof(char), olength+1);
    #ifdef SINGLE_LINE_MAPS
    memcpy(buffer, "{ ", 2);
    #else
    memcpy(buffer, "{\n", 2);
    #endif
    bufferindex = 2;
    for(i = 0; i < valueamount; i++)
    {
        key = keys[i];
        value = valuesconverted[i];
        #ifndef SINGLE_LINE_MAPS
        buffer[bufferindex++] = '\t';
        #endif
        memcpy(&buffer[bufferindex], key->chars, lit_string_getlength(key));
        bufferindex += lit_string_getlength(key);
        memcpy(&buffer[bufferindex], " = ", 3);
        bufferindex += 3;
        memcpy(&buffer[bufferindex], value->chars, lit_string_getlength(value));
        bufferindex += lit_string_getlength(value);
        if(hasmore && i == valueamount - 1)
        {
            #ifdef SINGLE_LINE_MAPS
            memcpy(&buffer[bufferindex], ", ... }", 7);
            #else
            memcpy(&buffer[bufferindex], ",\n\t...\n}", 8);
            #endif
            bufferindex += 8;
        }
        else
        {
            #ifdef SINGLE_LINE_MAPS
            memcpy(&buffer[bufferindex], (i == valueamount - 1) ? " }" : ", ", 2);
            #else
            memcpy(&buffer[bufferindex], (i == valueamount - 1) ? "\n}" : ",\n", 2);
            #endif
            bufferindex += 2;
        }
        lit_state_poproot(state);
    }
    buffer[olength] = '\0';
    LIT_FREE(vm->state, sizeof(LitString*), keys);
    LIT_FREE(vm->state, sizeof(LitString*), valuesconverted);
    return lit_value_fromobject(lit_string_take(vm->state, buffer, olength, false));
}

static LitValue objfn_map_length(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_value_makenumber(vm->state, lit_value_asmap(instance)->values.count);
}

void lit_open_map_library(LitState* state)
{
    LitClass* klass;
    klass = lit_create_classobject(state, "Map");
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
        lit_class_bindconstructor(state, klass, objfn_map_constructor);
        lit_class_bindmethod(state, klass, "[]", objfn_map_subscript);
        lit_class_bindmethod(state, klass, "addAll", objfn_map_addall);
        lit_class_bindmethod(state, klass, "clear", objfn_map_clear);
        lit_class_bindmethod(state, klass, "iterator", objfn_map_iterator);
        lit_class_bindmethod(state, klass, "iteratorValue", objfn_map_iteratorvalue);
        lit_class_bindmethod(state, klass, "clone", objfn_map_clone);
        lit_class_bindmethod(state, klass, "toString", objfn_map_tostring);
        lit_class_bindgetset(state, klass, "length", objfn_map_length, NULL, false);
        state->mapvalue_class = klass;
    }
    lit_state_setglobal(state, klass->name, lit_value_fromobject(klass));
    if(klass->super == NULL)
    {
        lit_class_inheritfrom(state, klass, state->objectvalue_class);
    };
}

