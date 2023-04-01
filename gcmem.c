
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "priv.h"


#define TIN_GCMEM_GROWCAPACITY(cap) \
    (((cap) < 8) ? (8) : ((cap) * 2))


#if 0
static TinObject g_stackmem[1024 * (1024 * 4)];
static size_t g_objcount = 0;
#endif

TinObject* tin_object_allocobject(TinState* state, size_t size, TinObjType type, bool islight)
{
    TinObject* obj;
    islight = false;
    if(islight)
    {
        #if 0
            int ofs = g_objcount; 
            obj = &g_stackmem[ofs];
            g_objcount += 1;
        #else
            goto forcealloc;
        #endif
        obj->mustfree = false;
    }
    else
    {
        forcealloc:
        obj = (TinObject*)tin_gcmem_memrealloc(state, NULL, 0, size);
        obj->mustfree = true;
    }
    obj->type = type;
    obj->marked = false;
    obj->next = state->vm->gcobjects;
    state->vm->gcobjects = obj;
    #ifdef TIN_LOG_ALLOCATION
        fprintf(stderr, "%p allocate %ld for %s\n", (void*)obj, size, tin_tostring_typename(type));
    #endif
    return obj;
}

void* tin_gcmem_memrealloc(TinState* state, void* pointer, size_t oldsize, size_t newsize)
{
    void* ptr;
    ptr = NULL;
    state->gcbytescount += (int64_t)newsize - (int64_t)oldsize;
    if(newsize > oldsize)
    {
#ifdef TIN_STRESS_TEST_GC
        tin_gcmem_collectgarbage(state->vm);
#endif
        if(state->gcbytescount > state->gcnext)
        {
            tin_gcmem_collectgarbage(state->vm);
        }
    }
    if(newsize == 0)
    {
        free(pointer);
        return NULL;
    }
    ptr = (void*)realloc(pointer, newsize);
    if(ptr == NULL)
    {
        tin_state_raiseerror(state, RUNTIME_ERROR, "internal error: failed to allocate %d bytes\n", newsize);
        exit(111);
    }
    return ptr;
}

void* tin_gcmem_allocate(TinState* state, size_t tsz, size_t cnt)
{
    return tin_gcmem_memrealloc(state, NULL, 0, tsz * (cnt));
}

void* tin_gcmem_growarray(TinState* state, void* pptr, size_t tsz, size_t oldcnt, size_t cnt)
{
    return tin_gcmem_memrealloc(state, pptr, tsz * oldcnt, tsz * cnt);
}

void tin_gcmem_free(TinState* state, size_t tsz, void* ptr)
{
    tin_gcmem_memrealloc(state, ptr, tsz, 0);    
}

void tin_gcmem_freearray(TinState* state, size_t tsz, void* ptr, size_t ocount)
{
    tin_gcmem_memrealloc(state, ptr, tsz * ocount, 0);
}

void tin_gcmem_marktable(TinVM* vm, TinTable* table)
{
    int i;
    TinTabEntry* entry;
    for(i = 0; i <= tin_table_getcapacity(table); i++)
    {
        entry = tin_table_getindex(table, i);
        tin_gcmem_markobject(vm, (TinObject*)entry->key);
        tin_gcmem_markvalue(vm, entry->value);
    }
}

void tin_gcmem_markobject(TinVM* vm, TinObject* object)
{
    if(object == NULL || object->marked)
    {
        return;
    }

    object->marked = true;

#ifdef TIN_LOG_MARKING
    printf("%p mark ", (void*)object);
    tin_towriter_value(tin_value_fromobject(object));
    printf("\n");
#endif
    if(vm->gcgraycapacity < vm->gcgraycount + 1)
    {
        vm->gcgraycapacity = TIN_GCMEM_GROWCAPACITY(vm->gcgraycapacity);
        vm->gcgraystack = (TinObject**)realloc(vm->gcgraystack, sizeof(TinObject*) * vm->gcgraycapacity);
    }
    vm->gcgraystack[vm->gcgraycount++] = object;
}

void tin_gcmem_markvalue(TinVM* vm, TinValue value)
{
    if(tin_value_isobject(value))
    {
        tin_gcmem_markobject(vm, tin_value_asobject(value));
    }
}

void tin_gcmem_vmmarkroots(TinVM* vm)
{
    size_t i;
    TinState* state;
    state = vm->state;
    for(i = 0; i < state->gcrootcount; i++)
    {
        tin_gcmem_markvalue(vm, state->gcroots[i]);
    }
    tin_gcmem_markobject(vm, (TinObject*)vm->fiber);
    tin_gcmem_markobject(vm, (TinObject*)state->primclassclass);
    tin_gcmem_markobject(vm, (TinObject*)state->primobjectclass);
    tin_gcmem_markobject(vm, (TinObject*)state->primnumberclass);
    tin_gcmem_markobject(vm, (TinObject*)state->primstringclass);
    tin_gcmem_markobject(vm, (TinObject*)state->primboolclass);
    tin_gcmem_markobject(vm, (TinObject*)state->primfunctionclass);
    tin_gcmem_markobject(vm, (TinObject*)state->primfiberclass);
    tin_gcmem_markobject(vm, (TinObject*)state->primmoduleclass);
    tin_gcmem_markobject(vm, (TinObject*)state->primarrayclass);
    tin_gcmem_markobject(vm, (TinObject*)state->primmapclass);
    tin_gcmem_markobject(vm, (TinObject*)state->primrangeclass);
    tin_gcmem_markobject(vm, (TinObject*)state->capiname);
    tin_gcmem_markobject(vm, (TinObject*)state->capifunction);
    tin_gcmem_markobject(vm, (TinObject*)state->capifiber);
    tin_gcmem_marktable(vm, &vm->modules->values);
    tin_gcmem_marktable(vm, &vm->globals->values);
}

void tin_gcmem_markvallist(TinVM* vm, TinValList* array)
{
    size_t i;
    for(i = 0; i < tin_vallist_count(array); i++)
    {
        tin_gcmem_markvalue(vm, tin_vallist_get(array, i));
    }
}

void tin_gcmem_markarray(TinVM* vm, TinArray* array)
{
    tin_gcmem_markvallist(vm, &array->list);
}

void tin_gcmem_vmblackobject(TinVM* vm, TinObject* object)
{
    size_t i;

#ifdef TIN_LOG_BLACKING
    printf("%p blacken ", (void*)object);
    tin_towriter_value(tin_value_fromobject(object));
    printf("\n");
#endif
    switch(object->type)
    {
        case TINTYPE_NATIVEFUNCTION:
        case TINTYPE_NATIVEPRIMITIVE:
        case TINTYPE_NATIVEMETHOD:
        case TINTYPE_PRIMITIVEMETHOD:
        case TINTYPE_RANGE:
        case TINTYPE_STRING:
        case TINTYPE_NUMBER:
            {
            }
            break;
        case TINTYPE_USERDATA:
            {
                TinUserdata* data;
                data = (TinUserdata*)object;
                if(data->cleanupfn != NULL)
                {
                    data->cleanupfn(vm->state, data, true);
                }
            }
            break;
        case TINTYPE_FUNCTION:
            {
                TinFunction* function;
                function = (TinFunction*)object;
                tin_gcmem_markobject(vm, (TinObject*)function->name);
                tin_gcmem_markvallist(vm, &function->chunk.constants);
            }
            break;
        case TINTYPE_FIBER:
            {
                TinFiber* fiber;
                TinCallFrame* frame;
                TinUpvalue* upvalue;
                fiber = (TinFiber*)object;
                for(TinValue* slot = fiber->stackvalues; slot < fiber->stacktop; slot++)
                {
                    tin_gcmem_markvalue(vm, *slot);
                }
                for(i = 0; i < fiber->framecount; i++)
                {
                    frame = &fiber->framevalues[i];
                    if(frame->closure != NULL)
                    {
                        tin_gcmem_markobject(vm, (TinObject*)frame->closure);
                    }
                    else
                    {
                        tin_gcmem_markobject(vm, (TinObject*)frame->function);
                    }
                }
                for(upvalue = fiber->openupvalues; upvalue != NULL; upvalue = upvalue->next)
                {
                    tin_gcmem_markobject(vm, (TinObject*)upvalue);
                }
                tin_gcmem_markvalue(vm, fiber->errorval);
                tin_gcmem_markobject(vm, (TinObject*)fiber->module);
                tin_gcmem_markobject(vm, (TinObject*)fiber->parent);
            }
            break;
        case TINTYPE_MODULE:
            {
                TinModule* module;
                module = (TinModule*)object;
                tin_gcmem_markvalue(vm, module->returnvalue);
                tin_gcmem_markobject(vm, (TinObject*)module->name);
                tin_gcmem_markobject(vm, (TinObject*)module->mainfunction);
                tin_gcmem_markobject(vm, (TinObject*)module->mainfiber);
                tin_gcmem_markobject(vm, (TinObject*)module->privnames);
                for(i = 0; i < module->privcount; i++)
                {
                    tin_gcmem_markvalue(vm, module->privates[i]);
                }
            }
            break;
        case TINTYPE_CLOSURE:
            {
                TinClosure* closure;
                closure = (TinClosure*)object;
                tin_gcmem_markobject(vm, (TinObject*)closure->function);
                // Check for NULL is needed for a really specific gc-case
                if(closure->upvalues != NULL)
                {
                    for(i = 0; i < closure->upvalcount; i++)
                    {
                        tin_gcmem_markobject(vm, (TinObject*)closure->upvalues[i]);
                    }
                }
            }
            break;
        case TINTYPE_UPVALUE:
            {
                tin_gcmem_markvalue(vm, ((TinUpvalue*)object)->closed);
            }
            break;
        case TINTYPE_CLASS:
            {
                TinClass* klass;
                klass = (TinClass*)object;
                tin_gcmem_markobject(vm, (TinObject*)klass->name);
                tin_gcmem_markobject(vm, (TinObject*)klass->parentclass);
                tin_gcmem_marktable(vm, &klass->methods);
                tin_gcmem_marktable(vm, &klass->staticfields);
            }
            break;
        case TINTYPE_INSTANCE:
            {
                TinInstance* instance = (TinInstance*)object;
                tin_gcmem_markobject(vm, (TinObject*)instance->klass);
                tin_gcmem_marktable(vm, &instance->fields);
            }
            break;
        case TINTYPE_BOUNDMETHOD:
            {
                TinBoundMethod* boundmethod;
                boundmethod = (TinBoundMethod*)object;
                tin_gcmem_markvalue(vm, boundmethod->receiver);
                tin_gcmem_markvalue(vm, boundmethod->method);
            }
            break;
        case TINTYPE_ARRAY:
            {
                TinArray* ta;
                ta = (TinArray*)object;
                tin_gcmem_markvallist(vm, &ta->list);
            }
            break;
        case TINTYPE_MAP:
            {
                TinMap* tmap;
                tmap = (TinMap*)object;
                tin_gcmem_marktable(vm, &tmap->values);
            }
            break;
        case TINTYPE_FIELD:
            {
                TinField* field;
                field = (TinField*)object;
                tin_gcmem_markobject(vm, (TinObject*)field->getter);
                tin_gcmem_markobject(vm, (TinObject*)field->setter);
            }
            break;
        case TINTYPE_REFERENCE:
            {
                TinReference* tref;
                tref = (TinReference*)object;
                tin_gcmem_markvalue(vm, *(tref->slot));
            }
            break;
        default:
            {
                fprintf(stderr, "internal error: trying to blacken something else!\n");
                UNREACHABLE
            }
            break;
    }
}

void tin_gcmem_vmtracerefs(TinVM* vm)
{
    TinObject* object;
    while(vm->gcgraycount > 0)
    {
        object = vm->gcgraystack[--vm->gcgraycount];
        tin_gcmem_vmblackobject(vm, object);
    }
}

void tin_gcmem_vmsweep(TinVM* vm)
{
    TinObject* unreached;
    TinObject* previous;
    TinObject* object;
    previous = NULL;
    object = vm->gcobjects;
    while(object != NULL)
    {
        if(object->marked)
        {
            object->marked = false;
            previous = object;
            object = object->next;
        }
        else
        {
            unreached = object;
            object = object->next;
            if(previous != NULL)
            {
                previous->next = object;
            }
            else
            {
                vm->gcobjects = object;
            }
            tin_object_destroy(vm->state, unreached);
        }
    }
}

uint64_t tin_gcmem_collectgarbage(TinVM* vm)
{
    clock_t t;
    uint64_t before;
    uint64_t collected;
    (void)t;
    if(!vm->state->gcallow)
    {
        return 0;
    }
    vm->state->gcallow = false;
    before = vm->state->gcbytescount;
#ifdef TIN_LOG_GC
    printf("-- gc begin\n");
    t = clock();
#endif
    tin_gcmem_vmmarkroots(vm);
    tin_gcmem_vmtracerefs(vm);
    tin_strreg_remwhite(vm->state);
    tin_gcmem_vmsweep(vm);
    vm->state->gcnext = vm->state->gcbytescount * TIN_GC_HEAP_GROW_FACTOR;
    vm->state->gcallow = true;
    collected = before - vm->state->gcbytescount;
#ifdef TIN_LOG_GC
    printf("-- gc end. Collected %imb in %gms\n", ((int)((collected / 1024.0 + 0.5) / 10)) * 10,
           (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
#endif
    return collected;
}

static TinValue objfn_gc_memory_used(TinVM* vm, TinValue instance, size_t arg_count, TinValue* args)
{
    (void)instance;
    (void)arg_count;
    (void)args;
    return tin_value_makefixednumber(vm->state, vm->state->gcbytescount);
}

static TinValue objfn_gc_next_round(TinVM* vm, TinValue instance, size_t arg_count, TinValue* args)
{
    (void)instance;
    (void)arg_count;
    (void)args;
    return tin_value_makefixednumber(vm->state, vm->state->gcnext);
}

static TinValue objfn_gc_trigger(TinVM* vm, TinValue instance, size_t arg_count, TinValue* args)
{
    (void)instance;
    (void)arg_count;
    (void)args;
    int64_t collected;
    vm->state->gcallow = true;
    collected = tin_gcmem_collectgarbage(vm);
    vm->state->gcallow = false;
    return tin_value_makefixednumber(vm->state, collected);
}

void tin_open_gc_library(TinState* state)
{
    TinClass* klass;
    klass = tin_object_makeclassname(state, "GC");
    {
        tin_class_bindgetset(state, klass, "memoryUsed", objfn_gc_memory_used, NULL, true);
        tin_class_bindgetset(state, klass, "nextRound", objfn_gc_next_round, NULL, true);
        tin_class_bindstaticmethod(state, klass, "trigger", objfn_gc_trigger);
    }
    tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
    if(klass->parentclass == NULL)
    {
        tin_class_inheritfrom(state, klass, state->primobjectclass);
    }
}


