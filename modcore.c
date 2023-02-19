
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



void lit_open_libraries(LitState* state)
{
    lit_open_math_library(state);
    lit_open_file_library(state);
    lit_open_gc_library(state);
}

#if 0
#define COMPARE(state, callee, a, b) \
    ( \
    { \
        LitValue argv[2]; \
        argv[0] = a; \
        argv[1] = b; \
        LitInterpretResult r = lit_state_callvalue(state, callee, argv, 2, false); \
        if(r.type != LITRESULT_OK) \
        { \
            return; \
        } \
        !lit_value_isfalsey(r.result); \
    })
#else
static LitInterpretResult COMPARE_inl(LitState* state, LitValue callee, LitValue a, LitValue b)
{
    LitValue argv[2];
    argv[0] = a;
    argv[1] = b;
    return lit_state_callvalue(state, callee, argv, 2, false);
}

#define COMPARE(state, callee, a, b) \
    COMPARE_inl(state, callee, a, b)
#endif



void util_custom_quick_sort(LitVM* vm, LitValue* l, int length, LitValue callee)
{
    LitInterpretResult rt;
    LitState* state;
    if(length < 2)
    {
        return;
    }
    state = vm->state;
    int pivotindex = length / 2;
    int i;
    int j;
    LitValue pivot = l[pivotindex];
    for(i = 0, j = length - 1;; i++, j--)
    {
        //while(i < pivotindex && COMPARE(state, callee, l[i], pivot))
        while(i < pivotindex)
        {
            if((rt = COMPARE(state, callee, l[i], pivot)).type != LITRESULT_OK)
            {
                return;
            }
            if(lit_value_isfalsey(rt.result))
            {
                break;
            }
            i++;
        }
        //while(j > pivotindex && COMPARE(state, callee, pivot, l[j]))
        while(j > pivotindex)
        {
            if((rt = COMPARE(state, callee, pivot, l[j])).type != LITRESULT_OK)
            {
                return;
            }
            if(lit_value_isfalsey(rt.result))
            {
                break;
            }
            j--;
        }
        if(i >= j)
        {
            break;
        }
        LitValue tmp = l[i];
        l[i] = l[j];
        l[j] = tmp;
    }
    util_custom_quick_sort(vm, l, i, callee);
    util_custom_quick_sort(vm, l + i, length - i, callee);
}

bool util_is_fiber_done(LitFiber* fiber)
{
    return fiber->frame_count == 0 || fiber->abort;
}

void util_run_fiber(LitVM* vm, LitFiber* fiber, LitValue* argv, size_t argc, bool catcher)
{
    bool vararg;
    int i;
    int to;
    int varargcount;
    int objfn_function_arg_count;
    LitArray* array;
    LitCallFrame* frame;
    if(util_is_fiber_done(fiber))
    {
        lit_vm_raiseexitingerror(vm, "Fiber already finished executing");
    }
    fiber->parent = vm->fiber;
    fiber->catcher = catcher;
    vm->fiber = fiber;
    frame = &fiber->frames[fiber->frame_count - 1];
    if(frame->ip == frame->function->chunk.code)
    {
        fiber->arg_count = argc;
        lit_ensure_fiber_stack(vm->state, fiber, frame->function->max_slots + 1 + (int)(fiber->stack_top - fiber->stack));
        frame->slots = fiber->stack_top;
        lit_vm_push(vm, lit_value_fromobject(frame->function));
        vararg = frame->function->vararg;
        objfn_function_arg_count = frame->function->arg_count;
        to = objfn_function_arg_count - (vararg ? 1 : 0);
        fiber->arg_count = objfn_function_arg_count;
        for(i = 0; i < to; i++)
        {
            lit_vm_push(vm, i < (int)argc ? argv[i] : NULL_VALUE);
        }
        if(vararg)
        {
            array = lit_create_array(vm->state);
            lit_vm_push(vm, lit_value_fromobject(array));
            varargcount = argc - objfn_function_arg_count + 1;
            if(varargcount > 0)
            {
                lit_vallist_ensuresize(vm->state, &array->list, varargcount);
                for(i = 0; i < varargcount; i++)
                {
                    lit_vallist_set(&array->list, i, argv[i + objfn_function_arg_count - 1]);
                }
            }
        }
    }
}

static inline bool compare(LitState* state, LitValue a, LitValue b)
{
    LitValue argv[1];
    if(lit_value_isnumber(a) && lit_value_isnumber(b))
    {
        return lit_value_asnumber(a) < lit_value_asnumber(b);
    }
    argv[0] = b;
    return !lit_value_isfalsey(lit_state_findandcallmethod(state, a, lit_string_copyconst(state, "<"), argv, 1, false).result);
}

void util_basic_quick_sort(LitState* state, LitValue* clist, int length)
{
    int i;
    int j;
    int pivotindex;
    LitValue tmp;
    LitValue pivot;
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

bool util_interpret(LitVM* vm, LitModule* module)
{
    LitFunction* function;
    LitFiber* fiber;
    LitCallFrame* frame;
    function = module->main_function;
    fiber = lit_create_fiber(vm->state, module, function);
    fiber->parent = vm->fiber;
    vm->fiber = fiber;
    frame = &fiber->frames[fiber->frame_count - 1];
    if(frame->ip == frame->function->chunk.code)
    {
        frame->slots = fiber->stack_top;
        lit_vm_push(vm, lit_value_fromobject(frame->function));
    }
    return true;
}

static bool compile_and_interpret(LitVM* vm, LitString* modname, char* source, size_t len)
{
    LitModule* module;
    module = lit_state_compilemodule(vm->state, modname, source, len);
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

LitValue util_invalid_constructor(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    lit_vm_raiseexitingerror(vm, "cannot create an instance of built-in type", lit_value_asinstance(instance)->klass->name);
    return NULL_VALUE;
}

static LitValue objfn_number_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return lit_string_numbertostring(vm->state, lit_value_asnumber(instance));
}

static LitValue objfn_number_tochar(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    char ch;
    (void)argc;
    (void)argv;
    ch = lit_value_asnumber(instance);
    return lit_value_fromobject(lit_string_copy(vm->state, &ch, 1));
}

static LitValue objfn_bool_compare(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    bool bv;
    (void)vm;
    (void)argc;
    bv = lit_value_asbool(instance);
    if(lit_value_isnull(argv[0]))
    {
        return lit_value_makebool(vm->state, false);
    }
    return lit_value_makebool(vm->state, lit_value_asbool(argv[0]) == bv);
}

static LitValue objfn_bool_tostring(LitVM* vm, LitValue instance, size_t argc, LitValue* argv)
{
    (void)argc;
    (void)argv;
    return lit_value_makestring(vm->state, lit_value_asbool(instance) ? "true" : "false");
}

static LitValue cfn_time(LitVM* vm, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_value_makenumber(vm->state, (double)clock() / CLOCKS_PER_SEC);
}

static LitValue cfn_systemTime(LitVM* vm, size_t argc, LitValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return lit_value_makenumber(vm->state, time(NULL));
}

static LitValue cfn_print(LitVM* vm, size_t argc, LitValue* argv)
{
    size_t i;
    size_t written = 0;
    LitString* sv;
    written = 0;
    if(argc == 0)
    {
        return lit_value_makenumber(vm->state, 0);
    }
    for(i = 0; i < argc; i++)
    {
        sv = lit_value_tostring(vm->state, argv[i]);
        written += fwrite(sv->chars, sizeof(char), lit_string_getlength(sv), stdout);
    }
    return lit_value_makenumber(vm->state, written);
}

static LitValue cfn_println(LitVM* vm, size_t argc, LitValue* argv)
{
    LitValue r;
    r = cfn_print(vm, argc, argv);
    fprintf(stdout, "\n");
    return r;
}

static bool cfn_eval(LitVM* vm, size_t argc, LitValue* argv)
{
    LitString* sc;
    (void)argc;
    (void)argv;
    sc = lit_value_checkobjstring(vm, argv, argc, 0);
    return compile_and_interpret(vm, vm->fiber->module->name, sc->chars, lit_string_getlength(sc));
}

void lit_open_string_library(LitState* state);
void lit_open_array_library(LitState* state);
void lit_open_map_library(LitState* state);
void lit_open_range_library(LitState* state);
void lit_open_fiber_library(LitState* state);
void lit_open_module_library(LitState* state);
void lit_state_openfunctionlibrary(LitState* state);
void lit_open_class_library(LitState* state);
void lit_state_openobjectlibrary(LitState* state);


void lit_open_core_library(LitState* state)
{
    LitClass* klass;
    /*
    * the order here is important: class must be declared first, and object second,
    * since object derives class, and everything else derives object.
    */
    {
        lit_open_class_library(state);
        lit_state_openobjectlibrary(state);
        lit_open_string_library(state);
        lit_open_array_library(state);
        lit_open_map_library(state);
        lit_open_range_library(state);
        lit_open_fiber_library(state);
        lit_open_module_library(state);
        lit_state_openfunctionlibrary(state);
    }
    {
        klass = lit_create_classobject(state, "Number");
        {
            lit_class_inheritfrom(state, klass, state->objectvalue_class);
            lit_class_bindconstructor(state, klass, util_invalid_constructor);
            lit_class_bindmethod(state, klass, "toString", objfn_number_tostring);
            lit_class_bindmethod(state, klass, "toChar", objfn_number_tochar);
            lit_class_bindgetset(state, klass, "chr", objfn_number_tochar, NULL, false);
            state->numbervalue_class = klass;
        }
        lit_state_setglobal(state, klass->name, lit_value_fromobject(klass));
        if(klass->super == NULL)
        {
            lit_class_inheritfrom(state, klass, state->objectvalue_class);
        };
    }
    {
        klass = lit_create_classobject(state, "Bool");
        {
            lit_class_inheritfrom(state, klass, state->objectvalue_class);
            lit_class_bindconstructor(state, klass, util_invalid_constructor);
            lit_class_bindmethod(state, klass, "==", objfn_bool_compare);
            lit_class_bindmethod(state, klass, "toString", objfn_bool_tostring);
            state->boolvalue_class = klass;
        }
        lit_state_setglobal(state, klass->name, lit_value_fromobject(klass));
        if(klass->super == NULL)
        {
            lit_class_inheritfrom(state, klass, state->objectvalue_class);
        };
    }
    {
        lit_state_defnativefunc(state, "time", cfn_time);
        lit_state_defnativefunc(state, "systemTime", cfn_systemTime);
        lit_state_defnativefunc(state, "print", cfn_print);
        lit_state_defnativefunc(state, "println", cfn_println);
        lit_state_defnativeprimitive(state, "eval", cfn_eval);
        lit_state_setglobal(state, lit_string_copyconst(state, "globals"), lit_value_fromobject(state->vm->globals));
    }
}
