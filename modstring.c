
#include "priv.h"
#include "sds.h"

char* itoa(int value, char* result, int base)
{
    int tmp_value;
    char* ptr;
    char* ptr1;
    char tmp_char;
    // check that the base if valid
    if (base < 2 || base > 36)
    {
        *result = '\0';
        return result;
    }
    ptr = result;
    ptr1 = result;
    do
    {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );
    // Apply negative sign
    if (tmp_value < 0)
    {
        *ptr++ = '-';
    }
    *ptr-- = '\0';
    while(ptr1 < ptr)
    {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

static char *tin_util_itoshelper(char *dest, size_t n, int x)
{
    if (n == 0)
    {
        return NULL;
    }
    if (x <= -10)
    {
        dest = tin_util_itoshelper(dest, n - 1, x / 10);
        if (dest == NULL)
        {
            return NULL;
        }
    }
    *dest = (char) ('0' - x % 10);
    return dest + 1;
}

char *tin_util_inttostring(char *dest, size_t n, int x)
{
    char *p;
    p = dest;
    if (n == 0)
    {
        return NULL;
    }
    n--;
    if (x < 0)
    {
        if(n == 0)
        {
            return NULL;
        }
        n--;
        *p++ = '-';
    }
    else
    {
        x = -x;
    }
    p = tin_util_itoshelper(p, n, x);
    if(p == NULL)
    {
        return NULL;
    }
    *p = 0;
    return dest;
}

uint32_t tin_util_hashstring(const char* key, size_t length)
{
    size_t i;
    uint32_t hash = 2166136261u;
    for(i = 0; i < length; i++)
    {
        hash ^= key[i];
        hash *= 16777619;
    }
    return hash;
}

int tin_util_decodenumbytes(uint8_t byte)
{
    if((byte & 0xc0) == 0x80)
    {
        return 0;
    }
    if((byte & 0xf8) == 0xf0)
    {
        return 4;
    }
    if((byte & 0xf0) == 0xe0)
    {
        return 3;
    }
    if((byte & 0xe0) == 0xc0)
    {
        return 2;
    }
    return 1;
}

int tin_ustring_length(TinString* string)
{
    int length;
    uint32_t i;
    length = 0;
    for(i = 0; i < tin_string_getlength(string);)
    {
        i += tin_util_decodenumbytes(string->chars[i]);
        length++;
    }
    return length;
}

TinString* tin_ustring_codepointat(TinState* state, TinString* string, uint32_t index)
{
    char bytes[2];
    int codepoint;
    if(index >= tin_string_getlength(string))
    {
        return NULL;
    }
    codepoint = tin_ustring_decode((uint8_t*)string->chars + index, tin_string_getlength(string) - index);
    if(codepoint == -1)
    {
        bytes[0] = string->chars[index];
        bytes[1] = '\0';
        return tin_string_copy(state, bytes, 1);
    }
    return tin_ustring_fromcodepoint(state, codepoint);
}

TinString* tin_ustring_fromcodepoint(TinState* state, int value)
{
    int length;
    char* bytes;
    TinString* rt;
    length = tin_util_encodenumbytes(value);
    bytes = TIN_ALLOCATE(state, sizeof(char), length + 1);
    tin_ustring_encode(value, (uint8_t*)bytes);
    /* this should be tin_string_take, but something prevents the memory from being free'd. */
    rt = tin_string_copy(state, bytes, length);
    TIN_FREE(state, sizeof(char), bytes);
    return rt;
}

TinString* tin_ustring_fromrange(TinState* state, TinString* source, int start, uint32_t count)
{
    int length;
    int index;
    int codepoint;
    uint32_t i;
    uint8_t* to;
    uint8_t* from;
    char* bytes;
    from = (uint8_t*)source->chars;
    length = 0;
    for(i = 0; i < count; i++)
    {
        length += tin_util_decodenumbytes(from[start + i]);
    }
    bytes = (char*)malloc(length + 1);
    to = (uint8_t*)bytes;
    for(i = 0; i < count; i++)
    {
        index = start + i;
        codepoint = tin_ustring_decode(from + index, tin_string_getlength(source) - index);
        if(codepoint != -1)
        {
            to += tin_ustring_encode(codepoint, to);
        }
    }
    return tin_string_take(state, bytes, length, false);
}

int tin_util_encodenumbytes(int value)
{
    if(value <= 0x7f)
    {
        return 1;
    }
    if(value <= 0x7ff)
    {
        return 2;
    }
    if(value <= 0xffff)
    {
        return 3;
    }
    if(value <= 0x10ffff)
    {
        return 4;
    }
    return 0;
}

int tin_ustring_encode(int value, uint8_t* bytes)
{
    if(value <= 0x7f)
    {
        *bytes = value & 0x7f;
        return 1;
    }
    else if(value <= 0x7ff)
    {
        *bytes = 0xc0 | ((value & 0x7c0) >> 6);
        bytes++;
        *bytes = 0x80 | (value & 0x3f);
        return 2;
    }
    else if(value <= 0xffff)
    {
        *bytes = 0xe0 | ((value & 0xf000) >> 12);
        bytes++;
        *bytes = 0x80 | ((value & 0xfc0) >> 6);
        bytes++;
        *bytes = 0x80 | (value & 0x3f);
        return 3;
    }
    else if(value <= 0x10ffff)
    {
        *bytes = 0xf0 | ((value & 0x1c0000) >> 18);
        bytes++;
        *bytes = 0x80 | ((value & 0x3f000) >> 12);
        bytes++;
        *bytes = 0x80 | ((value & 0xfc0) >> 6);
        bytes++;
        *bytes = 0x80 | (value & 0x3f);
        return 4;
    }
    UNREACHABLE
    return 0;
}

int tin_ustring_decode(const uint8_t* bytes, uint32_t length)
{
    int value;
    uint32_t remainingbytes;
    if(*bytes <= 0x7f)
    {
        return *bytes;
    }
    if((*bytes & 0xe0) == 0xc0)
    {
        value = *bytes & 0x1f;
        remainingbytes = 1;
    }
    else if((*bytes & 0xf0) == 0xe0)
    {
        value = *bytes & 0x0f;
        remainingbytes = 2;
    }
    else if((*bytes & 0xf8) == 0xf0)
    {
        value = *bytes & 0x07;
        remainingbytes = 3;
    }
    else
    {
        return -1;
    }
    if(remainingbytes > length - 1)
    {
        return -1;
    }
    while(remainingbytes > 0)
    {
        bytes++;
        remainingbytes--;
        if((*bytes & 0xc0) != 0x80)
        {
            return -1;
        }
        value = value << 6 | (*bytes & 0x3f);
    }
    return value;
}

int tin_util_ucharoffset(char* str, int index)
{
#define is_utf(c) (((c)&0xC0) != 0x80)
    int offset;
    offset = 0;
    while(index > 0 && str[offset])
    {
        (void)(is_utf(str[++offset]) || is_utf(str[++offset]) || is_utf(str[++offset]) || ++offset);
        index--;
    }
    return offset;
#undef is_utf
}

TinString* tin_string_makeempty(TinState* state, size_t length, bool reuse)
{
    TinString* string;
    string = (TinString*)tin_gcmem_allocobject(state, sizeof(TinString), TINTYPE_STRING, false);
    if(!reuse)
    {
        string->chars = sds_makeempty();
        /* reserving the required space may reduce number of allocations */
        string->chars = sds_allocroomfor(string->chars, length);
    }
    //string->chars = NULL;
    string->hash = 0;
    return string;
}

/*
* if given $chars was alloc'd via sds, then only a TinString instance is created, without initializing
* string->chars.
* if it was not, and not scheduled for reuse, then first an empty sds string is created,
* and $chars is appended, and finally, $chars is freed.
* NB. do *not* actually allocate any sds instance here - this is already done in tin_string_makeempty().
*/
TinString* tin_string_makelen(TinState* state, char* chars, size_t length, uint32_t hash, bool wassds, bool reuse)
{
    TinString* string;
    string = tin_string_makeempty(state, length, reuse);
    if(wassds && reuse)
    {
        string->chars = chars;
    }
    else
    {
        /*
        * string->chars is initialized in tin_string_makeempty(),
        * as an empty string!
        */
        string->chars = sds_appendlen(string->chars, chars, length);
    }
    string->hash = hash;
    if(!wassds)
    {
        TIN_FREE(state, sizeof(char), chars);
    }
    tin_state_regstring(state, string);
    return string;
}

void tin_state_regstring(TinState* state, TinString* string)
{
    if(tin_string_getlength(string) > 0)
    {
        tin_state_pushroot(state, (TinObject*)string);
        tin_table_set(state, &state->vm->gcstrings, string, NULL_VALUE);
        tin_state_poproot(state);
    }
}

/* todo: track if $chars is a sds instance - additional argument $fromsds? */
TinString* tin_string_take(TinState* state, char* chars, size_t length, bool wassds)
{
    bool reuse;
    uint32_t hash;
    hash = tin_util_hashstring(chars, length);
    TinString* interned;
    interned = tin_table_find_string(&state->vm->gcstrings, chars, length, hash);
    if(interned != NULL)
    {
        if(!wassds)
        {
            TIN_FREE(state, sizeof(char), chars);
            //sds_destroy(chars);
        }
        return interned;
    }
    reuse = wassds;
    return tin_string_makelen(state, (char*)chars, length, hash, wassds, reuse);
}

TinString* tin_string_copy(TinState* state, const char* chars, size_t length)
{
    uint32_t hash;
    char* heapchars;
    TinString* interned;
    hash= tin_util_hashstring(chars, length);
    interned = tin_table_find_string(&state->vm->gcstrings, chars, length, hash);
    if(interned != NULL)
    {
        return interned;
    }
    /*
    heapchars = TIN_ALLOCATE(state, sizeof(char), length + 1);
    memcpy(heapchars, chars, length);
    heapchars[length] = '\0';
    */
    heapchars = sds_makelength(chars, length);
#ifdef TIN_LOG_ALLOCATION
    printf("Allocated new string '%s'\n", chars);
#endif
    return tin_string_makelen(state, heapchars, length, hash, true, true);
}

const char* tin_string_getdata(TinString* ls)
{
    return ls->chars;
}

size_t tin_string_getlength(TinString* ls)
{
    if(ls->chars == NULL)
    {
        return 0;
    }
    return sds_getlength(ls->chars);
}

void tin_string_appendlen(TinString* ls, const char* s, size_t len)
{
    if(len > 0)
    {
        if(ls->chars == NULL)
        {
            ls->chars = sds_makelength(s, len);
        }
        else
        {
            ls->chars = sds_appendlen(ls->chars, s, len);
        }
    }
}

void tin_string_appendobj(TinString* ls, TinString* other)
{
    tin_string_appendlen(ls, other->chars, tin_string_getlength(other));
}

void tin_string_appendchar(TinString* ls, char ch)
{
    ls->chars = sds_appendlen(ls->chars, (const char*)&ch, 1);
}

TinValue tin_string_numbertostring(TinState* state, double value)
{
    if(isnan(value))
    {
        return tin_value_makestring(state, "nan");
    }

    if(isinf(value))
    {
        if(value > 0.0)
        {
            return tin_value_makestring(state, "infinity");
        }
        else
        {
            return tin_value_makestring(state, "-infinity");
        }
    }

    char buffer[24];
    int length = sprintf(buffer, "%.14g", value);

    return tin_value_fromobject(tin_string_copy(state, buffer, length));
}


TinValue tin_string_format(TinState* state, const char* format, ...)
{
    char ch;
    size_t length;
    size_t totallength;
    bool wasallowed;
    const char* c;
    const char* strval;
    va_list arglist;
    TinValue val;
    TinString* string;
    TinString* result;
    wasallowed = state->gcallow;
    state->gcallow = false;
    va_start(arglist, format);
    totallength = strlen(format);
    va_end(arglist);
    result = tin_string_makeempty(state, totallength + 1, false);
    va_start(arglist, format);
    for(c = format; *c != '\0'; c++)
    {
        switch(*c)
        {
            case '$':
                {
                    strval = va_arg(arglist, const char*);
                    if(strval != NULL)
                    {
                        length = strlen(strval);
                        result->chars = sds_appendlen(result->chars, strval, length);
                    }
                    else
                    {
                        goto defaultendingcopying;
                    }
                }
                break;
            case '@':
                {
                    val = va_arg(arglist, TinValue);
                    if(tin_value_isstring(val))
                    {
                        string = tin_value_asstring(val);
                    }
                    else
                    {
                        //fprintf(stderr, "format: not a string, but a '%s'\n", tin_tostring_typename(val));
                        //string = tin_value_tostring(state, val);
                        goto defaultendingcopying;
                    }
                    if(string != NULL)
                    {
                        length = sds_getlength(string->chars);
                        if(length > 0)
                        {
                            result->chars = sds_appendlen(result->chars, string->chars, length);
                        }
                    }
                    else
                    {
                        goto defaultendingcopying;
                    }
                }
                break;
            case '#':
                {
                    string = tin_value_asstring(tin_string_numbertostring(state, va_arg(arglist, double)));
                    length = sds_getlength(string->chars);
                    if(length > 0)
                    {
                        result->chars = sds_appendlen(result->chars, string->chars, length);
                    }
                }
                break;
            default:
                {
                    defaultendingcopying:
                    ch = *c;
                    result->chars = sds_appendlen(result->chars, &ch, 1);
                }
                break;
        }
    }
    va_end(arglist);
    result->hash = tin_util_hashstring(result->chars, tin_string_getlength(result));
    tin_state_regstring(state, result);
    state->gcallow = wasallowed;
    return tin_value_fromobject(result);
}

bool tin_string_equal(TinState* state, TinString* a, TinString* b)
{
    (void)state;
    if((a == NULL) || (b == NULL))
    {
        return false;
    }
    return (sds_compare(a->chars, b->chars) == 0);
}

TinValue util_invalid_constructor(TinVM* vm, TinValue instance, size_t argc, TinValue* argv);


static TinValue objfn_string_fromcharcode(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    char ch;
    TinString* s;
    (void)instance;
    ch = tin_value_checknumber(vm, argv, argc, 0);
    s = tin_string_copy(vm->state, &ch, 1);
    return tin_value_fromobject(s);
}

static TinValue objfn_string_plus(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinString* selfstr;
    TinString* result;
    TinValue value;
    (void)argc;
    selfstr = tin_value_asstring(instance);
    value = argv[0];
    TinString* strval = NULL;
    if(tin_value_isstring(value))
    {
        strval = tin_value_asstring(value);
    }
    else
    {
        strval = tin_value_tostring(vm->state, value);
    }
    result = tin_string_makeempty(vm->state, tin_string_getlength(selfstr) + tin_string_getlength(strval), false);
    tin_string_appendobj(result, selfstr);
    tin_string_appendobj(result, strval);
    tin_state_regstring(vm->state, result);
    return tin_value_fromobject(result);
}

static TinValue objfn_string_splice(TinVM* vm, TinString* string, int from, int to)
{
    int length;
    length = tin_ustring_length(string);
    if(from < 0)
    {
        from = length + from;
    }
    if(to < 0)
    {
        to = length + to;
    }
    from = fmax(from, 0);
    to = fmin(to, length - 1);
    if(from > to)
    {
        tin_vm_raiseexitingerror(vm, "String.splice argument 'from' is larger than argument 'to'");
    }
    from = tin_util_ucharoffset(string->chars, from);
    to = tin_util_ucharoffset(string->chars, to);
    return tin_value_fromobject(tin_ustring_fromrange(vm->state, string, from, to - from + 1));
}

static TinValue objfn_string_subscript(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int index;
    TinString* c;
    TinString* string;
    if(tin_value_isrange(argv[0]))
    {
        TinRange* range = tin_value_asrange(argv[0]);
        return objfn_string_splice(vm, tin_value_asstring(instance), range->from, range->to);
    }
    string = tin_value_asstring(instance);
    index = tin_value_checknumber(vm, argv, argc, 0);
    if(argc != 1)
    {
        tin_vm_raiseexitingerror(vm, "cannot modify strings with the subscript op");
    }
    if(index < 0)
    {
        index = tin_ustring_length(string) + index;

        if(index < 0)
        {
            return NULL_VALUE;
        }
    }
    c = tin_ustring_codepointat(vm->state, string, tin_util_ucharoffset(string->chars, index));
    return c == NULL ? NULL_VALUE : tin_value_fromobject(c);
}


static TinValue objfn_string_compare(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinString* self;
    TinString* other;
    (void)argc;
    self = tin_value_asstring(instance);
    if(tin_value_isstring(argv[0]))
    {
        other = tin_value_asstring(argv[0]);
        if(tin_string_getlength(self) == tin_string_getlength(other))
        {
            //fprintf(stderr, "string: same length(self=\"%s\" other=\"%s\")... strncmp=%d\n", self->chars, other->chars, strncmp(self->chars, other->chars, self->length));
            if(memcmp(self->chars, other->chars, tin_string_getlength(self)) == 0)
            {
                return TRUE_VALUE;
            }
        }
        return FALSE_VALUE;
    }
    else if(tin_value_isnull(argv[0]))
    {
        if((self == NULL) || tin_value_isnull(instance))
        {
            return TRUE_VALUE;
        }
        return FALSE_VALUE;
    }
    tin_vm_raiseexitingerror(vm, "can only compare string to another string or null");
    return FALSE_VALUE;
}

static TinValue objfn_string_less(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    return tin_value_makebool(vm->state, strcmp(tin_value_asstring(instance)->chars, tin_value_checkstring(vm, argv, argc, 0)) < 0);
}

static TinValue objfn_string_greater(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    return tin_value_makebool(vm->state, strcmp(tin_value_asstring(instance)->chars, tin_value_checkstring(vm, argv, argc, 0)) > 0);
}

static TinValue objfn_string_tostring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return instance;
}

static TinValue objfn_string_tonumber(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    double result;
    (void)vm;
    (void)argc;
    (void)argv;
    result = strtod(tin_value_asstring(instance)->chars, NULL);
    if(errno == ERANGE)
    {
        errno = 0;
        return NULL_VALUE;
    }
    return tin_value_makefixednumber(vm->state, result);
}

static TinValue objfn_string_touppercase(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t i;
    char* buffer;
    TinString* rt;
    TinString* string;
    string = tin_value_asstring(instance);
    (void)argc;
    (void)argv;
    buffer = TIN_ALLOCATE(vm->state, sizeof(char), tin_string_getlength(string) + 1);
    for(i = 0; i < tin_string_getlength(string); i++)
    {
        buffer[i] = (char)toupper(string->chars[i]);
    }
    buffer[i] = 0;
    rt = tin_string_take(vm->state, buffer, tin_string_getlength(string), false);
    return tin_value_fromobject(rt);
}

static TinValue objfn_string_tolowercase(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t i;
    char* buffer;
    TinString* rt;
    TinString* string;
    string = tin_value_asstring(instance);
    (void)argc;
    (void)argv;
    buffer = TIN_ALLOCATE(vm->state, sizeof(char), tin_string_getlength(string) + 1);
    for(i = 0; i < tin_string_getlength(string); i++)
    {
        buffer[i] = (char)tolower(string->chars[i]);
    }
    buffer[i] = 0;
    rt = tin_string_take(vm->state, buffer, tin_string_getlength(string), false);
    return tin_value_fromobject(rt);
}

static TinValue objfn_string_tobyte(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int iv;
    TinString* selfstr;
    (void)vm;
    (void)argc;
    (void)argv;
    selfstr = tin_value_asstring(instance);
    iv = tin_ustring_decode((const uint8_t*)selfstr->chars, tin_string_getlength(selfstr));
    return tin_value_makefixednumber(vm->state, iv);
}

static TinValue objfn_string_contains(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    TinString* sub;
    TinString* string;
    string = tin_value_asstring(instance);
    sub = tin_value_checkobjstring(vm, argv, argc, 0);
    if(sub == string)
    {
        return TRUE_VALUE;
    }
    return tin_value_makebool(vm->state, strstr(string->chars, sub->chars) != NULL);
}

static TinValue objfn_string_startswith(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t i;
    TinString* sub;
    TinString* string;
    string = tin_value_asstring(instance);
    sub = tin_value_checkobjstring(vm, argv, argc, 0);
    if(sub == string)
    {
        return TRUE_VALUE;
    }
    if(tin_string_getlength(sub) > tin_string_getlength(string))
    {
        return FALSE_VALUE;
    }
    for(i = 0; i < tin_string_getlength(sub); i++)
    {
        if(sub->chars[i] != string->chars[i])
        {
            return FALSE_VALUE;
        }
    }
    return TRUE_VALUE;
}

static TinValue objfn_string_endswith(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t i;
    size_t start;
    TinString* sub;
    TinString* string;
    string = tin_value_asstring(instance);
    sub = tin_value_checkobjstring(vm, argv, argc, 0);
    if(sub == string)
    {
        return TRUE_VALUE;
    }
    if(tin_string_getlength(sub) > tin_string_getlength(string))
    {
        return FALSE_VALUE;
    }
    start = tin_string_getlength(string) - tin_string_getlength(sub);
    for(i = 0; i < tin_string_getlength(sub); i++)
    {
        if(sub->chars[i] != string->chars[i + start])
        {
            return FALSE_VALUE;
        }
    }
    return TRUE_VALUE;
}

static TinValue objfn_string_replace(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    size_t i;
    size_t bufferlength;
    size_t bufferindex;
    char* buffer;
    TinString* string;
    TinString* what;
    TinString* with;
    TIN_ENSURE_ARGS(vm->state, 2);
    if(!tin_value_isstring(argv[0]) || !tin_value_isstring(argv[1]))
    {
        tin_vm_raiseexitingerror(vm, "expected 2 string arguments");
    }
    string = tin_value_asstring(instance);
    what = tin_value_asstring(argv[0]);
    with = tin_value_asstring(argv[1]);
    bufferlength = 0;
    for(i = 0; i < tin_string_getlength(string); i++)
    {
        if(strncmp(string->chars + i, what->chars, tin_string_getlength(what)) == 0)
        {
            i += tin_string_getlength(what) - 1;
            bufferlength += tin_string_getlength(with);
        }
        else
        {
            bufferlength++;
        }
    }
    bufferindex = 0;
    buffer = TIN_ALLOCATE(vm->state, sizeof(char), bufferlength+1);
    for(i = 0; i < tin_string_getlength(string); i++)
    {
        if(strncmp(string->chars + i, what->chars, tin_string_getlength(what)) == 0)
        {
            memcpy(buffer + bufferindex, with->chars, tin_string_getlength(with));
            bufferindex += tin_string_getlength(with);
            i += tin_string_getlength(what) - 1;
        }
        else
        {
            buffer[bufferindex] = string->chars[i];
            bufferindex++;
        }
    }
    buffer[bufferlength] = '\0';
    return tin_value_fromobject(tin_string_take(vm->state, buffer, bufferlength, false));
}

static TinValue objfn_string_substring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int to;
    int from;
    from = tin_value_checknumber(vm, argv, argc, 0);
    to = tin_value_checknumber(vm, argv, argc, 1);
    return objfn_string_splice(vm, tin_value_asstring(instance), from, to);
}

static TinValue objfn_string_byteat(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int idx;
    idx = tin_value_checknumber(vm, argv, argc, 0);
    return tin_value_makefixednumber(vm->state, tin_value_asstring(instance)->chars[idx]);
}

static TinValue objfn_string_indexof(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int i;
    int len;
    char findme;
    TinString* self;
    findme = -1;
    if(tin_value_isstring(argv[0]))
    {
        findme = tin_value_checkobjstring(vm, argv, argc, 0)->chars[0];
    }
    else
    {
        if(tin_value_isnull(argv[0]))
        {
            return tin_value_makefixednumber(vm->state, -1);
        }
        findme = tin_value_checknumber(vm, argv, argc, 0);
    }
    self = tin_value_asstring(instance);
    len = tin_string_getlength(self);
    for(i=0; i<len; i++)
    {
        if(self->chars[i] == findme)
        {
            return tin_value_makefixednumber(vm->state, i);
        }
    }
    return tin_value_makefixednumber(vm->state, -1);
}


static TinValue objfn_string_length(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return tin_value_makefixednumber(vm->state, tin_ustring_length(tin_value_asstring(instance)));
}

static TinValue objfn_string_iterator(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    int index;
    TinString* string;
    string = tin_value_asstring(instance);
    if(tin_value_isnull(argv[0]))
    {
        if(tin_string_getlength(string) == 0)
        {
            return NULL_VALUE;
        }
        return tin_value_makefixednumber(vm->state, 0);
    }
    index = tin_value_checknumber(vm, argv, argc, 0);
    if(index < 0)
    {
        return NULL_VALUE;
    }
    do
    {
        index++;
        if(index >= (int)tin_string_getlength(string))
        {
            return NULL_VALUE;
        }
    } while((string->chars[index] & 0xc0) == 0x80);
    return tin_value_makefixednumber(vm->state, index);
}


static TinValue objfn_string_iteratorvalue(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    uint32_t index;
    TinString* string;
    string = tin_value_asstring(instance);
    index = tin_value_checknumber(vm, argv, argc, 0);
    if(index == UINT32_MAX)
    {
        return FALSE_VALUE;
    }
    return tin_value_fromobject(tin_ustring_codepointat(vm->state, string, index));
}


bool check_fmt_arg(TinVM* vm, char* buf, size_t ai, size_t argc, TinValue* argv, const char* fmttext)
{
    (void)argv;
    if(ai <= argc)
    {
        return true;
    }
    sds_destroy(buf);
    tin_vm_raiseexitingerror(vm, "too few arguments for format flag '%s' at position %d (argc=%d)", fmttext, ai, argc);
    return false;
}

static TinValue objfn_string_format(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    char ch;
    char pch;
    char nch;
    char tmpch;
    int iv;
    size_t i;
    size_t ai;
    size_t selflen;
    TinString* selfstr;
    TinValue rtv;
    char* buf;
    (void)pch;
    selfstr = tin_value_asstring(instance);
    selflen = tin_string_getlength(selfstr);
    buf = sds_makeempty();
    buf = sds_allocroomfor(buf, selflen + 10);
    ai = 0;
    ch = -1;
    pch = -1;
    nch = -1;
    for(i=0; i<selflen; i++)
    {
        pch = ch;
        ch = selfstr->chars[i];
        if(i <= selflen)
        {
            nch = selfstr->chars[i + 1];
        }
        if(ch == '%')
        {
            if(nch == '%')
            {
                buf = sds_appendlen(buf, &ch, 1);
                i += 1;
            }
            else
            {
                i++;
                switch(nch)
                {
                    case 's':
                        {
                            if(!check_fmt_arg(vm, buf, ai, argc, argv, "%s"))
                            {
                                return NULL_VALUE;
                            }
                            buf = sds_appendlen(buf, tin_value_asstring(argv[ai])->chars, tin_string_getlength(tin_value_asstring(argv[ai])));
                        }
                        break;
                    case 'd':
                    case 'i':
                        {
                            if(!check_fmt_arg(vm, buf, ai, argc, argv, "%d"))
                            {
                                return NULL_VALUE;
                            }
                            if(tin_value_isnumber(argv[ai]))
                            {
                                iv = tin_value_checknumber(vm, argv, argc, ai);
                                buf = sds_appendfmt(buf, "%i", iv);
                            }
                            break;
                        }
                        break;
                    case 'c':
                        {
                            if(!check_fmt_arg(vm, buf, ai, argc, argv, "%d"))
                            {
                                return NULL_VALUE;
                            }
                            if(!tin_value_isnumber(argv[ai]))
                            {
                                sds_destroy(buf);
                                tin_vm_raiseexitingerror(vm, "flag 'c' expects a number");
                            }
                            iv = tin_value_checknumber(vm, argv, argc, ai);
                            /* TODO: both of these use the same amount of memory. but which is faster? */
                            #if 0
                                buf = sds_appendfmt(buf, "%c", iv);
                            #else
                                tmpch = iv;
                                buf = sds_appendlen(buf, &tmpch, 1);
                            #endif
                        }
                        break;
                    default:
                        {
                            sds_destroy(buf);
                            tin_vm_raiseexitingerror(vm, "unrecognized formatting flag '%c'", nch);
                            return NULL_VALUE;
                        }
                        break;
                }
                ai++;
            }
        }
        else
        {
            buf = sds_appendlen(buf, &ch, 1);
        }
    }
    rtv = tin_value_fromobject(tin_string_copy(vm->state, buf, sds_getlength(buf)));
    sds_destroy(buf);
    return rtv;
}

void tin_open_string_library(TinState* state)
{
    {
        TinClass* klass;
        klass = tin_object_makeclassname(state, "String");
        {
            tin_class_inheritfrom(state, klass, state->primobjectclass);
            tin_class_bindconstructor(state, klass, util_invalid_constructor);
            tin_class_bindstaticmethod(state, klass, "fromCharCode", objfn_string_fromcharcode);
            tin_class_bindmethod(state, klass, "+", objfn_string_plus);
            tin_class_bindmethod(state, klass, "[]", objfn_string_subscript);
            tin_class_bindmethod(state, klass, "<", objfn_string_less);
            tin_class_bindmethod(state, klass, ">", objfn_string_greater);
            tin_class_bindmethod(state, klass, "==", objfn_string_compare);
            tin_class_bindmethod(state, klass, "toString", objfn_string_tostring);
            // js-isms
            tin_class_bindmethod(state, klass, "indexOf", objfn_string_indexof);
            tin_class_bindmethod(state, klass, "charCodeAt", objfn_string_byteat);
            tin_class_bindmethod(state, klass, "charAt", objfn_string_subscript);
            {
                tin_class_bindmethod(state, klass, "toNumber", objfn_string_tonumber);
                // ruby-isms? bit of an identity crisis.
                tin_class_bindgetset(state, klass, "to_i", objfn_string_tonumber, NULL, false);
            }
            {
                tin_class_bindmethod(state, klass, "toUpperCase", objfn_string_touppercase);
                tin_class_bindmethod(state, klass, "toUpper", objfn_string_touppercase);
                tin_class_bindgetset(state, klass, "upper", objfn_string_touppercase, NULL, false);
            }
            {
                tin_class_bindmethod(state, klass, "toLowerCase", objfn_string_tolowercase);
                tin_class_bindmethod(state, klass, "toLower", objfn_string_tolowercase);
                tin_class_bindgetset(state, klass, "lower", objfn_string_tolowercase, NULL, false);
            }
            {
                tin_class_bindgetset(state, klass, "toByte", objfn_string_tobyte, NULL, false);
                tin_class_bindgetset(state, klass, "ord", objfn_string_tobyte, NULL, false);
            }
            tin_class_bindmethod(state, klass, "contains", objfn_string_contains);
            tin_class_bindmethod(state, klass, "startsWith", objfn_string_startswith);
            tin_class_bindmethod(state, klass, "endsWith", objfn_string_endswith);
            tin_class_bindmethod(state, klass, "replace", objfn_string_replace);
            tin_class_bindmethod(state, klass, "substring", objfn_string_substring);
            tin_class_bindmethod(state, klass, "iterator", objfn_string_iterator);
            tin_class_bindmethod(state, klass, "iteratorValue", objfn_string_iteratorvalue);
            tin_class_bindgetset(state, klass, "length", objfn_string_length, NULL, false);
            tin_class_bindmethod(state, klass, "format", objfn_string_format);
            state->primstringclass = klass;
        }
        tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
        if(klass->super == NULL)
        {
            tin_class_inheritfrom(state, klass, state->primobjectclass);
        };
    }
}

