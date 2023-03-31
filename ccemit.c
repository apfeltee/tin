
#include <string.h>
#include "priv.h"


#define TIN_CCEMIT_GROWCAPACITY(cap) \
    (((cap) < 8) ? (8) : ((cap) * 2))


static bool tin_astemit_emitexpression(TinAstEmitter* emt, TinAstExpression* expr);
static void tin_astemit_resolvestmt(TinAstEmitter* emt, TinAstExpression* stmt);

static inline void tin_uintlist_init(TinUintList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

static inline void tin_uintlist_destroy(TinState* state, TinUintList* array)
{
    tin_gcmem_freearray(state, sizeof(size_t), array->values, array->capacity);
    tin_uintlist_init(array);
}

static inline void tin_uintlist_push(TinState* state, TinUintList* array, size_t value)
{
    size_t oldcapacity;
    if(array->capacity < (array->count + 1))
    {
        oldcapacity = array->capacity;
        array->capacity = TIN_CCEMIT_GROWCAPACITY(oldcapacity);
        array->values = (size_t*)tin_gcmem_growarray(state, array->values, sizeof(size_t), oldcapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

static inline size_t tin_uintlist_get(TinUintList* array, size_t idx)
{
    return array->values[idx];
}

static inline size_t tin_uintlist_count(TinUintList* array)
{
    return array->count;
}

static void resolve_statements(TinAstEmitter* emt, TinAstExprList* statements)
{
    size_t i;
    for(i = 0; i < statements->count; i++)
    {
        tin_astemit_resolvestmt(emt, statements->values[i]);
    }
}

void tin_privlist_init(TinAstPrivList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void tin_privlist_destroy(TinState* state, TinAstPrivList* array)
{
    tin_gcmem_freearray(state, sizeof(TinAstPrivate), array->values, array->capacity);
    tin_privlist_init(array);
}

void tin_privlist_push(TinState* state, TinAstPrivList* array, TinAstPrivate value)
{
    size_t oldcapacity;
    if(array->capacity < array->count + 1)
    {
        oldcapacity = array->capacity;
        array->capacity = TIN_CCEMIT_GROWCAPACITY(oldcapacity);
        array->values = (TinAstPrivate*)tin_gcmem_growarray(state, array->values, sizeof(TinAstPrivate), oldcapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}
void tin_loclist_init(TinAstLocList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void tin_loclist_destroy(TinState* state, TinAstLocList* array)
{
    tin_gcmem_freearray(state, sizeof(TinAstLocal), array->values, array->capacity);
    tin_loclist_init(array);
}

void tin_loclist_push(TinState* state, TinAstLocList* array, TinAstLocal value)
{
    size_t oldcapacity;
    if(array->capacity < array->count + 1)
    {
        oldcapacity = array->capacity;
        array->capacity = TIN_CCEMIT_GROWCAPACITY(oldcapacity);
        array->values = (TinAstLocal*)tin_gcmem_growarray(state, array->values, sizeof(TinAstLocal), oldcapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

static void tin_astemit_raiseerror(TinAstEmitter* emt, size_t line, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    tin_state_raiseerror(emt->state, COMPILE_ERROR, tin_vformat_error(emt->state, line, fmt, args)->data);
    va_end(args);
}

void tin_astemit_init(TinState* state, TinAstEmitter* emt)
{
    emt->state = state;
    emt->loopstart = 0;
    emt->emitref = 0;
    emt->classname = NULL;
    emt->compiler = NULL;
    emt->chunk = NULL;
    emt->module = NULL;
    emt->prevwasexprstmt = false;
    emt->classisinheriting = false;
    tin_privlist_init(&emt->privates);
    tin_uintlist_init(&emt->breaks);
    tin_uintlist_init(&emt->continues);
}

void tin_astemit_destroy(TinAstEmitter* emt)
{
    tin_uintlist_destroy(emt->state, &emt->breaks);
    tin_uintlist_destroy(emt->state, &emt->continues);
}

static void tin_astemit_emit1byte(TinAstEmitter* emt, uint16_t line, uint8_t byte)
{
    if(line < emt->lastline)
    {
        // Egor-fail proofing
        line = emt->lastline;
    }
    tin_chunk_push(emt->state, emt->chunk, byte, line);
    emt->lastline = line;
}

static const int8_t stack_effects[] =
{
    /* OP_POP */ -1,
    /* OP_RETURN */ 0,
    /* OP_CONSTVALUE */ 1,
    /* OP_CONSTLONG */ 1,
    /* OP_VALTRUE */ 1,
    /* OP_VALFALSE */ 1,
    /* OP_VALNULL */ 1,
    /* OP_VALARRAY */ 1,
    /* OP_VALOBJECT */ 1,
    /* OP_RANGE */ -1,
    /* OP_NEGATE */ 0,
    /* OP_NOT */ 0,
    /* OP_MATHADD */ -1,
    /* OP_MATHSUB */ -1,
    /* OP_MATHMULT */ -1,
    /* OP_MATHPOWER */ -1,
    /* OP_MATHDIV */ -1,
    /* OP_MATHFLOORDIV */ -1,
    /* OP_MATHMOD */ -1,
    /* OP_BINAND */ -1,
    /* OP_BINOR */ -1,
    /* OP_BINXOR */ -1,
    /* OP_LEFTSHIFT */ -1,
    /* OP_RIGHTSHIFT */ -1,
    /* OP_BINNOT */ 0,
    /* OP_EQUAL */ -1,
    /* OP_GREATERTHAN */ -1,
    /* OP_GREATEREQUAL */ -1,
    /* OP_LESSTHAN */ -1,
    /* OP_LESSEQUAL */ -1,
    /* OP_GLOBALSET */ 0,
    /* OP_GLOBALGET */ 1,
    /* OP_LOCALSET */ 0,
    /* OP_LOCALGET */ 1,
    /* OP_LOCALLONGSET */ 0,
    /* OP_LOCALLONGGET */ 1,
    /* OP_PRIVATESET */ 0,
    /* OP_PRIVATEGET */ 1,
    /* OP_PRIVATELONGSET */ 0,
    /* OP_PRIVATELONGGET */ 1,
    /* OP_UPVALSET */ 0,
    /* OP_UPVALGET */ 1,
    /* OP_JUMPIFFALSE */ -1,
    /* OP_JUMPIFNULL */ 0,
    /* OP_JUMPIFNULLPOP */ -1,
    /* OP_JUMPALWAYS */ 0,
    /* OP_JUMPBACK */ 0,
    /* OP_AND */ -1,
    /* OP_OR */ -1,
    /* OP_NULLOR */ -1,
    /* OP_MAKECLOSURE */ 1,
    /* OP_UPVALCLOSE */ -1,
    /* OP_MAKECLASS */ 1,
    /* OP_FIELDGET */ -1,
    /* OP_FIELDSET */ -2,
    /* OP_GETINDEX */ -1,
    /* OP_SETINDEX */ -2,
    /* OP_ARRAYPUSHVALUE */ -1,
    /* OP_OBJECTPUSHFIELD */ -2,
    /* OP_MAKEMETHOD */ -1,
    /* OP_FIELDSTATIC */ -1,
    /* OP_FIELDDEFINE */ -1,
    /* OP_CLASSINHERIT */ 0,
    /* OP_ISCLASS */ -1,
    /* OP_GETSUPERMETHOD */ 0,
    /* OP_CALLFUNCTION */ 0,
    /* OP_INVOKEMETHOD */ 0,
    /* OP_INVOKESUPER */ 0,
    /* OP_INVOKEIGNORING */ 0,
    /* OP_INVOKESUPERIGNORING */ 0,
    /* OP_POPLOCALS */ 0,
    /* OP_VARARG */ 0,
    /* OP_REFGLOBAL */ 1,
    /* OP_REFPRIVATE */ 0,
    /* OP_REFLOCAL */ 1,
    /* OP_REFUPVAL */ 1,
    /* OP_REFFIELD */ -1,
    /* OP_REFSET */ -1,
};

static void tin_astemit_emit2bytes(TinAstEmitter* emt, uint16_t line, uint8_t a, uint8_t b)
{
    if(line < emt->lastline)
    {
        // Egor-fail proofing
        line = emt->lastline;
    }
    tin_chunk_push(emt->state, emt->chunk, a, line);
    tin_chunk_push(emt->state, emt->chunk, b, line);
    emt->lastline = line;
}

static void tin_astemit_emit1op(TinAstEmitter* emt, uint16_t line, TinOpCode op)
{
    TinAstCompiler* compiler;
    compiler = emt->compiler;
    tin_astemit_emit1byte(emt, line, (uint8_t)op);
    compiler->slots += stack_effects[(int)op];
    if(compiler->slots > (int)compiler->function->maxslots)
    {
        compiler->function->maxslots = (size_t)compiler->slots;
    }
}

static void tin_astemit_emit2ops(TinAstEmitter* emt, uint16_t line, TinOpCode a, TinOpCode b)
{
    TinAstCompiler* compiler;
    compiler = emt->compiler;
    tin_astemit_emit2bytes(emt, line, (uint8_t)a, (uint8_t)b);
    compiler->slots += stack_effects[(int)a] + stack_effects[(int)b];
    if(compiler->slots > (int)compiler->function->maxslots)
    {
        compiler->function->maxslots = (size_t)compiler->slots;
    }
}

static void tin_astemit_emitvaryingop(TinAstEmitter* emt, uint16_t line, TinOpCode op, uint8_t arg)
{
    TinAstCompiler* compiler;
    compiler = emt->compiler;
    tin_astemit_emit2bytes(emt, line, (uint8_t)op, arg);
    compiler->slots -= arg;
    if(compiler->slots > (int)compiler->function->maxslots)
    {
        compiler->function->maxslots = (size_t)compiler->slots;
    }
}

static void tin_astemit_emitargedop(TinAstEmitter* emt, uint16_t line, TinOpCode op, uint8_t arg)
{
    TinAstCompiler* compiler;
    compiler = emt->compiler;
    tin_astemit_emit2bytes(emt, line, (uint8_t)op, arg);
    compiler->slots += stack_effects[(int)op];
    if(compiler->slots > (int)compiler->function->maxslots)
    {
        compiler->function->maxslots = (size_t)compiler->slots;
    }
}

static void tin_astemit_emitshort(TinAstEmitter* emt, uint16_t line, uint16_t value)
{
    tin_astemit_emit2bytes(emt, line, (uint8_t)((value >> 8) & 0xff), (uint8_t)(value & 0xff));
}

static void tin_astemit_emitbyteorshort(TinAstEmitter* emt, uint16_t line, uint8_t a, uint8_t b, uint16_t index)
{
    if(index > UINT8_MAX)
    {
        tin_astemit_emit1op(emt, line, (TinOpCode)b);
        tin_astemit_emitshort(emt, line, (uint16_t)index);
    }
    else
    {
        tin_astemit_emitargedop(emt, line, (TinOpCode)a, (uint8_t)index);
    }
}

static inline TinAstLocal tin_compiler_makelocal(const char* name, size_t length, int depth, bool captured, bool constant)
{
    TinAstLocal tl;
    tl.name = name;
    tl.length = length;
    tl.depth = depth;
    tl.captured = captured;
    tl.constant = constant;
    return tl;
}

static void tin_compiler_compiler(TinAstEmitter* emt, TinAstCompiler* compiler, TinAstFuncType type)
{
    const char* name;
    tin_loclist_init(&compiler->locals);
    compiler->type = type;
    compiler->scope_depth = 0;
    compiler->enclosing = (struct TinAstCompiler*)emt->compiler;
    compiler->skipreturn = false;
    compiler->function = tin_object_makefunction(emt->state, emt->module);
    compiler->loopdepth = 0;
    emt->compiler = compiler;
    name = emt->state->scanner->filename;
    if(emt->compiler == NULL)
    {
        compiler->function->name = tin_string_copy(emt->state, name, strlen(name));
    }
    emt->chunk = &compiler->function->chunk;
    if(tin_astopt_isoptenabled(TINOPTSTATE_LINEINFO))
    {
        emt->chunk->has_line_info = false;
    }
    if(type == TINFUNC_METHOD || type == TINFUNC_STATICMETHOD || type == TINFUNC_CONSTRUCTOR)
    {
        tin_loclist_push(emt->state, &compiler->locals, tin_compiler_makelocal(TIN_VALUE_THISNAME, strlen(TIN_VALUE_THISNAME), -1, false, false));
    }
    else
    {
        tin_loclist_push(emt->state, &compiler->locals, tin_compiler_makelocal("", 0, -1, false, false));
    }
    compiler->slots = 1;
    compiler->maxslots = 1;
}

static void tin_astemit_emitreturn(TinAstEmitter* emt, size_t line)
{
    if(emt->compiler->type == TINFUNC_CONSTRUCTOR)
    {
        tin_astemit_emitargedop(emt, line, OP_LOCALGET, 0);
        tin_astemit_emit1op(emt, line, OP_RETURN);
    }
    else if(emt->prevwasexprstmt && emt->chunk->count > 0)
    {
        emt->chunk->count--;// Remove the OP_POP
        tin_astemit_emit1op(emt, line, OP_RETURN);
    }
    else
    {
        tin_astemit_emit2ops(emt, line, OP_VALNULL, OP_RETURN);
    }
}

static TinFunction* tin_compiler_end(TinAstEmitter* emt, TinString* name)
{
    TinFunction* function;
    if(!emt->compiler->skipreturn)
    {
        tin_astemit_emitreturn(emt, emt->lastline);
        emt->compiler->skipreturn = true;
    }
    function = emt->compiler->function;
    tin_loclist_destroy(emt->state, &emt->compiler->locals);
    emt->compiler = (TinAstCompiler*)emt->compiler->enclosing;
    emt->chunk = emt->compiler == NULL ? NULL : &emt->compiler->function->chunk;
    if(name != NULL)
    {
        function->name = name;
    }
#ifdef TIN_TRACE_CHUNK
    tin_disassemble_chunk(&function->chunk, function->name->data, NULL);
#endif
    return function;
}

static void tin_astemit_beginscope(TinAstEmitter* emt)
{
    emt->compiler->scope_depth++;
}

static void tin_astemit_endscope(TinAstEmitter* emt, uint16_t line)
{
    TinAstLocList* locals;
    TinAstCompiler* compiler;
    emt->compiler->scope_depth--;
    compiler = emt->compiler;
    locals = &compiler->locals;
    while(locals->count > 0 && locals->values[locals->count - 1].depth > compiler->scope_depth)
    {
        if(locals->values[locals->count - 1].captured)
        {
            tin_astemit_emit1op(emt, line, OP_UPVALCLOSE);
        }
        else
        {
            tin_astemit_emit1op(emt, line, OP_POP);
        }
        locals->count--;
    }
}

static uint16_t tin_astemit_addconstant(TinAstEmitter* emt, size_t line, TinValue value)
{
    size_t constant;
    constant = tin_chunk_addconst(emt->state, emt->chunk, value);
    if(constant >= UINT16_MAX)
    {
        tin_astemit_raiseerror(emt, line, "too many constants for one chunk");
    }
    return constant;
}

static size_t tin_astemit_emitconstant(TinAstEmitter* emt, size_t line, TinValue value)
{
    size_t constant;
    constant = tin_chunk_addconst(emt->state, emt->chunk, value);
    if(constant < UINT8_MAX)
    {
        tin_astemit_emitargedop(emt, line, OP_CONSTVALUE, constant);
    }
    else if(constant < UINT16_MAX)
    {
        tin_astemit_emit1op(emt, line, OP_CONSTLONG);
        tin_astemit_emitshort(emt, line, constant);
    }
    else
    {
        tin_astemit_raiseerror(emt, line, "too many constants for one chunk");
    }
    return constant;
}

static inline TinAstPrivate tin_astemit_makeprivate(bool initialized, bool constant)
{
    TinAstPrivate tv;
    tv.initialized = initialized;
    tv.constant = constant;
    return tv;
}

static int tin_astemit_addprivate(TinAstEmitter* emt, const char* name, size_t length, size_t line, bool constant)
{
    int index;
    TinValue idxval;
    TinState* state;
    TinString* key;
    TinTable* privnames;
    TinAstPrivList* privates;
    privates = &emt->privates;
    if(privates->count == UINT16_MAX)
    {
        tin_astemit_raiseerror(emt, line, "too many private locals for one module");
    }
    privnames = &emt->module->private_names->values;
    key = tin_table_findstring(privnames, name, length, tin_util_hashstring(name, length));
    if(key != NULL)
    {
        tin_astemit_raiseerror(emt, line, "variable '%.*s' was already declared in this scope", length, name);
        tin_table_get(privnames, key, &idxval);
        return tin_value_asnumber(idxval);
    }
    state = emt->state;
    index = (int)privates->count;
    tin_privlist_push(state, privates, tin_astemit_makeprivate(false, constant));
    tin_table_set(state, privnames, tin_string_copy(state, name, length), tin_value_makefixednumber(state, index));
    emt->module->private_count++;
    return index;
}

static int tin_astemit_resolveprivate(TinAstEmitter* emt, const char* name, size_t length, size_t line)
{
    int numberindex;
    TinValue index;
    TinString* key;
    TinTable* privnames;
    privnames = &emt->module->private_names->values;
    key = tin_table_findstring(privnames, name, length, tin_util_hashstring(name, length));
    if(key != NULL)
    {
        tin_table_get(privnames, key, &index);
        numberindex = tin_value_asnumber(index);
        if(!emt->privates.values[numberindex].initialized)
        {
            tin_astemit_raiseerror(emt, line, "variable '%.*s' cannot use itself in its initializer", length, name);
        }
        return numberindex;
    }
    return -1;
}

static int tin_astemit_addlocal(TinAstEmitter* emt, const char* name, size_t length, size_t line, bool constant)
{
    int i;
    TinAstLocal* local;
    TinAstLocList* locals;
    TinAstCompiler* compiler;
    compiler = emt->compiler;
    locals = &compiler->locals;
    if(locals->count == UINT16_MAX)
    {
        tin_astemit_raiseerror(emt, line, "too many local variables for one function");
    }
    for(i = (int)locals->count - 1; i >= 0; i--)
    {
        local = &locals->values[i];
        if(local->depth != UINT16_MAX && local->depth < compiler->scope_depth)
        {
            break;
        }
        if(length == local->length && memcmp(local->name, name, length) == 0)
        {
            tin_astemit_raiseerror(emt, line, "variable '%.*s' was already declared in this scope", length, name);
        }
    }
    tin_loclist_push(emt->state, locals, tin_compiler_makelocal(name, length, UINT16_MAX, false, constant));
    return (((int)locals->count) - 1);
}

static int tin_astemit_resolvelocal(TinAstEmitter* emt, TinAstCompiler* compiler, const char* name, size_t length, size_t line)
{
    int i;
    TinAstLocal* local;
    TinAstLocList* locals;
    locals = &compiler->locals;
    for(i = (int)locals->count - 1; i >= 0; i--)
    {
        local = &locals->values[i];

        if(local->length == length && memcmp(local->name, name, length) == 0)
        {
            if(local->depth == UINT16_MAX)
            {
                tin_astemit_raiseerror(emt, line, "variable '%.*s' cannot use itself in its initializer", length, name);
            }

            return i;
        }
    }

    return -1;
}

static int tin_astemit_addupvalue(TinAstEmitter* emt, TinAstCompiler* compiler, uint8_t index, size_t line, bool islocal)
{
    size_t i;
    size_t upvalcnt;
    TinAstCompUpvalue* upvalue;
    upvalcnt = compiler->function->upvalue_count;
    for(i= 0; i < upvalcnt; i++)
    {
        upvalue = &compiler->upvalues[i];
        if(upvalue->index == index && upvalue->isLocal == islocal)
        {
            return i;
        }
    }
    if(upvalcnt == UINT16_COUNT)
    {
        tin_astemit_raiseerror(emt, line, "too many upvalues for one function");
        return 0;
    }
    compiler->upvalues[upvalcnt].isLocal = islocal;
    compiler->upvalues[upvalcnt].index = index;
    return compiler->function->upvalue_count++;
}

static int tin_astemit_resolveupvalue(TinAstEmitter* emt, TinAstCompiler* compiler, const char* name, size_t length, size_t line)
{
    int local;
    int upvalue;
    if(compiler->enclosing == NULL)
    {
        return -1;
    }
    local = tin_astemit_resolvelocal(emt, (TinAstCompiler*)compiler->enclosing, name, length, line);
    if(local != -1)
    {
        ((TinAstCompiler*)compiler->enclosing)->locals.values[local].captured = true;
        return tin_astemit_addupvalue(emt, compiler, (uint8_t)local, line, true);
    }
    upvalue = tin_astemit_resolveupvalue(emt, (TinAstCompiler*)compiler->enclosing, name, length, line);
    if(upvalue != -1)
    {
        return tin_astemit_addupvalue(emt, compiler, (uint8_t)upvalue, line, false);
    }
    return -1;
}

static void tin_astemit_marklocalinit(TinAstEmitter* emt, size_t index)
{
    emt->compiler->locals.values[index].depth = emt->compiler->scope_depth;
}

static void tin_astemit_markprivateinit(TinAstEmitter* emt, size_t index)
{
    emt->privates.values[index].initialized = true;
}

static size_t tin_astemit_emitjump(TinAstEmitter* emt, TinOpCode code, size_t line)
{
    tin_astemit_emit1op(emt, line, code);
    tin_astemit_emit2bytes(emt, line, 0xff, 0xff);
    return emt->chunk->count - 2;
}

static void tin_astemit_patchjump(TinAstEmitter* emt, size_t offset, size_t line)
{
    size_t jump;
    jump = emt->chunk->count - offset - 2;
    if(jump > UINT16_MAX)
    {
        tin_astemit_raiseerror(emt, line, "too much code to jump over");
    }
    emt->chunk->code[offset] = (jump >> 8) & 0xff;
    emt->chunk->code[offset + 1] = jump & 0xff;
}

static void tin_astemit_emitloop(TinAstEmitter* emt, size_t start, size_t line)
{
    size_t offset;
    tin_astemit_emit1op(emt, line, OP_JUMPBACK);
    offset = emt->chunk->count - start + 2;
    if(offset > UINT16_MAX)
    {
        tin_astemit_raiseerror(emt, line, "too much code to jump over");
    }
    tin_astemit_emitshort(emt, line, offset);
}

static void tin_astemit_patchloopjumps(TinAstEmitter* emt, TinUintList* breaks, size_t line)
{
    size_t i;
    for(i = 0; i < tin_uintlist_count(breaks); i++)
    {
        tin_astemit_patchjump(emt, tin_uintlist_get(breaks, i), line);
    }
    tin_uintlist_destroy(emt->state, breaks);
}

static bool tin_astemit_emitparamlist(TinAstEmitter* emt, TinAstParamList* parameters, size_t line)
{
    size_t i;
    size_t jump;
    int index;
    TinAstParameter* parameter;
    for(i = 0; i < parameters->count; i++)
    {
        parameter = &parameters->values[i];
        index = tin_astemit_addlocal(emt, parameter->name, parameter->length, line, false);
        tin_astemit_marklocalinit(emt, index);
        // Vararg ...
        if(parameter->length == 3 && memcmp(parameter->name, "...", 3) == 0)
        {
            return true;
        }
        if(parameter->defaultexpr != NULL)
        {
            tin_astemit_emitbyteorshort(emt, line, OP_LOCALGET, OP_LOCALLONGGET, index);
            jump = tin_astemit_emitjump(emt, OP_NULLOR, line);
            tin_astemit_emitexpression(emt, parameter->defaultexpr);
            tin_astemit_patchjump(emt, jump, line);
            tin_astemit_emitbyteorshort(emt, line, OP_LOCALSET, OP_LOCALLONGSET, index);
            tin_astemit_emit1op(emt, line, OP_POP);
        }
    }
    return false;
}

static void tin_astemit_resolvestmt(TinAstEmitter* emt, TinAstExpression* stmt)
{
    TinAstFunctionExpr* funcstmt;
    TinAstAssignVarExpr* varstmt;
    switch(stmt->type)
    {
        case TINEXPR_VARSTMT:
            {
                varstmt = (TinAstAssignVarExpr*)stmt;
                tin_astemit_markprivateinit(emt, tin_astemit_addprivate(emt, varstmt->name, varstmt->length, stmt->line, varstmt->constant));
            }
            break;
        case TINEXPR_FUNCTION:
            {
                funcstmt = (TinAstFunctionExpr*)stmt;
                if(!funcstmt->exported)
                {
                    tin_astemit_markprivateinit(emt, tin_astemit_addprivate(emt, funcstmt->name, funcstmt->length, stmt->line, false));
                }
            }
            break;
        case TINEXPR_CLASS:
        case TINEXPR_BLOCK:
        case TINEXPR_FOR:
        case TINEXPR_WHILE:
        case TINEXPR_IFSTMT:
        case TINEXPR_CONTINUE:
        case TINEXPR_BREAK:
        case TINEXPR_RETURN:
        case TINEXPR_METHOD:
        case TINEXPR_FIELD:
        case TINEXPR_EXPRESSION:
            {
            }
            break;
        default:
            {
                
            }
            break;
    }
}

static bool tin_astemit_doemitliteral(TinAstEmitter* emt, TinAstExpression* expr)
{
    TinValue value;
    value = ((TinAstLiteralExpr*)expr)->value;
    if(tin_value_isnumber(value) || tin_value_isstring(value))
    {
        tin_astemit_emitconstant(emt, expr->line, value);
    }
    else if(tin_value_isbool(value))
    {
        tin_astemit_emit1op(emt, expr->line, tin_value_asbool(value) ? OP_VALTRUE : OP_VALFALSE);
    }
    else if(tin_value_isnull(value))
    {
        tin_astemit_emit1op(emt, expr->line, OP_VALNULL);
    }
    else
    {
        assert(!"missing or invalid emit instruction for literal");
    }
    return true;
}

static bool tin_astemit_doemitbinary(TinAstEmitter* emt, TinAstExpression* expr)
{
    size_t jump;
    TinAstTokType op;
    TinAstBinaryExpr* binexpr;
    binexpr = (TinAstBinaryExpr*)expr;
    tin_astemit_emitexpression(emt, binexpr->left);
    if(binexpr->right == NULL)
    {
        return true;
    }
    op = binexpr->op;
    if(op == TINTOK_DOUBLEAMPERSAND || op == TINTOK_DOUBLEBAR || op == TINTOK_DOUBLEQUESTION)
    {
        //jump = tin_astemit_emitjump(emt, op == TINTOK_DOUBLEBAR ? OP_OR : (op == TINTOK_DOUBLEQUESTION ? OP_NULLOR : OP_AND), emt->lastline);
        jump = 0;
        if(op == TINTOK_DOUBLEBAR)
        {
            jump = tin_astemit_emitjump(emt, OP_OR, emt->lastline);
        }
        else if(op == TINTOK_DOUBLEQUESTION)
        {
            jump = tin_astemit_emitjump(emt, OP_NULLOR, emt->lastline);
        }
        else
        {
            jump = tin_astemit_emitjump(emt, OP_AND, emt->lastline);
        }
        tin_astemit_emitexpression(emt, binexpr->right);
        tin_astemit_patchjump(emt, jump, emt->lastline);
        return true;
    }
    tin_astemit_emitexpression(emt, binexpr->right);
    switch(op)
    {
        case TINTOK_PLUS:
            {
                tin_astemit_emit1op(emt, expr->line, OP_MATHADD);
            }
            break;
        case TINTOK_MINUS:
            {
                tin_astemit_emit1op(emt, expr->line, OP_MATHSUB);
            }
            break;
        case TINTOK_STAR:
            {
                tin_astemit_emit1op(emt, expr->line, OP_MATHMULT);
            }
            break;
        case TINTOK_DOUBLESTAR:
            {
                tin_astemit_emit1op(emt, expr->line, OP_MATHPOWER);
            }
            break;
        case TINTOK_SLASH:
            {
                tin_astemit_emit1op(emt, expr->line, OP_MATHDIV);
            }
            break;
        case TINTOK_SHARP:
            {
                tin_astemit_emit1op(emt, expr->line, OP_MATHFLOORDIV);
            }
            break;
        case TINTOK_PERCENT:
            {
                tin_astemit_emit1op(emt, expr->line, OP_MATHMOD);
            }
            break;
        case TINTOK_KWIS:
            {
                tin_astemit_emit1op(emt, expr->line, OP_ISCLASS);
            }
            break;
        case TINTOK_EQUAL:
            {
                tin_astemit_emit1op(emt, expr->line, OP_EQUAL);
            }
            break;
        case TINTOK_BANGEQUAL:
            {
                tin_astemit_emit2ops(emt, expr->line, OP_EQUAL, OP_NOT);
            }
            break;
        case TINTOK_GREATERTHAN:
            {
                tin_astemit_emit1op(emt, expr->line, OP_GREATERTHAN);
            }
            break;
        case TINTOK_GREATEREQUAL:
            {
                tin_astemit_emit1op(emt, expr->line, OP_GREATEREQUAL);
            }
            break;
        case TINTOK_LESSTHAN:
            {
                tin_astemit_emit1op(emt, expr->line, OP_LESSTHAN);
            }
            break;
        case TINTOK_LESSEQUAL:
            {
                tin_astemit_emit1op(emt, expr->line, OP_LESSEQUAL);
            }
            break;
        case TINTOK_SHIFTLEFT:
            {
                tin_astemit_emit1op(emt, expr->line, OP_LEFTSHIFT);
            }
            break;
        case TINTOK_SHIFTRIGHT:
            {
                tin_astemit_emit1op(emt, expr->line, OP_RIGHTSHIFT);
            }
            break;
        case TINTOK_BAR:
            {
                tin_astemit_emit1op(emt, expr->line, OP_BINOR);
            }
            break;
        case TINTOK_AMPERSAND:
            {
                tin_astemit_emit1op(emt, expr->line, OP_BINAND);
            }
            break;
        case TINTOK_CARET:
            {
                tin_astemit_emit1op(emt, expr->line, OP_BINXOR);
            }
            break;
        default:
            {
                fprintf(stderr, "in tin_astemit_emitexpression: binary expression #2 is NULL! might be a bug\n");
            }
        break;
    }
    return true;
}

static bool tin_astemit_doemitunary(TinAstEmitter* emt, TinAstExpression* expr)
{
    TinAstUnaryExpr* unexpr;
    unexpr = (TinAstUnaryExpr*)expr;
    tin_astemit_emitexpression(emt, unexpr->right);
    switch(unexpr->op)
    {
        case TINTOK_MINUS:
            {
                tin_astemit_emit1op(emt, expr->line, OP_NEGATE);
            }
            break;
        case TINTOK_BANG:
            {
                tin_astemit_emit1op(emt, expr->line, OP_NOT);
            }
            break;
        case TINTOK_TILDE:
            {
                tin_astemit_emit1op(emt, expr->line, OP_BINNOT);
            }
            break;
        default:
            {
                fprintf(stderr, "in tin_astemit_emitexpression: unary expr is NULL! might be an internal bug\n");
            }
            break;
    }
    return true;
}

static bool tin_astemit_doemitvarexpr(TinAstEmitter* emt, TinAstExpression* expr)
{
    int index;
    bool ref;
    TinAstVarExpr* varexpr;
    varexpr = (TinAstVarExpr*)expr;
    ref = emt->emitref > 0;
    if(ref)
    {
        emt->emitref--;
    }
    index = tin_astemit_resolvelocal(emt, emt->compiler, varexpr->name, varexpr->length, expr->line);
    if(index == -1)
    {
        index = tin_astemit_resolveupvalue(emt, emt->compiler, varexpr->name, varexpr->length, expr->line);
        if(index == -1)
        {
            index = tin_astemit_resolveprivate(emt, varexpr->name, varexpr->length, expr->line);
            if(index == -1)
            {
                tin_astemit_emit1op(emt, expr->line, ref ? OP_REFGLOBAL : OP_GLOBALGET);
                tin_astemit_emitshort(emt, expr->line,
                           tin_astemit_addconstant(emt, expr->line,
                                        tin_value_fromobject(tin_string_copy(emt->state, varexpr->name, varexpr->length))));
            }
            else
            {
                if(ref)
                {
                    tin_astemit_emit1op(emt, expr->line, OP_REFPRIVATE);
                    tin_astemit_emitshort(emt, expr->line, index);
                }
                else
                {
                    tin_astemit_emitbyteorshort(emt, expr->line, OP_PRIVATEGET, OP_PRIVATELONGGET, index);
                }
            }
        }
        else
        {
            tin_astemit_emitargedop(emt, expr->line, ref ? OP_REFUPVAL : OP_UPVALGET, (uint8_t)index);
        }
    }
    else
    {
        if(ref)
        {
            tin_astemit_emit1op(emt, expr->line, OP_REFLOCAL);
            tin_astemit_emitshort(emt, expr->line, index);
        }
        else
        {
            tin_astemit_emitbyteorshort(emt, expr->line, OP_LOCALGET, OP_LOCALLONGGET, index);
        }
    }
    return true;
}

static bool tin_astemit_doemitassign(TinAstEmitter* emt, TinAstExpression* expr)
{
    int index;
    TinAstVarExpr* e;
    TinAstGetExpr* getexpr;
    TinAstIndexExpr* indexexpr;
    TinAstAssignExpr* assignexpr;
    assignexpr = (TinAstAssignExpr*)expr;
    if(assignexpr->to->type == TINEXPR_VAREXPR)
    {
        tin_astemit_emitexpression(emt, assignexpr->value);
        e = (TinAstVarExpr*)assignexpr->to;
        index = tin_astemit_resolvelocal(emt, emt->compiler, e->name, e->length, assignexpr->to->line);
        if(index == -1)
        {
            index = tin_astemit_resolveupvalue(emt, emt->compiler, e->name, e->length, assignexpr->to->line);
            if(index == -1)
            {
                index = tin_astemit_resolveprivate(emt, e->name, e->length, assignexpr->to->line);
                if(index == -1)
                {
                    tin_astemit_emit1op(emt, expr->line, OP_GLOBALSET);
                    tin_astemit_emitshort(emt, expr->line,
                               tin_astemit_addconstant(emt, expr->line,
                                            tin_value_fromobject(tin_string_copy(emt->state, e->name, e->length))));
                }
                else
                {
                    if(emt->privates.values[index].constant)
                    {
                        tin_astemit_raiseerror(emt, expr->line, "attempt to modify constant '%.*s'", e->length, e->name);
                    }
                    tin_astemit_emitbyteorshort(emt, expr->line, OP_PRIVATESET, OP_PRIVATELONGSET, index);
                }
            }
            else
            {
                tin_astemit_emitargedop(emt, expr->line, OP_UPVALSET, (uint8_t)index);
            }
            return true;
        }
        else
        {
            if(emt->compiler->locals.values[index].constant)
            {
                tin_astemit_raiseerror(emt, expr->line, "attempt to modify constant '%.*s'", e->length, e->name);
            }

            tin_astemit_emitbyteorshort(emt, expr->line, OP_LOCALSET, OP_LOCALLONGSET, index);
        }
    }
    else if(assignexpr->to->type == TINEXPR_GET)
    {
        tin_astemit_emitexpression(emt, assignexpr->value);
        getexpr = (TinAstGetExpr*)assignexpr->to;
        tin_astemit_emitexpression(emt, getexpr->where);
        tin_astemit_emitexpression(emt, assignexpr->value);
        tin_astemit_emitconstant(emt, emt->lastline, tin_value_fromobject(tin_string_copy(emt->state, getexpr->name, getexpr->length)));
        tin_astemit_emit2ops(emt, emt->lastline, OP_FIELDSET, OP_POP);
    }
    else if(assignexpr->to->type == TINEXPR_SUBSCRIPT)
    {
        indexexpr = (TinAstIndexExpr*)assignexpr->to;
        tin_astemit_emitexpression(emt, indexexpr->array);
        tin_astemit_emitexpression(emt, indexexpr->index);
        tin_astemit_emitexpression(emt, assignexpr->value);
        tin_astemit_emit1op(emt, emt->lastline, OP_SETINDEX);
    }
    else if(assignexpr->to->type == TINEXPR_REFERENCE)
    {
        tin_astemit_emitexpression(emt, assignexpr->value);
        tin_astemit_emitexpression(emt, ((TinAstRefExpr*)assignexpr->to)->to);
        tin_astemit_emit1op(emt, expr->line, OP_REFSET);
    }
    else
    {
        tin_astemit_raiseerror(emt, expr->line, "invalid assigment target");
    }
    return true;
}

static bool tin_astemit_doemitcall(TinAstEmitter* emt, TinAstExpression* expr)
{
    size_t i;
    uint8_t index;
    bool issuper;
    bool ismethod;
    TinString* name;
    TinAstExpression* e;
    TinAstVarExpr* ee;
    TinAstObjectExpr* init;
    TinAstCallExpr* callexpr;
    TinAstSuperExpr* superexpr;
    TinAstExpression* get;
    TinAstGetExpr* getexpr;
    TinAstGetExpr* getter;
    name = NULL;
    callexpr = (TinAstCallExpr*)expr;
    ismethod = callexpr->callee->type == TINEXPR_GET;
    issuper = callexpr->callee->type == TINEXPR_SUPER;
    if(ismethod)
    {
        ((TinAstGetExpr*)callexpr->callee)->ignemit = true;
    }
    else if(issuper)
    {
        ((TinAstSuperExpr*)callexpr->callee)->ignemit = true;
    }
    tin_astemit_emitexpression(emt, callexpr->callee);
    if(issuper)
    {
        tin_astemit_emitargedop(emt, expr->line, OP_LOCALGET, 0);
    }
    for(i = 0; i < callexpr->args.count; i++)
    {
        e = callexpr->args.values[i];
        if(e->type == TINEXPR_VAREXPR)
        {
            ee = (TinAstVarExpr*)e;
            // Vararg ...
            if(ee->length == 3 && memcmp(ee->name, "...", 3) == 0)
            {
                tin_astemit_emitargedop(emt, e->line, OP_VARARG,
                              tin_astemit_resolvelocal(emt, emt->compiler, "...", 3, expr->line));
                break;
            }
        }
        tin_astemit_emitexpression(emt, e);
    }
    if(ismethod || issuper)
    {
        if(ismethod)
        {
            getexpr = (TinAstGetExpr*)callexpr->callee;
            tin_astemit_emitvaryingop(emt, expr->line,
                            ((TinAstGetExpr*)callexpr->callee)->ignresult ? OP_INVOKEIGNORING : OP_INVOKEMETHOD,
                            (uint8_t)callexpr->args.count);
            tin_astemit_emitshort(emt, emt->lastline,
                       tin_astemit_addconstant(emt, emt->lastline,
                                    tin_value_fromobject(tin_string_copy(emt->state, getexpr->name, getexpr->length))));
        }
        else
        {
            superexpr = (TinAstSuperExpr*)callexpr->callee;
            index = tin_astemit_resolveupvalue(emt, emt->compiler, TIN_VALUE_SUPERNAME, strlen(TIN_VALUE_SUPERNAME), emt->lastline);
            tin_astemit_emitargedop(emt, expr->line, OP_UPVALGET, index);
            tin_astemit_emitvaryingop(emt, emt->lastline,
                            ((TinAstSuperExpr*)callexpr->callee)->ignresult ? OP_INVOKESUPERIGNORING : OP_INVOKESUPER,
                            (uint8_t)callexpr->args.count);
            tin_astemit_emitshort(emt, emt->lastline, tin_astemit_addconstant(emt, emt->lastline, tin_value_fromobject(superexpr->method)));
        }
    }
    else
    {
        name = callexpr->name;
        tin_astemit_emitvaryingop(emt, expr->line, OP_CALLFUNCTION, (uint8_t)callexpr->args.count);
        assert(name);
        //tin_astemit_emitconstant(emt, expr->line, tin_value_fromobject(name));
        tin_astemit_emitshort(emt, expr->line, tin_astemit_addconstant(emt, expr->line, tin_value_fromobject(name)));
    }
    if(ismethod)
    {
        get = callexpr->callee;
        while(get != NULL)
        {
            if(get->type == TINEXPR_GET)
            {
                getter = (TinAstGetExpr*)get;
                if(getter->jump > 0)
                {
                    tin_astemit_patchjump(emt, getter->jump, emt->lastline);
                }
                get = getter->where;
            }
            else if(get->type == TINEXPR_SUBSCRIPT)
            {
                get = ((TinAstIndexExpr*)get)->array;
            }
            else
            {
                break;
            }
        }
    }
    if(callexpr->init == NULL)
    {
        return false;
    }
    init = (TinAstObjectExpr*)callexpr->init;
    for(i = 0; i < init->values.count; i++)
    {
        e = init->values.values[i];
        emt->lastline = e->line;
        tin_astemit_emitexpression(emt, init->keys.values[i]);
        tin_astemit_emitexpression(emt, e);
        tin_astemit_emit1op(emt, emt->lastline, OP_OBJECTPUSHFIELD);
    }
    return true;
}

static bool tin_astemit_doemitget(TinAstEmitter* emt, TinAstExpression* expr)
{
    bool ref;
    TinAstGetExpr* getexpr;
    getexpr = (TinAstGetExpr*)expr;
    ref = emt->emitref > 0;
    if(ref)
    {
        emt->emitref--;
    }
    tin_astemit_emitexpression(emt, getexpr->where);
    if(getexpr->jump == 0)
    {
        getexpr->jump = tin_astemit_emitjump(emt, OP_JUMPIFNULL, emt->lastline);
        if(!getexpr->ignemit)
        {
            tin_astemit_emitconstant(emt, emt->lastline,
                          tin_value_fromobject(tin_string_copy(emt->state, getexpr->name, getexpr->length)));
            tin_astemit_emit1op(emt, emt->lastline, ref ? OP_REFFIELD : OP_FIELDGET);
        }
        tin_astemit_patchjump(emt, getexpr->jump, emt->lastline);
    }
    else if(!getexpr->ignemit)
    {
        tin_astemit_emitconstant(emt, emt->lastline, tin_value_fromobject(tin_string_copy(emt->state, getexpr->name, getexpr->length)));
        tin_astemit_emit1op(emt, emt->lastline, ref ? OP_REFFIELD : OP_FIELDGET);
    }
    return true;
}

static bool tin_astemit_doemitset(TinAstEmitter* emt, TinAstExpression* expr)
{
    TinAstSetExpr* setexpr;
    setexpr = (TinAstSetExpr*)expr;
    tin_astemit_emitexpression(emt, setexpr->where);
    tin_astemit_emitexpression(emt, setexpr->value);
    tin_astemit_emitconstant(emt, emt->lastline, tin_value_fromobject(tin_string_copy(emt->state, setexpr->name, setexpr->length)));
    tin_astemit_emit1op(emt, emt->lastline, OP_FIELDSET);
    return true;
}

static bool tin_astemit_doemitlambda(TinAstEmitter* emt, TinAstExpression* expr)
{
    size_t i;
    bool vararg;
    bool singleexpr;
    TinAstCompiler compiler;
    TinString* name;
    TinFunction* function;
    TinAstFunctionExpr* lambdaexpr;
    lambdaexpr = (TinAstFunctionExpr*)expr;
    name = tin_value_asstring(tin_string_format(emt->state,
        "lambda @:@", tin_value_fromobject(emt->module->name), tin_string_numbertostring(emt->state, expr->line)));
    tin_compiler_compiler(emt, &compiler, TINFUNC_REGULAR);
    tin_astemit_beginscope(emt);
    vararg = tin_astemit_emitparamlist(emt, &lambdaexpr->parameters, expr->line);
    if(lambdaexpr->body != NULL)
    {
        singleexpr = lambdaexpr->body->type == TINEXPR_EXPRESSION;
        if(singleexpr)
        {
            compiler.skipreturn = true;
            ((TinAstExprExpr*)lambdaexpr->body)->pop = false;
        }
        tin_astemit_emitexpression(emt, lambdaexpr->body);
        if(singleexpr)
        {
            tin_astemit_emit1op(emt, emt->lastline, OP_RETURN);
        }
    }
    tin_astemit_endscope(emt, emt->lastline);
    function = tin_compiler_end(emt, name);
    function->arg_count = lambdaexpr->parameters.count;
    function->maxslots += function->arg_count;
    function->vararg = vararg;
    if(function->upvalue_count > 0)
    {
        tin_astemit_emit1op(emt, emt->lastline, OP_MAKECLOSURE);
        tin_astemit_emitshort(emt, emt->lastline, tin_astemit_addconstant(emt, emt->lastline, tin_value_fromobject(function)));
        for(i = 0; i < function->upvalue_count; i++)
        {
            tin_astemit_emit2bytes(emt, emt->lastline, compiler.upvalues[i].isLocal ? 1 : 0, compiler.upvalues[i].index);
        }
    }
    else
    {
        tin_astemit_emitconstant(emt, emt->lastline, tin_value_fromobject(function));
    }
    return true;
}

static bool tin_astemit_doemitarray(TinAstEmitter* emt, TinAstExpression* expr)
{
    size_t i;
    TinAstArrayExpr* arrexpr;
    arrexpr = (TinAstArrayExpr*)expr;
    tin_astemit_emit1op(emt, expr->line, OP_VALARRAY);
    for(i = 0; i < arrexpr->values.count; i++)
    {
        tin_astemit_emitexpression(emt, arrexpr->values.values[i]);
        tin_astemit_emit1op(emt, emt->lastline, OP_ARRAYPUSHVALUE);
    }
    return true;
}

static bool tin_astemit_doemitobject(TinAstEmitter* emt, TinAstExpression* expr)
{
    size_t i;
    TinAstObjectExpr* objexpr;
    objexpr = (TinAstObjectExpr*)expr;
    tin_astemit_emit1op(emt, expr->line, OP_VALOBJECT);
    for(i = 0; i < objexpr->values.count; i++)
    {
        tin_astemit_emitexpression(emt, objexpr->keys.values[i]);
        tin_astemit_emitexpression(emt, objexpr->values.values[i]);
        tin_astemit_emit1op(emt, emt->lastline, OP_OBJECTPUSHFIELD);
    }
    return true;
}

static bool tin_astemit_doemitthis(TinAstEmitter* emt, TinAstExpression* expr)
{
    int local;
    TinAstFuncType type;
    type = emt->compiler->type;
    if(type == TINFUNC_STATICMETHOD)
    {
        tin_astemit_raiseerror(emt, expr->line, "'this' cannot be used %s", "in static methods");
    }
    if(type == TINFUNC_CONSTRUCTOR || type == TINFUNC_METHOD)
    {
        tin_astemit_emitargedop(emt, expr->line, OP_LOCALGET, 0);
    }
    else
    {
        if(emt->compiler->enclosing == NULL)
        {
            tin_astemit_raiseerror(emt, expr->line, "'this' cannot be used %s", "in functions outside of any class");
        }
        else
        {
            local = tin_astemit_resolvelocal(emt, (TinAstCompiler*)emt->compiler->enclosing, TIN_VALUE_THISNAME, strlen(TIN_VALUE_THISNAME), expr->line);
            tin_astemit_emitargedop(emt, expr->line, OP_UPVALGET,
                          tin_astemit_addupvalue(emt, emt->compiler, local, expr->line, true));
        }
    }
    return true;
}

static bool tin_astemit_doemitsuper(TinAstEmitter* emt, TinAstExpression* expr)
{
    uint8_t index;
    TinAstSuperExpr* superexpr;
    if(emt->compiler->type == TINFUNC_STATICMETHOD)
    {
        tin_astemit_raiseerror(emt, expr->line, "'super' cannot be used %s", "in static methods");
    }
    else if(!emt->classisinheriting)
    {
        tin_astemit_raiseerror(emt, expr->line, "'super' cannot be used in class '%s', because it does not have a super class", emt->classname->data);
    }
    superexpr = (TinAstSuperExpr*)expr;
    if(!superexpr->ignemit)
    {
        index = tin_astemit_resolveupvalue(emt, emt->compiler, TIN_VALUE_SUPERNAME, strlen(TIN_VALUE_SUPERNAME), emt->lastline);
        tin_astemit_emitargedop(emt, expr->line, OP_LOCALGET, 0);
        tin_astemit_emitargedop(emt, expr->line, OP_UPVALGET, index);
        tin_astemit_emit1op(emt, expr->line, OP_GETSUPERMETHOD);
        tin_astemit_emitshort(emt, expr->line, tin_astemit_addconstant(emt, expr->line, tin_value_fromobject(superexpr->method)));
    }
    return true;
}

static bool tin_astemit_doemitternary(TinAstEmitter* emt, TinAstExpression* expr)
{
    uint64_t endjump;
    uint64_t elsejump;
    TinAstTernaryExpr* ifexpr;
    ifexpr = (TinAstTernaryExpr*)expr;
    tin_astemit_emitexpression(emt, ifexpr->condition);
    elsejump = tin_astemit_emitjump(emt, OP_JUMPIFFALSE, expr->line);
    tin_astemit_emitexpression(emt, ifexpr->ifbranch);
    endjump = tin_astemit_emitjump(emt, OP_JUMPALWAYS, emt->lastline);
    tin_astemit_patchjump(emt, elsejump, ifexpr->elsebranch->line);
    tin_astemit_emitexpression(emt, ifexpr->elsebranch);
    tin_astemit_patchjump(emt, endjump, emt->lastline);
    return true;
}

static bool tin_astemit_doemitinterpolation(TinAstEmitter* emt, TinAstExpression* expr)
{
    size_t i;
    TinAstStrInterExpr* ifexpr;
    ifexpr = (TinAstStrInterExpr*)expr;
    tin_astemit_emit1op(emt, expr->line, OP_VALARRAY);
    for(i = 0; i < ifexpr->expressions.count; i++)
    {
        tin_astemit_emitexpression(emt, ifexpr->expressions.values[i]);
        tin_astemit_emit1op(emt, emt->lastline, OP_ARRAYPUSHVALUE);
    }
    tin_astemit_emitvaryingop(emt, emt->lastline, OP_INVOKEMETHOD, 0);
    tin_astemit_emitshort(emt, emt->lastline,
               tin_astemit_addconstant(emt, emt->lastline, tin_value_makestring(emt->state, "join")));
    return true;
}

static bool tin_astemit_doemitreference(TinAstEmitter* emt, TinAstExpression* expr)
{
    int old;
    TinAstExpression* to;
    to = ((TinAstRefExpr*)expr)->to;
    if(to->type != TINEXPR_VAREXPR && to->type != TINEXPR_GET && to->type != TINEXPR_THIS && to->type != TINEXPR_SUPER)
    {
        tin_astemit_raiseerror(emt, expr->line, "invalid refence target");
        return false;
    }
    old = emt->emitref;
    emt->emitref++;
    tin_astemit_emitexpression(emt, to);
    emt->emitref = old;
    return true;
}

static bool tin_astemit_doemitvarstmt(TinAstEmitter* emt, TinAstExpression* expr)
{
    bool isprivate;
    int index;
    size_t line;
    TinAstAssignVarExpr* varstmt;
    varstmt = (TinAstAssignVarExpr*)expr;
    line = expr->line;
    isprivate = emt->compiler->enclosing == NULL && emt->compiler->scope_depth == 0;
    index = 0;
    if(isprivate)
    {
        index = tin_astemit_resolveprivate(emt, varstmt->name, varstmt->length, expr->line);
    }
    else
    {
        index = tin_astemit_addlocal(emt, varstmt->name, varstmt->length, expr->line, varstmt->constant);
    }
    if(varstmt->init == NULL)
    {
        tin_astemit_emit1op(emt, line, OP_VALNULL);
    }
    else
    {
        tin_astemit_emitexpression(emt, varstmt->init);
    }
    if(isprivate)
    {
        tin_astemit_markprivateinit(emt, index);
    }
    else
    {
        tin_astemit_marklocalinit(emt, index);
    }
    tin_astemit_emitbyteorshort(emt, expr->line, isprivate ? OP_PRIVATESET : OP_LOCALSET,
                       isprivate ? OP_PRIVATELONGSET : OP_LOCALLONGSET, index);
    if(isprivate)
    {
        // Privates don't live on stack, so we need to pop them manually
        tin_astemit_emit1op(emt, expr->line, OP_POP);
    }
    return true;
}

static bool tin_astemit_doemitifstmt(TinAstEmitter* emt, TinAstExpression* expr)
{
    size_t i;
    size_t elsejump;
    size_t endjump;
    uint64_t* endjumps;
    TinAstExpression* e;
    TinAstIfExpr* ifstmt;
    ifstmt = (TinAstIfExpr*)expr;
    elsejump = 0;
    endjump = 0;
    if(ifstmt->condition == NULL)
    {
        elsejump = tin_astemit_emitjump(emt, OP_JUMPALWAYS, expr->line);
    }
    else
    {
        tin_astemit_emitexpression(emt, ifstmt->condition);
        elsejump = tin_astemit_emitjump(emt, OP_JUMPIFFALSE, expr->line);
        tin_astemit_emitexpression(emt, ifstmt->ifbranch);
        endjump = tin_astemit_emitjump(emt, OP_JUMPALWAYS, emt->lastline);
    }
    /* important: endjumps must be N*sizeof(uint64_t) - merely allocating N isn't enough! */
    //uint64_t endjumps[ifstmt->elseifbranches == NULL ? 1 : ifstmt->elseifbranches->count];
    endjumps = (uint64_t*)malloc(sizeof(uint64_t) * (ifstmt->elseifbranches == NULL ? 1 : ifstmt->elseifbranches->count));
    if(ifstmt->elseifbranches != NULL)
    {
        for(i = 0; i < ifstmt->elseifbranches->count; i++)
        {
            e = ifstmt->elseifconds->values[i];
            if(e == NULL)
            {
                continue;
            }
            tin_astemit_patchjump(emt, elsejump, e->line);
            tin_astemit_emitexpression(emt, e);
            elsejump = tin_astemit_emitjump(emt, OP_JUMPIFFALSE, emt->lastline);
            tin_astemit_emitexpression(emt, ifstmt->elseifbranches->values[i]);
            endjumps[i] = tin_astemit_emitjump(emt, OP_JUMPALWAYS, emt->lastline);
        }
    }
    if(ifstmt->elsebranch != NULL)
    {
        tin_astemit_patchjump(emt, elsejump, ifstmt->elsebranch->line);
        tin_astemit_emitexpression(emt, ifstmt->elsebranch);
    }
    else
    {
        tin_astemit_patchjump(emt, elsejump, emt->lastline);
    }
    if(endjump != 0)
    {
        tin_astemit_patchjump(emt, endjump, emt->lastline);
    }
    if(ifstmt->elseifbranches != NULL)
    {
        for(i = 0; i < ifstmt->elseifbranches->count; i++)
        {
            if(ifstmt->elseifbranches->values[i] == NULL)
            {
                continue;
            }
            tin_astemit_patchjump(emt, endjumps[i], ifstmt->elseifbranches->values[i]->line);
        }
    }
    free(endjumps);
    return true;
}

static bool tin_astemit_doemitblockstmt(TinAstEmitter* emt, TinAstExpression* expr)
{
    size_t i;
    TinAstExpression* blockstmt;
    TinAstExprList* statements;
    statements = &((TinAstBlockExpr*)expr)->statements;
    tin_astemit_beginscope(emt);
    {
        for(i = 0; i < statements->count; i++)
        {
            blockstmt = statements->values[i];
            if(tin_astemit_emitexpression(emt, blockstmt))
            {
                break;
            }
        }
    }
    tin_astemit_endscope(emt, emt->lastline);
    return true;
}

static bool tin_astemit_doemitwhilestmt(TinAstEmitter* emt, TinAstExpression* expr)
{
    size_t start;
    size_t exitjump;
    TinAstWhileExpr* whilestmt;
    whilestmt = (TinAstWhileExpr*)expr;
    start = emt->chunk->count;
    emt->loopstart = start;
    emt->compiler->loopdepth++;
    tin_astemit_emitexpression(emt, whilestmt->condition);
    exitjump = tin_astemit_emitjump(emt, OP_JUMPIFFALSE, expr->line);
    tin_astemit_emitexpression(emt, whilestmt->body);
    tin_astemit_patchloopjumps(emt, &emt->continues, emt->lastline);
    tin_astemit_emitloop(emt, start, emt->lastline);
    tin_astemit_patchjump(emt, exitjump, emt->lastline);
    tin_astemit_patchloopjumps(emt, &emt->breaks, emt->lastline);
    emt->compiler->loopdepth--;
    return true;
}

static bool tin_astemit_doemitforstmt(TinAstEmitter* emt, TinAstExpression* expr)
{
    size_t i;
    size_t start;
    size_t exitjump;
    size_t bodyjump;
    size_t incrstart;
    size_t sequence;
    size_t iterator;
    size_t localcnt;
    const char* varstr;
    TinAstExprList* statements;
    TinAstAssignVarExpr* var;
    TinAstForExpr* forstmt;
    forstmt = (TinAstForExpr*)expr;
    tin_astemit_beginscope(emt);
    emt->compiler->loopdepth++;
    if(forstmt->cstyle)
    {
        if(forstmt->var != NULL)
        {
            tin_astemit_emitexpression(emt, forstmt->var);
        }
        else if(forstmt->init != NULL)
        {
            tin_astemit_emitexpression(emt, forstmt->init);
        }
        start = emt->chunk->count;
        exitjump = 0;
        if(forstmt->condition != NULL)
        {
            tin_astemit_emitexpression(emt, forstmt->condition);
            exitjump = tin_astemit_emitjump(emt, OP_JUMPIFFALSE, emt->lastline);
        }
        if(forstmt->increment != NULL)
        {
            bodyjump = tin_astemit_emitjump(emt, OP_JUMPALWAYS, emt->lastline);
            incrstart = emt->chunk->count;
            tin_astemit_emitexpression(emt, forstmt->increment);
            tin_astemit_emit1op(emt, emt->lastline, OP_POP);
            tin_astemit_emitloop(emt, start, emt->lastline);
            start = incrstart;
            tin_astemit_patchjump(emt, bodyjump, emt->lastline);
        }
        emt->loopstart = start;
        tin_astemit_beginscope(emt);
        if(forstmt->body != NULL)
        {
            if(forstmt->body->type == TINEXPR_BLOCK)
            {
                statements = &((TinAstBlockExpr*)forstmt->body)->statements;
                for(i = 0; i < statements->count; i++)
                {
                    tin_astemit_emitexpression(emt, statements->values[i]);
                }
            }
            else
            {
                tin_astemit_emitexpression(emt, forstmt->body);
            }
        }
        tin_astemit_patchloopjumps(emt, &emt->continues, emt->lastline);
        tin_astemit_endscope(emt, emt->lastline);
        tin_astemit_emitloop(emt, start, emt->lastline);
        if(forstmt->condition != NULL)
        {
            tin_astemit_patchjump(emt, exitjump, emt->lastline);
        }
    }
    else
    {
        {
            varstr = "seq ";
            sequence = tin_astemit_addlocal(emt, varstr, strlen(varstr), expr->line, false);
            tin_astemit_marklocalinit(emt, sequence);
            tin_astemit_emitexpression(emt, forstmt->condition);
            tin_astemit_emitbyteorshort(emt, emt->lastline, OP_LOCALSET, OP_LOCALLONGSET, sequence);
        }
        {
            varstr = "iter ";
            iterator = tin_astemit_addlocal(emt, varstr, strlen(varstr), expr->line, false);
            tin_astemit_marklocalinit(emt, iterator);
            tin_astemit_emit1op(emt, emt->lastline, OP_VALNULL);
            tin_astemit_emitbyteorshort(emt, emt->lastline, OP_LOCALSET, OP_LOCALLONGSET, iterator);
        }
        start = emt->chunk->count;
        emt->loopstart = emt->chunk->count;
        // iter = seq.iterator(iter)
        tin_astemit_emitbyteorshort(emt, emt->lastline, OP_LOCALGET, OP_LOCALLONGGET, sequence);
        tin_astemit_emitbyteorshort(emt, emt->lastline, OP_LOCALGET, OP_LOCALLONGGET, iterator);
        tin_astemit_emitvaryingop(emt, emt->lastline, OP_INVOKEMETHOD, 1);
        tin_astemit_emitshort(emt, emt->lastline,
                   tin_astemit_addconstant(emt, emt->lastline, tin_value_makestring(emt->state, "iterator")));
        tin_astemit_emitbyteorshort(emt, emt->lastline, OP_LOCALSET, OP_LOCALLONGSET, iterator);
        // If iter is null, just get out of the loop
        exitjump = tin_astemit_emitjump(emt, OP_JUMPIFNULLPOP, emt->lastline);
        tin_astemit_beginscope(emt);
        // var i = seq.iteratorValue(iter)
        var = (TinAstAssignVarExpr*)forstmt->var;
        localcnt = tin_astemit_addlocal(emt, var->name, var->length, expr->line, false);
        tin_astemit_marklocalinit(emt, localcnt);
        tin_astemit_emitbyteorshort(emt, emt->lastline, OP_LOCALGET, OP_LOCALLONGGET, sequence);
        tin_astemit_emitbyteorshort(emt, emt->lastline, OP_LOCALGET, OP_LOCALLONGGET, iterator);
        tin_astemit_emitvaryingop(emt, emt->lastline, OP_INVOKEMETHOD, 1);
        tin_astemit_emitshort(emt, emt->lastline,
                   tin_astemit_addconstant(emt, emt->lastline, tin_value_makestring(emt->state, "iteratorValue")));
        tin_astemit_emitbyteorshort(emt, emt->lastline, OP_LOCALSET, OP_LOCALLONGSET, localcnt);
        if(forstmt->body != NULL)
        {
            if(forstmt->body->type == TINEXPR_BLOCK)
            {
                statements = &((TinAstBlockExpr*)forstmt->body)->statements;
                for(i = 0; i < statements->count; i++)
                {
                    tin_astemit_emitexpression(emt, statements->values[i]);
                }
            }
            else
            {
                tin_astemit_emitexpression(emt, forstmt->body);
            }
        }
        tin_astemit_patchloopjumps(emt, &emt->continues, emt->lastline);
        tin_astemit_endscope(emt, emt->lastline);
        tin_astemit_emitloop(emt, start, emt->lastline);
        tin_astemit_patchjump(emt, exitjump, emt->lastline);
    }
    tin_astemit_patchloopjumps(emt, &emt->breaks, emt->lastline);
    tin_astemit_endscope(emt, emt->lastline);
    emt->compiler->loopdepth--;
    return true;
}

static bool tin_astemit_doemitbreak(TinAstEmitter* emt, TinAstExpression* expr)
{
    int depth;
    int ii;
    uint16_t local_count;
    TinAstLocal* local;
    TinAstLocList* locals;
    if(emt->compiler->loopdepth == 0)
    {
        tin_astemit_raiseerror(emt, expr->line, "cannot use '%s' outside of loops", "break");
    }
    tin_astemit_emit1op(emt, expr->line, OP_POPLOCALS);
    depth = emt->compiler->scope_depth;
    local_count = 0;
    locals = &emt->compiler->locals;
    for(ii = locals->count - 1; ii >= 0; ii--)
    {
        local = &locals->values[ii];
        if(local->depth < depth)
        {
            break;
        }

        if(!local->captured)
        {
            local_count++;
        }
    }
    tin_astemit_emitshort(emt, expr->line, local_count);
    tin_uintlist_push(emt->state, &emt->breaks, tin_astemit_emitjump(emt, OP_JUMPALWAYS, expr->line));
    return true;
}

static bool tin_astemit_doemitcontinue(TinAstEmitter* emt, TinAstExpression* expr)
{
    if(emt->compiler->loopdepth == 0)
    {
        tin_astemit_raiseerror(emt, expr->line, "cannot use '%s' outside of loops", "continue");
    }
    tin_uintlist_push(emt->state, &emt->continues, tin_astemit_emitjump(emt, OP_JUMPALWAYS, expr->line));
    return true;
}

static bool tin_astemit_doemitreturn(TinAstEmitter* emt, TinAstExpression* expr)
{
    TinAstExpression* subexpr;
    if(emt->compiler->type == TINFUNC_CONSTRUCTOR)
    {
        tin_astemit_raiseerror(emt, expr->line, "cannot use 'return' in constructors");
        return false;
    }
    subexpr = ((TinAstReturnExpr*)expr)->expression;
    if(subexpr == NULL)
    {
        tin_astemit_emit1op(emt, emt->lastline, OP_VALNULL);
    }
    else
    {
        tin_astemit_emitexpression(emt, subexpr);
    }
    tin_astemit_emit1op(emt, emt->lastline, OP_RETURN);
    if(emt->compiler->scope_depth == 0)
    {
        emt->compiler->skipreturn = true;
    }
    return true;
}

static bool tin_astemit_doemitfunction(TinAstEmitter* emt, TinAstExpression* expr)
{
    int index;
    size_t i;
    bool isprivate;
    bool islocal;
    bool isexport;
    bool vararg;
    TinString* name;
    TinAstCompiler compiler;
    TinFunction* function;
    TinAstFunctionExpr* funcstmt;
    funcstmt = (TinAstFunctionExpr*)expr;
    isexport = funcstmt->exported;
    isprivate = !isexport && emt->compiler->enclosing == NULL && emt->compiler->scope_depth == 0;
    islocal = !(isexport || isprivate);
    index = 0;
    if(!isexport)
    {
        index = isprivate ? tin_astemit_resolveprivate(emt, funcstmt->name, funcstmt->length, expr->line) :
                          tin_astemit_addlocal(emt, funcstmt->name, funcstmt->length, expr->line, false);
    }
    name = tin_string_copy(emt->state, funcstmt->name, funcstmt->length);
    if(islocal)
    {
        tin_astemit_marklocalinit(emt, index);
    }
    else if(isprivate)
    {
        tin_astemit_markprivateinit(emt, index);
    }
    tin_compiler_compiler(emt, &compiler, TINFUNC_REGULAR);
    tin_astemit_beginscope(emt);
    vararg = tin_astemit_emitparamlist(emt, &funcstmt->parameters, expr->line);
    tin_astemit_emitexpression(emt, funcstmt->body);
    tin_astemit_endscope(emt, emt->lastline);
    function = tin_compiler_end(emt, name);
    function->arg_count = funcstmt->parameters.count;
    function->maxslots += function->arg_count;
    function->vararg = vararg;
    if(function->upvalue_count > 0)
    {
        tin_astemit_emit1op(emt, emt->lastline, OP_MAKECLOSURE);
        tin_astemit_emitshort(emt, emt->lastline, tin_astemit_addconstant(emt, emt->lastline, tin_value_fromobject(function)));
        for(i = 0; i < function->upvalue_count; i++)
        {
            tin_astemit_emit2bytes(emt, emt->lastline, compiler.upvalues[i].isLocal ? 1 : 0, compiler.upvalues[i].index);
        }
    }
    else
    {
        tin_astemit_emitconstant(emt, emt->lastline, tin_value_fromobject(function));
    }
    if(isexport)
    {
        tin_astemit_emit1op(emt, emt->lastline, OP_GLOBALSET);
        tin_astemit_emitshort(emt, emt->lastline, tin_astemit_addconstant(emt, emt->lastline, tin_value_fromobject(name)));
    }
    else if(isprivate)
    {
        tin_astemit_emitbyteorshort(emt, emt->lastline, OP_PRIVATESET, OP_PRIVATELONGSET, index);
    }
    else
    {
        tin_astemit_emitbyteorshort(emt, emt->lastline, OP_LOCALSET, OP_LOCALLONGSET, index);
    }
    tin_astemit_emit1op(emt, emt->lastline, OP_POP);
    return true;
}

static bool tin_astemit_doemitmethod(TinAstEmitter* emt, TinAstExpression* expr)
{
    bool vararg;
    bool constructor;
    size_t i;
    TinAstCompiler compiler;
    TinFunction* function;
    TinAstMethodExpr* mthstmt;
    mthstmt = (TinAstMethodExpr*)expr;
    constructor = memcmp(mthstmt->name->data, TIN_VALUE_CTORNAME, strlen(TIN_VALUE_CTORNAME)) == 0;
    if(constructor && mthstmt->isstatic)
    {
        tin_astemit_raiseerror(emt, expr->line, "constructors cannot be static (at least for now)");
        return false;
    }
    tin_compiler_compiler(emt, &compiler,
                  constructor ? TINFUNC_CONSTRUCTOR : (mthstmt->isstatic ? TINFUNC_STATICMETHOD : TINFUNC_METHOD));
    tin_astemit_beginscope(emt);
    vararg = tin_astemit_emitparamlist(emt, &mthstmt->parameters, expr->line);
    tin_astemit_emitexpression(emt, mthstmt->body);
    tin_astemit_endscope(emt, emt->lastline);
    function = tin_compiler_end(emt, tin_value_asstring(tin_string_format(emt->state, "@:@", tin_value_fromobject(emt->classname), tin_value_fromobject(mthstmt->name))));
    function->arg_count = mthstmt->parameters.count;
    function->maxslots += function->arg_count;
    function->vararg = vararg;
    if(function->upvalue_count > 0)
    {
        tin_astemit_emit1op(emt, emt->lastline, OP_MAKECLOSURE);
        tin_astemit_emitshort(emt, emt->lastline, tin_astemit_addconstant(emt, emt->lastline, tin_value_fromobject(function)));
        for(i = 0; i < function->upvalue_count; i++)
        {
            tin_astemit_emit2bytes(emt, emt->lastline, compiler.upvalues[i].isLocal ? 1 : 0, compiler.upvalues[i].index);
        }
    }
    else
    {
        tin_astemit_emitconstant(emt, emt->lastline, tin_value_fromobject(function));
    }
    tin_astemit_emit1op(emt, emt->lastline, mthstmt->isstatic ? OP_FIELDSTATIC : OP_MAKEMETHOD);
    tin_astemit_emitshort(emt, emt->lastline, tin_astemit_addconstant(emt, expr->line, tin_value_fromobject(mthstmt->name)));
    return true;
}

static bool tin_astemit_doemitclass(TinAstEmitter* emt, TinAstExpression* expr)
{
    size_t i;
    uint8_t superidx;
    const char* varstr;
    TinAstExpression* s;
    TinAstClassExpr* clstmt;
    TinAstAssignVarExpr* var;
    clstmt = (TinAstClassExpr*)expr;
    emt->classname = clstmt->name;
    if(clstmt->parent != NULL)
    {
        tin_astemit_emit1op(emt, emt->lastline, OP_GLOBALGET);
        tin_astemit_emitshort(emt, emt->lastline, tin_astemit_addconstant(emt, emt->lastline, tin_value_fromobject(clstmt->parent)));
    }
    tin_astemit_emit1op(emt, expr->line, OP_MAKECLASS);
    tin_astemit_emitshort(emt, emt->lastline, tin_astemit_addconstant(emt, emt->lastline, tin_value_fromobject(clstmt->name)));
    if(clstmt->parent != NULL)
    {
        tin_astemit_emit1op(emt, emt->lastline, OP_CLASSINHERIT);
        emt->classisinheriting = true;
        tin_astemit_beginscope(emt);
        varstr = TIN_VALUE_SUPERNAME;
        superidx = tin_astemit_addlocal(emt, varstr, strlen(varstr), emt->lastline, false);
        tin_astemit_marklocalinit(emt, superidx);
    }
    for(i = 0; i < clstmt->fields.count; i++)
    {
        s = clstmt->fields.values[i];
        if(s->type == TINEXPR_VARSTMT)
        {
            var = (TinAstAssignVarExpr*)s;
            tin_astemit_emitexpression(emt, var->init);
            tin_astemit_emit1op(emt, expr->line, OP_FIELDSTATIC);
            tin_astemit_emitshort(emt, expr->line,
                       tin_astemit_addconstant(emt, expr->line,
                                    tin_value_fromobject(tin_string_copy(emt->state, var->name, var->length))));
        }
        else
        {
            tin_astemit_emitexpression(emt, s);
        }
    }
    tin_astemit_emit1op(emt, emt->lastline, OP_POP);
    if(clstmt->parent != NULL)
    {
        tin_astemit_endscope(emt, emt->lastline);
    }
    emt->classname = NULL;
    emt->classisinheriting = false;
    return true;
}

static bool tin_astemit_doemitfield(TinAstEmitter* emt, TinAstExpression* expr)
{
    const char* valstr;
    TinAstCompiler compiler;
    TinField* field;
    TinFunction* getter;
    TinFunction* setter;
    TinAstFieldExpr* fieldstmt;
    fieldstmt = (TinAstFieldExpr*)expr;
    getter = NULL;
    setter = NULL;
    if(fieldstmt->getter != NULL)
    {
        tin_compiler_compiler(emt, &compiler, fieldstmt->isstatic ? TINFUNC_STATICMETHOD : TINFUNC_METHOD);
        tin_astemit_beginscope(emt);
        tin_astemit_emitexpression(emt, fieldstmt->getter);
        tin_astemit_endscope(emt, emt->lastline);
        getter = tin_compiler_end(emt,
            tin_value_asstring(tin_string_format(emt->state, "@:get @", tin_value_fromobject(emt->classname), fieldstmt->name)));
    }
    if(fieldstmt->setter != NULL)
    {
        valstr = "value";
        tin_compiler_compiler(emt, &compiler, fieldstmt->isstatic ? TINFUNC_STATICMETHOD : TINFUNC_METHOD);
        tin_astemit_marklocalinit(emt, tin_astemit_addlocal(emt, valstr, strlen(valstr), expr->line, false));
        tin_astemit_beginscope(emt);
        tin_astemit_emitexpression(emt, fieldstmt->setter);
        tin_astemit_endscope(emt, emt->lastline);
        setter = tin_compiler_end(emt,
            tin_value_asstring(tin_string_format(emt->state, "@:set @", tin_value_fromobject(emt->classname), fieldstmt->name)));
        setter->arg_count = 1;
        setter->maxslots++;
    }
    field = tin_object_makefield(emt->state, (TinObject*)getter, (TinObject*)setter);
    tin_astemit_emitconstant(emt, expr->line, tin_value_fromobject(field));
    tin_astemit_emit1op(emt, expr->line, fieldstmt->isstatic ? OP_FIELDSTATIC : OP_FIELDDEFINE);
    tin_astemit_emitshort(emt, expr->line, tin_astemit_addconstant(emt, expr->line, tin_value_fromobject(fieldstmt->name)));
    return true;
}

static bool tin_astemit_doemitrange(TinAstEmitter* emt, TinAstExpression* expr)
{
    TinAstRangeExpr* rangeexpr;
    rangeexpr = (TinAstRangeExpr*)expr;
    tin_astemit_emitexpression(emt, rangeexpr->to);
    tin_astemit_emitexpression(emt, rangeexpr->from);
    tin_astemit_emit1op(emt, expr->line, OP_RANGE);
    return true;
}

static bool tin_astemit_doemitsubscript(TinAstEmitter* emt, TinAstExpression* expr)
{
    TinAstIndexExpr* subscrexpr;
    subscrexpr = (TinAstIndexExpr*)expr;
    tin_astemit_emitexpression(emt, subscrexpr->array);
    tin_astemit_emitexpression(emt, subscrexpr->index);
    tin_astemit_emit1op(emt, expr->line, OP_GETINDEX);
    return true;
}

static bool tin_astemit_doemitexpression(TinAstEmitter* emt, TinAstExpression* expr)
{
    TinAstExprExpr* stmtexpr;
    stmtexpr = (TinAstExprExpr*)expr;
    tin_astemit_emitexpression(emt, stmtexpr->expression);
    if(stmtexpr->pop)
    {
        tin_astemit_emit1op(emt, expr->line, OP_POP);
    }
    return true;
}

static bool tin_astemit_emitexpression(TinAstEmitter* emt, TinAstExpression* expr)
{
    if(expr == NULL)
    {
        return false;
    }
    switch(expr->type)
    {
        case TINEXPR_LITERAL:
            {
                if(!tin_astemit_doemitliteral(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_BINARY:
            {
                if(!tin_astemit_doemitbinary(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_UNARY:
            {
                if(!tin_astemit_doemitunary(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_VAREXPR:
            {
                if(!tin_astemit_doemitvarexpr(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_ASSIGN:
            {
                if(!tin_astemit_doemitassign(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_CALL:
            {
                if(!tin_astemit_doemitcall(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_GET:
            {
                if(!tin_astemit_doemitget(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_SET:
            {
                if(!tin_astemit_doemitset(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_LAMBDA:
            {
                if(!tin_astemit_doemitlambda(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_ARRAY:
            {
                if(!tin_astemit_doemitarray(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_OBJECT:
            {
                if(!tin_astemit_doemitobject(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_SUBSCRIPT:
            {
                if(!tin_astemit_doemitsubscript(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_THIS:
            {
                if(!tin_astemit_doemitthis(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_SUPER:
            {
                if(!tin_astemit_doemitsuper(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_RANGE:
            {
                if(!tin_astemit_doemitrange(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_TERNARY:
            {
                if(!tin_astemit_doemitternary(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_INTERPOLATION:
            {
                if(!tin_astemit_doemitinterpolation(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_REFERENCE:
            {
                if(!tin_astemit_doemitreference(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_EXPRESSION:
            {
                if(!tin_astemit_doemitexpression(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_BLOCK:
            {
                if(!tin_astemit_doemitblockstmt(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_VARSTMT:
            {
                if(!tin_astemit_doemitvarstmt(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_IFSTMT:
            {
                if(!tin_astemit_doemitifstmt(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_WHILE:
            {
                if(!tin_astemit_doemitwhilestmt(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_FOR:
            {
                if(!tin_astemit_doemitforstmt(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_CONTINUE:
            {
                if(!tin_astemit_doemitcontinue(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_BREAK:
            {
                if(!tin_astemit_doemitbreak(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_FUNCTION:
            {
                if(!tin_astemit_doemitfunction(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_RETURN:
            {
                if(!tin_astemit_doemitreturn(emt, expr))
                {
                    return false;
                }
                //return true;
            }
            break;
        case TINEXPR_METHOD:
            {
                if(!tin_astemit_doemitmethod(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_CLASS:
            {
                if(!tin_astemit_doemitclass(emt, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_FIELD:
            {
                if(!tin_astemit_doemitfield(emt, expr))
                {
                    return false;
                }
            }
            break;
        default:
            {
                tin_astemit_raiseerror(emt, expr->line, "unknown expression with id '%i'", (int)expr->type);
            }
            break;
    }
    emt->prevwasexprstmt = expr->type == TINEXPR_EXPRESSION;
    return false;
}

TinModule* tin_astemit_modemit(TinAstEmitter* emt, TinAstExprList* statements, TinString* module_name)
{
    size_t i;
    size_t total;
    size_t oldprivatescnt;
    bool isnew;
    TinState* state;        
    TinValue modulevalue;
    TinModule* module;
    TinAstPrivList* privates;
    TinAstCompiler compiler;
    TinAstExpression* exstmt;
    emt->lastline = 1;
    emt->emitref = 0;
    state = emt->state;
    isnew = false;
    if(tin_table_get(&emt->state->vm->modules->values, module_name, &modulevalue))
    {
        module = tin_value_asmodule(modulevalue);
    }
    else
    {
        module = tin_object_makemodule(emt->state, module_name);
        isnew = true;
    }
    emt->module = module;
    oldprivatescnt = module->private_count;
    if(oldprivatescnt > 0)
    {
        privates = &emt->privates;
        privates->count = oldprivatescnt - 1;
        tin_privlist_push(state, privates, tin_astemit_makeprivate(true, false));
        for(i = 0; i < oldprivatescnt; i++)
        {
            privates->values[i].initialized = true;
        }
    }
    tin_compiler_compiler(emt, &compiler, TINFUNC_SCRIPT);
    emt->chunk = &compiler.function->chunk;
    resolve_statements(emt, statements);
    for(i = 0; i < statements->count; i++)
    {
        exstmt = statements->values[i];
        if(tin_astemit_emitexpression(emt, exstmt))
        {
            break;
        }
    }
    tin_astemit_endscope(emt, emt->lastline);
    module->main_function = tin_compiler_end(emt, module_name);
    if(isnew)
    {
        total = emt->privates.count;
        module->privates = (TinValue*)tin_gcmem_allocate(emt->state, sizeof(TinValue), total);
        for(i = 0; i < total; i++)
        {
            module->privates[i] = tin_value_makenull(emt->state);
        }
    }
    else
    {
        module->privates = (TinValue*)tin_gcmem_growarray(emt->state, module->privates, sizeof(TinValue), oldprivatescnt, module->private_count);
        for(i = oldprivatescnt; i < module->private_count; i++)
        {
            module->privates[i] = tin_value_makenull(emt->state);
        }
    }
    tin_privlist_destroy(emt->state, &emt->privates);
    if(tin_astopt_isoptenabled(TINOPTSTATE_PRIVATENAMES))
    {
        tin_table_destroy(emt->state, &emt->module->private_names->values);
    }
    if(isnew && !state->haderror)
    {
        tin_table_set(state, &state->vm->modules->values, module_name, tin_value_fromobject(module));
    }
    module->ran = true;
    return module;
}
