
#include "priv.h"

void tin_table_init(TinState* state, TinTable* table)
{
    table->state = state;
    table->capacity = -1;
    table->count = 0;
    table->entries = NULL;
}

void tin_table_destroy(TinState* state, TinTable* table)
{
    if(table->capacity > 0)
    {
        TIN_FREE_ARRAY(state, sizeof(TinTableEntry), table->entries, table->capacity + 1);
    }
    tin_table_init(state, table);
}

static TinTableEntry* find_entry(TinTableEntry* entries, int capacity, TinString* key)
{
    uint32_t index;
    TinTableEntry* entry;
    TinTableEntry* tombstone;
    index = key->hash % capacity;
    tombstone = NULL;
    while(true)
    {
        entry = &entries[index];
        if(entry->key == NULL)
        {
            if(tin_value_isnull(entry->value))
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
    return NULL;
}

static void adjust_capacity(TinState* state, TinTable* table, int capacity)
{
    int i;
    TinTableEntry* destination;
    TinTableEntry* entries;
    TinTableEntry* entry;
    entries = TIN_ALLOCATE(state, sizeof(TinTableEntry), capacity + 1);
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
    TIN_FREE_ARRAY(state, sizeof(TinTableEntry), table->entries, table->capacity + 1);
    table->capacity = capacity;
    table->entries = entries;
}

bool tin_table_set(TinState* state, TinTable* table, TinString* key, TinValue value)
{
    bool isnew;
    int capacity;
    TinTableEntry* entry;
    if(table->count + 1 > (table->capacity + 1) * TABLE_MAX_LOAD)
    {
        capacity = TIN_GROW_CAPACITY(table->capacity + 1) - 1;
        adjust_capacity(state, table, capacity);
    }
    entry = find_entry(table->entries, table->capacity, key);
    isnew = entry->key == NULL;
    if(isnew && tin_value_isnull(entry->value))
    {
        table->count++;
    }
    entry->key = key;
    entry->value = value;
    return isnew;
}

bool tin_table_get(TinTable* table, TinString* key, TinValue* value)
{
    TinTableEntry* entry;
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

bool tin_table_get_slot(TinTable* table, TinString* key, TinValue** value)
{
    TinTableEntry* entry;
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

bool tin_table_delete(TinTable* table, TinString* key)
{
    TinTableEntry* entry;
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
    entry->value = tin_value_makebool(table->state, true);
    return true;
}

TinString* tin_table_find_string(TinTable* table, const char* chars, size_t length, uint32_t hash)
{
    uint32_t index;
    TinTableEntry* entry;
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
            if(tin_value_isnull(entry->value))
            {
                return NULL;
            }
        }
        else if(tin_string_getlength(entry->key) == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0)
        {
            return entry->key;
        }
        index = (index + 1) % table->capacity;
    }
    return NULL;
}

void tin_table_add_all(TinState* state, TinTable* from, TinTable* to)
{
    int i;
    TinTableEntry* entry;
    for(i = 0; i <= from->capacity; i++)
    {
        entry = &from->entries[i];
        if(entry->key != NULL)
        {
            tin_table_set(state, to, entry->key, entry->value);
        }
    }
}

void tin_table_removewhite(TinTable* table)
{
    int i;
    TinTableEntry* entry;
    for(i = 0; i <= table->capacity; i++)
    {
        entry = &table->entries[i];
        if(entry->key != NULL && !entry->key->object.marked)
        {
            tin_table_delete(table, entry->key);
        }
    }
}

int util_table_iterator(TinTable* table, int number)
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

TinValue util_table_iterator_key(TinTable* table, int index)
{
    if(table->capacity <= index)
    {
        return NULL_VALUE;
    }
    return tin_value_fromobject(table->entries[index].key);
}

TinMap* tin_object_makemap(TinState* state)
{
    TinMap* map;
    map = (TinMap*)tin_gcmem_allocobject(state, sizeof(TinMap), TINTYPE_MAP, false);
    tin_table_init(state, &map->values);
    map->index_fn = NULL;
    return map;
}

bool tin_map_set(TinState* state, TinMap* map, TinString* key, TinValue value)
{
    if(tin_value_isnull(value))
    {
        tin_map_delete(map, key);
        return false;
    }
    return tin_table_set(state, &map->values, key, value);
}

bool tin_map_get(TinMap* map, TinString* key, TinValue* value)
{
    return tin_table_get(&map->values, key, value);
}

bool tin_map_delete(TinMap* map, TinString* key)
{
    return tin_table_delete(&map->values, key);
}

void tin_map_add_all(TinState* state, TinMap* from, TinMap* to)
{
    int i;
    TinTableEntry* entry;
    for(i = 0; i <= from->values.capacity; i++)
    {
        entry = &from->values.entries[i];
        if(entry->key != NULL)
        {
            tin_table_set(state, &to->values, entry->key, entry->value);
        }
    }
}

static TinValue objfn_map_constructor(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return tin_value_fromobject(tin_object_makemap(vm->state));
}

static TinValue objfn_map_subscript(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinValue val;
    TinValue value;
    TinMap* map;
    TinString* index;
    if(!tin_value_isstring(argv[0]))
    {
        tin_vm_raiseexitingerror(vm, "map index must be a string");
    }
    map = tin_value_asmap(instance);
    index = tin_value_asstring(argv[0]);
    if(argc == 2)
    {
        val = argv[1];
        if(map->index_fn != NULL)
        {
            return map->index_fn(vm, map, index, &val);
        }
        tin_map_set(vm->state, map, index, val);
        return val;
    }
    if(map->index_fn != NULL)
    {
        return map->index_fn(vm, map, index, NULL);
    }
    if(!tin_table_get(&map->values, index, &value))
    {
        return NULL_VALUE;
    }
    return value;
}

static TinValue objfn_map_addall(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TIN_ENSURE_ARGS(vm->state, 1);
    if(!tin_value_ismap(argv[0]))
    {
        tin_vm_raiseexitingerror(vm, "expected map as the argument");
    }
    tin_map_add_all(vm->state, tin_value_asmap(argv[0]), tin_value_asmap(instance));
    return NULL_VALUE;
}


static TinValue objfn_map_clear(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argv;
    (void)argc;
    tin_value_asmap(instance)->values.count = 0;
    return NULL_VALUE;
}

static TinValue objfn_map_iterator(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TIN_ENSURE_ARGS(vm->state, 1);
    (void)vm;
    int index;
    int value;
    index = tin_value_isnull(argv[0]) ? -1 : tin_value_asnumber(argv[0]);
    value = util_table_iterator(&tin_value_asmap(instance)->values, index);
    if(value == -1)
    {
        return NULL_VALUE;
    }
    return tin_value_makefixednumber(vm->state, value);
}

static TinValue objfn_map_iteratorvalue(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t index;
    index = tin_value_checknumber(vm, argv, argc, 0);
    return util_table_iterator_key(&tin_value_asmap(instance)->values, index);
}

static TinValue objfn_map_clone(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    TinState* state;
    TinMap* map;
    state = vm->state;
    map = tin_object_makemap(state);
    tin_table_add_all(state, &tin_value_asmap(instance)->values, &map->values);
    return tin_value_fromobject(map);
}

static TinValue objfn_map_tostring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    TinMap* map;
    TinWriter wr;
    map = tin_value_asmap(instance);
    tin_writer_init_string(vm->state, &wr);
    tin_towriter_map(vm->state, &wr, map, map->values.count);
    return tin_value_fromobject(tin_writer_get_string(&wr));
}

static TinValue objfn_map_length(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return tin_value_makefixednumber(vm->state, tin_value_asmap(instance)->values.count);
}

void tin_open_map_library(TinState* state)
{
    TinClass* klass;
    klass = tin_object_makeclassname(state, "Map");
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
        tin_class_bindconstructor(state, klass, objfn_map_constructor);
        tin_class_bindmethod(state, klass, "[]", objfn_map_subscript);
        tin_class_bindmethod(state, klass, "addAll", objfn_map_addall);
        tin_class_bindmethod(state, klass, "clear", objfn_map_clear);
        tin_class_bindmethod(state, klass, "iterator", objfn_map_iterator);
        tin_class_bindmethod(state, klass, "iteratorValue", objfn_map_iteratorvalue);
        tin_class_bindmethod(state, klass, "clone", objfn_map_clone);
        tin_class_bindmethod(state, klass, "toString", objfn_map_tostring);
        tin_class_bindgetset(state, klass, "length", objfn_map_length, NULL, false);
        state->primmapclass = klass;
    }
    tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
    if(klass->super == NULL)
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
    };
}

