
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#if defined(__unix__) || defined(__linux__)
#include <dirent.h>
#endif
#include "priv.h"



void tin_open_libraries(TinState* state)
{
    tin_open_math_library(state);
    tin_open_file_library(state);
    tin_open_gc_library(state);
}

#if 0
#define COMPARE(state, callee, a, b) \
    ( \
    { \
        TinValue argv[2]; \
        argv[0] = a; \
        argv[1] = b; \
        TinInterpretResult r = tin_state_callvalue(state, callee, argv, 2, false); \
        if(r.type != TINSTATE_OK) \
        { \
            return; \
        } \
        !tin_value_isfalsey(r.result); \
    })
#else
static TinInterpretResult COMPARE_inl(TinState* state, TinValue callee, TinValue a, TinValue b)
{
    TinValue argv[2];
    argv[0] = a;
    argv[1] = b;
    return tin_state_callvalue(state, callee, argv, 2, false);
}

#define COMPARE(state, callee, a, b) \
    COMPARE_inl(state, callee, a, b)
#endif



void util_custom_quick_sort(TinVM* vm, TinValue* l, int length, TinValue callee)
{
    TinInterpretResult rt;
    TinState* state;
    if(length < 2)
    {
        return;
    }
    state = vm->state;
    int pivotindex = length / 2;
    int i;
    int j;
    TinValue pivot = l[pivotindex];
    for(i = 0, j = length - 1;; i++, j--)
    {
        //while(i < pivotindex && COMPARE(state, callee, l[i], pivot))
        while(i < pivotindex)
        {
            if((rt = COMPARE(state, callee, l[i], pivot)).type != TINSTATE_OK)
            {
                return;
            }
            if(tin_value_isfalsey(rt.result))
            {
                break;
            }
            i++;
        }
        //while(j > pivotindex && COMPARE(state, callee, pivot, l[j]))
        while(j > pivotindex)
        {
            if((rt = COMPARE(state, callee, pivot, l[j])).type != TINSTATE_OK)
            {
                return;
            }
            if(tin_value_isfalsey(rt.result))
            {
                break;
            }
            j--;
        }
        if(i >= j)
        {
            break;
        }
        TinValue tmp = l[i];
        l[i] = l[j];
        l[j] = tmp;
    }
    util_custom_quick_sort(vm, l, i, callee);
    util_custom_quick_sort(vm, l + i, length - i, callee);
}

bool util_is_fiber_done(TinFiber* fiber)
{
    return fiber->frame_count == 0 || fiber->abort;
}

void util_run_fiber(TinVM* vm, TinFiber* fiber, TinValue* argv, size_t argc, bool catcher)
{
    bool vararg;
    int i;
    int to;
    int varargcount;
    int objfn_function_arg_count;
    TinArray* array;
    TinCallFrame* frame;
    if(util_is_fiber_done(fiber))
    {
        tin_vm_raiseexitingerror(vm, "Fiber already finished executing");
    }
    fiber->parent = vm->fiber;
    fiber->catcher = catcher;
    vm->fiber = fiber;
    frame = &fiber->frames[fiber->frame_count - 1];
    if(frame->ip == frame->function->chunk.code)
    {
        fiber->arg_count = argc;
        tin_fiber_ensurestack(vm->state, fiber, frame->function->maxslots + 1 + (int)(fiber->stack_top - fiber->stack));
        frame->slots = fiber->stack_top;
        tin_vm_push(vm, tin_value_fromobject(frame->function));
        vararg = frame->function->vararg;
        objfn_function_arg_count = frame->function->arg_count;
        to = objfn_function_arg_count - (vararg ? 1 : 0);
        fiber->arg_count = objfn_function_arg_count;
        for(i = 0; i < to; i++)
        {
            tin_vm_push(vm, i < (int)argc ? argv[i] : NULL_VALUE);
        }
        if(vararg)
        {
            array = tin_object_makearray(vm->state);
            tin_vm_push(vm, tin_value_fromobject(array));
            varargcount = argc - objfn_function_arg_count + 1;
            if(varargcount > 0)
            {
                tin_vallist_ensuresize(vm->state, &array->list, varargcount);
                for(i = 0; i < varargcount; i++)
                {
                    tin_vallist_set(&array->list, i, argv[i + objfn_function_arg_count - 1]);
                }
            }
        }
    }
}

static inline bool compare(TinState* state, TinValue a, TinValue b)
{
    TinValue argv[1];
    if(tin_value_isnumber(a) && tin_value_isnumber(b))
    {
        return tin_value_asnumber(a) < tin_value_asnumber(b);
    }
    argv[0] = b;
    return !tin_value_isfalsey(tin_state_findandcallmethod(state, a, tin_string_copyconst(state, "<"), argv, 1, false).result);
}

void util_basic_quick_sort(TinState* state, TinValue* clist, int length)
{
    int i;
    int j;
    int pivotindex;
    TinValue tmp;
    TinValue pivot;
    if(length < 2)
    {
        return;
    }
    pivotindex = length / 2;
    pivot = clist[pivotindex];
    for(i = 0, j = length - 1;; i++, j--)
    {
        while(i < pivotindex && compare(state, clist[i], pivot))
        {
            i++;
        }

        while(j > pivotindex && compare(state, pivot, clist[j]))
        {
            j--;
        }

        if(i >= j)
        {
            break;
        }
        tmp = clist[i];
        clist[i] = clist[j];
        clist[j] = tmp;
    }
    util_basic_quick_sort(state, clist, i);
    util_basic_quick_sort(state, clist + i, length - i);
}

bool util_interpret(TinVM* vm, TinModule* module)
{
    TinFunction* function;
    TinFiber* fiber;
    TinCallFrame* frame;
    function = module->main_function;
    fiber = tin_object_makefiber(vm->state, module, function);
    fiber->parent = vm->fiber;
    vm->fiber = fiber;
    frame = &fiber->frames[fiber->frame_count - 1];
    if(frame->ip == frame->function->chunk.code)
    {
        frame->slots = fiber->stack_top;
        tin_vm_push(vm, tin_value_fromobject(frame->function));
    }
    return true;
}

static bool compile_and_interpret(TinVM* vm, TinString* modname, char* source, size_t len)
{
    TinModule* module;
    module = tin_state_compilemodule(vm->state, modname, source, len);
    if(module == NULL)
    {
        return false;
    }
    module->ran = true;
    return util_interpret(vm, module);
}

bool util_test_file_exists(const char* filename)
{
    struct stat buffer;
    return stat(filename, &buffer) == 0;
}

TinValue util_invalid_constructor(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    tin_vm_raiseexitingerror(vm, "cannot create an instance of built-in type", tin_value_asinstance(instance)->klass->name);
    return NULL_VALUE;
}

static TinValue objfn_number_tostring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    return tin_string_numbertostring(vm->state, tin_value_asnumber(instance));
}

static TinValue objfn_number_tochar(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    char ch;
    (void)argc;
    (void)argv;
    ch = tin_value_asnumber(instance);
    return tin_value_fromobject(tin_string_copy(vm->state, &ch, 1));
}

static TinValue objfn_bool_compare(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    bool bv;
    (void)vm;
    (void)argc;
    bv = tin_value_asbool(instance);
    if(tin_value_isnull(argv[0]))
    {
        return tin_value_makebool(vm->state, false);
    }
    return tin_value_makebool(vm->state, tin_value_asbool(argv[0]) == bv);
}

static TinValue objfn_bool_tostring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    return tin_value_makestring(vm->state, tin_value_asbool(instance) ? "true" : "false");
}

static TinValue cfn_time(TinVM* vm, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return tin_value_makefloatnumber(vm->state, (double)clock() / CLOCKS_PER_SEC);
}

static TinValue cfn_systemTime(TinVM* vm, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return tin_value_makefixednumber(vm->state, time(NULL));
}

static TinValue cfn_print(TinVM* vm, size_t argc, TinValue* argv)
{
    size_t i;
    size_t written = 0;
    TinString* sv;
    written = 0;
    if(argc == 0)
    {
        return tin_value_makefixednumber(vm->state, 0);
    }
    for(i = 0; i < argc; i++)
    {
        #if 0
            sv = tin_value_tostring(vm->state, argv[i]);
            written += fwrite(sv->data, sizeof(char), tin_string_getlength(sv), stdout);
        #else
            //void tin_towriter_value(TinState *state, TinWriter *wr, TinValue value, bool withquot);
            tin_towriter_value(vm->state, &vm->state->stdoutwriter, argv[i], false);
        #endif
    }
    return tin_value_makefixednumber(vm->state, written);
}

static TinValue cfn_println(TinVM* vm, size_t argc, TinValue* argv)
{
    TinValue r;
    r = cfn_print(vm, argc, argv);
    fprintf(stdout, "\n");
    return r;
}

static bool cfn_eval(TinVM* vm, size_t argc, TinValue* argv)
{
    TinString* sc;
    (void)argc;
    (void)argv;
    sc = tin_args_checkobjstring(vm, argv, argc, 0);
    return compile_and_interpret(vm, vm->fiber->module->name, sc->data, tin_string_getlength(sc));
}

void tin_open_string_library(TinState* state);
void tin_open_array_library(TinState* state);
void tin_open_map_library(TinState* state);
void tin_open_range_library(TinState* state);
void tin_open_fiber_library(TinState* state);
void tin_open_module_library(TinState* state);
void tin_state_openfunctionlibrary(TinState* state);
void tin_open_class_library(TinState* state);
void tin_state_openobjectlibrary(TinState* state);


void tin_open_core_library(TinState* state)
{
    TinClass* klass;
    /*
    * the order here is important: class must be declared first, and object second,
    * since object derives class, and everything else derives object.
    */
    {
        tin_open_class_library(state);
        tin_state_openobjectlibrary(state);
        tin_open_string_library(state);
        tin_open_array_library(state);
        tin_open_map_library(state);
        tin_open_range_library(state);
        tin_open_fiber_library(state);
        tin_open_module_library(state);
        tin_state_openfunctionlibrary(state);
    }
    {
        klass = tin_object_makeclassname(state, "Number");
        {
            tin_class_inheritfrom(state, klass, state->primobjectclass);
            tin_class_bindconstructor(state, klass, util_invalid_constructor);
            tin_class_bindmethod(state, klass, "toString", objfn_number_tostring);
            tin_class_bindmethod(state, klass, "toChar", objfn_number_tochar);
            tin_class_bindgetset(state, klass, "chr", objfn_number_tochar, NULL, false);
            state->primnumberclass = klass;
        }
        tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
        if(klass->super == NULL)
        {
            tin_class_inheritfrom(state, klass, state->primobjectclass);
        };
    }
    {
        klass = tin_object_makeclassname(state, "Bool");
        {
            tin_class_inheritfrom(state, klass, state->primobjectclass);
            tin_class_bindconstructor(state, klass, util_invalid_constructor);
            tin_class_bindmethod(state, klass, "==", objfn_bool_compare);
            tin_class_bindmethod(state, klass, "toString", objfn_bool_tostring);
            state->primboolclass = klass;
        }
        tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
        if(klass->super == NULL)
        {
            tin_class_inheritfrom(state, klass, state->primobjectclass);
        };
    }
    {
        tin_state_defnativefunc(state, "time", cfn_time);
        tin_state_defnativefunc(state, "systemTime", cfn_systemTime);
        tin_state_defnativefunc(state, "print", cfn_print);
        tin_state_defnativefunc(state, "println", cfn_println);
        tin_state_defnativeprimitive(state, "eval", cfn_eval);
        tin_state_setglobal(state, tin_string_copyconst(state, "globals"), tin_value_fromobject(state->vm->globals));
    }
}
