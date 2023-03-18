
#include <stdlib.h>
#include <time.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "priv.h"

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#if !(defined(__unix__) || defined(__linux__))
    #define rand_r(v) (*v)
#endif

static size_t staticrandomdata;


static TinValue math_abs(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, fabs(tin_args_checknumber(vm, argv, argc, 0)));
}

static TinValue math_cos(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, cos(tin_args_checknumber(vm, argv, argc, 0)));
}

static TinValue math_sin(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, sin(tin_args_checknumber(vm, argv, argc, 0)));
}

static TinValue math_tan(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, tan(tin_args_checknumber(vm, argv, argc, 0)));
}

static TinValue math_acos(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, acos(tin_args_checknumber(vm, argv, argc, 0)));
}

static TinValue math_asin(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, asin(tin_args_checknumber(vm, argv, argc, 0)));
}

static TinValue math_atan(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, atan(tin_args_checknumber(vm, argv, argc, 0)));
}

static TinValue math_atan2(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, atan2(tin_args_checknumber(vm, argv, argc, 0), tin_args_checknumber(vm, argv, argc, 1)));
}

static TinValue math_floor(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, floor(tin_args_checknumber(vm, argv, argc, 0)));
}

static TinValue math_ceil(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, ceil(tin_args_checknumber(vm, argv, argc, 0)));
}

static TinValue math_round(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int places;
    double value;
    (void)instance;
    value = tin_args_checknumber(vm, argv, argc, 0);
    if(argc > 1)
    {
        places = (int)pow(10, tin_args_checknumber(vm, argv, argc, 1));
        return tin_value_makefloatnumber(vm->state, round(value * places) / places);
    }
    return tin_value_makefloatnumber(vm->state, round(value));
}

static TinValue math_min(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, fmin(tin_args_checknumber(vm, argv, argc, 0), tin_args_checknumber(vm, argv, argc, 1)));
}

static TinValue math_max(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, fmax(tin_args_checknumber(vm, argv, argc, 0), tin_args_checknumber(vm, argv, argc, 1)));
}

static TinValue math_mid(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    double x;
    double y;
    double z;
    (void)instance;
    x = tin_args_checknumber(vm, argv, argc, 0);
    y = tin_args_checknumber(vm, argv, argc, 1);
    z = tin_args_checknumber(vm, argv, argc, 2);
    if(x > y)
    {
        return tin_value_makefloatnumber(vm->state, fmax(x, fmin(y, z)));
    }
    return tin_value_makefloatnumber(vm->state, fmax(y, fmin(x, z)));
}

static TinValue math_toRadians(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, tin_args_checknumber(vm, argv, argc, 0) * M_PI / 180.0);
}

static TinValue math_toDegrees(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, tin_args_checknumber(vm, argv, argc, 0) * 180.0 / M_PI);
}

static TinValue math_sqrt(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, sqrt(tin_args_checknumber(vm, argv, argc, 0)));
}

static TinValue math_log(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, exp(tin_args_checknumber(vm, argv, argc, 0)));
}

static TinValue math_exp(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    return tin_value_makefloatnumber(vm->state, exp(tin_args_checknumber(vm, argv, argc, 0)));
}

/*
 * Random
 */

static size_t* extract_random_data(TinState* state, TinValue instance)
{
    TinValue data;
    if(tin_value_isclass(instance))
    {
        return &staticrandomdata;
    }
    if(!tin_table_get(&tin_value_asinstance(instance)->fields, tin_string_copyconst(state, "_data"), &data))
    {
        return 0;
    }
    return (size_t*)tin_value_asuserdata(data)->data;
}

static TinValue random_constructor(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t* data;
    size_t number;
    TinUserdata* userdata;
    userdata = tin_object_makeuserdata(vm->state, sizeof(size_t), false);
    tin_table_set(vm->state, &tin_value_asinstance(instance)->fields, tin_string_copyconst(vm->state, "_data"), tin_value_fromobject(userdata));
    data = (size_t*)userdata->data;
    if(argc == 1)
    {
        number = (size_t)tin_args_checknumber(vm, argv, argc, 0);
        *data = number;
    }
    else
    {
        *data = time(NULL);
    }
    return instance;
}

static TinValue random_setSeed(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t* data;
    size_t number;
    data = extract_random_data(vm->state, instance);
    if(argc == 1)
    {
        number = (size_t)tin_args_checknumber(vm, argv, argc, 0);
        *data = number;
    }
    else
    {
        *data = time(NULL);
    }
    return NULL_VALUE;
}

static TinValue random_int(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t* data;
    int vmin;
    int vmax;
    int bound;
    data = extract_random_data(vm->state, instance);
    if(argc == 1)
    {
        bound = (int)tin_value_getnumber(vm, argv, argc, 0, 0);
        return tin_value_makefixednumber(vm->state, rand_r((unsigned int*)data) % bound);
    }
    else if(argc == 2)
    {
        vmin = (int)tin_value_getnumber(vm, argv, argc, 0, 0);
        vmax = (int)tin_value_getnumber(vm, argv, argc, 1, 1);
        if(vmax - vmin == 0)
        {
            return tin_value_makefixednumber(vm->state, vmax);
        }
        return tin_value_makefixednumber(vm->state, vmin + rand_r((unsigned int*)data) % (vmax - vmin));
    }
    return tin_value_makefixednumber(vm->state, rand_r((unsigned int*)data));
}

static TinValue random_float(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t* data;
    int vmin;
    int vmax;
    int bound;
    double value;
    data = extract_random_data(vm->state, instance);
    value = (double)rand_r((unsigned int*)data) / RAND_MAX;
    if(argc == 1)
    {
        bound = (int)tin_value_getnumber(vm, argv, argc, 0, 0);
        return tin_value_makefloatnumber(vm->state, value * bound);
    }
    else if(argc == 2)
    {
        vmin = (int)tin_value_getnumber(vm, argv, argc, 0, 0);
        vmax = (int)tin_value_getnumber(vm, argv, argc, 1, 1);
        if(vmax - vmin == 0)
        {
            return tin_value_makefloatnumber(vm->state, vmax);
        }
        return tin_value_makefloatnumber(vm->state, vmin + value * (vmax - vmin));
    }
    return tin_value_makefloatnumber(vm->state, value);
}

static TinValue random_bool(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    return tin_value_makebool(vm->state, rand_r((unsigned int*)extract_random_data(vm->state, instance)) % 2);
}

static TinValue random_chance(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    float c;
    c = tin_value_getnumber(vm, argv, argc, 0, 50);
    if((((float)rand_r((unsigned int*)extract_random_data(vm->state, instance))) / ((float)(RAND_MAX-1)) * 100) <= c)
    {
        return TRUE_VALUE;
    }
    return FALSE_VALUE;
}

static TinValue pickrand_map(TinVM* vm, int randoffset, TinValue* av0)
{
    size_t i;
    size_t fidx;
    size_t target;
    size_t length;
    size_t capacity;
    TinMap* map;
    (void)vm;
    map = tin_value_asmap(*av0);
    length = map->values.count;
    capacity = map->values.capacity;
    if(length == 0)
    {
        return NULL_VALUE;
    }
    target = randoffset % length;
    fidx = 0;
    for(i = 0; i < capacity; i++)
    {
        if(map->values.entries[i].key != NULL)
        {
            if(fidx == target)
            {
                return map->values.entries[i].value;
            }
            fidx++;
        }
    }
    return NULL_VALUE;
}

static TinValue pickrand_array(TinVM* vm, int randoffset, TinValue* av0)
{
    TinArray* array;
    (void)vm;
    array = tin_value_asarray(*av0);
    if(tin_vallist_count(&array->list) == 0)
    {
        return NULL_VALUE;
    }
    return tin_vallist_get(&array->list, randoffset % tin_vallist_count(&array->list));
}

static TinValue random_pick(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int randoffset;
    randoffset = rand_r((unsigned int*)extract_random_data(vm->state, instance));
    if(argc == 1)
    {
        if(tin_value_isarray(argv[0]))
        {
            return pickrand_array(vm, randoffset, &argv[0]);
        }
        else if(tin_value_ismap(argv[0]))
        {
            return pickrand_map(vm, randoffset, &argv[0]);
        }
        else
        {
            tin_vm_raiseexitingerror(vm, "Expected map or array as the argument");
        }
    }
    else
    {
        return argv[randoffset % argc];
    }

    return NULL_VALUE;
}

void tin_open_math_library(TinState* state)
{
    TinClass* klass;
    {
        klass = tin_object_makeclassname(state, "Math");
        {
            tin_class_setstaticfield(state, klass, "Pi", tin_value_makefloatnumber(state, M_PI));
            tin_class_setstaticfield(state, klass, "Tau", tin_value_makefloatnumber(state, M_PI * 2));
            tin_class_bindstaticmethod(state, klass, "abs", math_abs);
            tin_class_bindstaticmethod(state, klass, "sin", math_sin);
            tin_class_bindstaticmethod(state, klass, "cos", math_cos);
            tin_class_bindstaticmethod(state, klass, "tan", math_tan);
            tin_class_bindstaticmethod(state, klass, "asin", math_asin);
            tin_class_bindstaticmethod(state, klass, "acos", math_acos);
            tin_class_bindstaticmethod(state, klass, "atan", math_atan);
            tin_class_bindstaticmethod(state, klass, "atan2", math_atan2);
            tin_class_bindstaticmethod(state, klass, "floor", math_floor);
            tin_class_bindstaticmethod(state, klass, "ceil", math_ceil);
            tin_class_bindstaticmethod(state, klass, "round", math_round);
            tin_class_bindstaticmethod(state, klass, "min", math_min);
            tin_class_bindstaticmethod(state, klass, "max", math_max);
            tin_class_bindstaticmethod(state, klass, "mid", math_mid);
            tin_class_bindstaticmethod(state, klass, "toRadians", math_toRadians);
            tin_class_bindstaticmethod(state, klass, "toDegrees", math_toDegrees);
            tin_class_bindstaticmethod(state, klass, "sqrt", math_sqrt);
            tin_class_bindstaticmethod(state, klass, "log", math_log);
            tin_class_bindstaticmethod(state, klass, "exp", math_exp);
        }
        tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
        if(klass->super == NULL)
        {
            tin_class_inheritfrom(state, klass, state->primobjectclass);
        };
    }
    srand(time(NULL));
    staticrandomdata = time(NULL);
    {
        klass = tin_object_makeclassname(state, "Random");
        {
            tin_class_bindconstructor(state, klass, random_constructor);
            tin_class_bindmethod(state, klass, "setSeed", random_setSeed);
            tin_class_bindmethod(state, klass, "int", random_int);
            tin_class_bindmethod(state, klass, "float", random_float);
            tin_class_bindmethod(state, klass, "chance", random_chance);
            tin_class_bindmethod(state, klass, "pick", random_pick);
            tin_class_bindstaticmethod(state, klass, "setSeed", random_setSeed);
            tin_class_bindstaticmethod(state, klass, "int", random_int);
            tin_class_bindstaticmethod(state, klass, "float", random_float);
            tin_class_bindstaticmethod(state, klass, "bool", random_bool);
            tin_class_bindstaticmethod(state, klass, "chance", random_chance);
            tin_class_bindstaticmethod(state, klass, "pick", random_pick);
        }
        tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
        if(klass->super == NULL)
        {
            tin_class_inheritfrom(state, klass, state->primobjectclass);
        }
    }
}
