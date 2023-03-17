
#include <string.h>
#include "priv.h"

static bool tin_astemit_emitexpression(TinAstEmitter* emitter, TinAstExpression* expression);
static void tin_astemit_resolvestmt(TinAstEmitter* emitter, TinAstExpression* statement);

static inline void tin_uintlist_init(TinUintList* array)
{
    tin_datalist_init(&array->list, sizeof(size_t));
}

static inline void tin_uintlist_destroy(TinState* state, TinUintList* array)
{
    tin_datalist_destroy(state, &array->list);
}

static inline void tin_uintlist_push(TinState* state, TinUintList* array, size_t value)
{
    tin_datalist_push(state, &array->list, value);
}

static inline size_t tin_uintlist_get(TinUintList* array, size_t idx)
{
    return (size_t)tin_datalist_get(&array->list, idx);
}

static inline size_t tin_uintlist_count(TinUintList* array)
{
    return tin_datalist_count(&array->list);
}

static void resolve_statements(TinAstEmitter* emitter, TinAstExprList* statements)
{
    size_t i;
    for(i = 0; i < statements->count; i++)
    {
        tin_astemit_resolvestmt(emitter, statements->values[i]);
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
    TIN_FREE_ARRAY(state, sizeof(TinAstPrivate), array->values, array->capacity);
    tin_privlist_init(array);
}

void tin_privlist_push(TinState* state, TinAstPrivList* array, TinAstPrivate value)
{
    size_t oldcapacity;
    if(array->capacity < array->count + 1)
    {
        oldcapacity = array->capacity;
        array->capacity = TIN_GROW_CAPACITY(oldcapacity);
        array->values = TIN_GROW_ARRAY(state, array->values, sizeof(TinAstPrivate), oldcapacity, array->capacity);
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
    TIN_FREE_ARRAY(state, sizeof(TinAstLocal), array->values, array->capacity);
    tin_loclist_init(array);
}

void tin_loclist_push(TinState* state, TinAstLocList* array, TinAstLocal value)
{
    size_t oldcapacity;
    if(array->capacity < array->count + 1)
    {
        oldcapacity = array->capacity;
        array->capacity = TIN_GROW_CAPACITY(oldcapacity);
        array->values = TIN_GROW_ARRAY(state, array->values, sizeof(TinAstLocal), oldcapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

static void tin_astemit_raiseerror(TinAstEmitter* emitter, size_t line, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    tin_state_raiseerror(emitter->state, COMPILE_ERROR, tin_vformat_error(emitter->state, line, fmt, args)->chars);
    va_end(args);
}

void tin_astemit_init(TinState* state, TinAstEmitter* emitter)
{
    emitter->state = state;
    emitter->loopstart = 0;
    emitter->emitref = 0;
    emitter->classname = NULL;
    emitter->compiler = NULL;
    emitter->chunk = NULL;
    emitter->module = NULL;
    emitter->prevwasexprstmt = false;
    emitter->classisinheriting = false;
    tin_privlist_init(&emitter->privates);
    tin_uintlist_init(&emitter->breaks);
    tin_uintlist_init(&emitter->continues);
}

void tin_astemit_destroy(TinAstEmitter* emitter)
{
    tin_uintlist_destroy(emitter->state, &emitter->breaks);
    tin_uintlist_destroy(emitter->state, &emitter->continues);
}

static void tin_astemit_emit1byte(TinAstEmitter* emitter, uint16_t line, uint8_t byte)
{
    if(line < emitter->lastline)
    {
        // Egor-fail proofing
        line = emitter->lastline;
    }
    tin_chunk_push(emitter->state, emitter->chunk, byte, line);
    emitter->lastline = line;
}

static const int8_t stack_effects[] =
{
#define OPCODE(_, effect) effect,
#include "opcodes.inc"
#undef OPCODE
};

static void tin_astemit_emit2bytes(TinAstEmitter* emitter, uint16_t line, uint8_t a, uint8_t b)
{
    if(line < emitter->lastline)
    {
        // Egor-fail proofing
        line = emitter->lastline;
    }
    tin_chunk_push(emitter->state, emitter->chunk, a, line);
    tin_chunk_push(emitter->state, emitter->chunk, b, line);
    emitter->lastline = line;
}

static void tin_astemit_emit1op(TinAstEmitter* emitter, uint16_t line, TinOpCode op)
{
    TinAstCompiler* compiler;
    compiler = emitter->compiler;
    tin_astemit_emit1byte(emitter, line, (uint8_t)op);
    compiler->slots += stack_effects[(int)op];
    if(compiler->slots > (int)compiler->function->maxslots)
    {
        compiler->function->maxslots = (size_t)compiler->slots;
    }
}

static void tin_astemit_emit2ops(TinAstEmitter* emitter, uint16_t line, TinOpCode a, TinOpCode b)
{
    TinAstCompiler* compiler;
    compiler = emitter->compiler;
    tin_astemit_emit2bytes(emitter, line, (uint8_t)a, (uint8_t)b);
    compiler->slots += stack_effects[(int)a] + stack_effects[(int)b];
    if(compiler->slots > (int)compiler->function->maxslots)
    {
        compiler->function->maxslots = (size_t)compiler->slots;
    }
}

static void tin_astemit_emitvaryingop(TinAstEmitter* emitter, uint16_t line, TinOpCode op, uint8_t arg)
{
    TinAstCompiler* compiler;
    compiler = emitter->compiler;
    tin_astemit_emit2bytes(emitter, line, (uint8_t)op, arg);
    compiler->slots -= arg;
    if(compiler->slots > (int)compiler->function->maxslots)
    {
        compiler->function->maxslots = (size_t)compiler->slots;
    }
}

static void tin_astemit_emitargedop(TinAstEmitter* emitter, uint16_t line, TinOpCode op, uint8_t arg)
{
    TinAstCompiler* compiler;
    compiler = emitter->compiler;
    tin_astemit_emit2bytes(emitter, line, (uint8_t)op, arg);
    compiler->slots += stack_effects[(int)op];
    if(compiler->slots > (int)compiler->function->maxslots)
    {
        compiler->function->maxslots = (size_t)compiler->slots;
    }
}

static void tin_astemit_emitshort(TinAstEmitter* emitter, uint16_t line, uint16_t value)
{
    tin_astemit_emit2bytes(emitter, line, (uint8_t)((value >> 8) & 0xff), (uint8_t)(value & 0xff));
}

static void tin_astemit_emitbyteorshort(TinAstEmitter* emitter, uint16_t line, uint8_t a, uint8_t b, uint16_t index)
{
    if(index > UINT8_MAX)
    {
        tin_astemit_emit1op(emitter, line, (TinOpCode)b);
        tin_astemit_emitshort(emitter, line, (uint16_t)index);
    }
    else
    {
        tin_astemit_emitargedop(emitter, line, (TinOpCode)a, (uint8_t)index);
    }
}

static void tin_compiler_compiler(TinAstEmitter* emitter, TinAstCompiler* compiler, TinAstFuncType type)
{
    const char* name;
    tin_loclist_init(&compiler->locals);
    compiler->type = type;
    compiler->scope_depth = 0;
    compiler->enclosing = (struct TinAstCompiler*)emitter->compiler;
    compiler->skipreturn = false;
    compiler->function = tin_object_makefunction(emitter->state, emitter->module);
    compiler->loopdepth = 0;
    emitter->compiler = compiler;
    name = emitter->state->scanner->filename;
    if(emitter->compiler == NULL)
    {
        compiler->function->name = tin_string_copy(emitter->state, name, strlen(name));
    }
    emitter->chunk = &compiler->function->chunk;
    if(tin_astopt_isoptenabled(TINOPTSTATE_LINEINFO))
    {
        emitter->chunk->has_line_info = false;
    }
    if(type == TINFUNC_METHOD || type == TINFUNC_STATICMETHOD || type == TINFUNC_CONSTRUCTOR)
    {
        tin_loclist_push(emitter->state, &compiler->locals, (TinAstLocal){ "this", 4, -1, false, false });
    }
    else
    {
        tin_loclist_push(emitter->state, &compiler->locals, (TinAstLocal){ "", 0, -1, false, false });
    }
    compiler->slots = 1;
    compiler->maxslots = 1;
}

static void tin_astemit_emitreturn(TinAstEmitter* emitter, size_t line)
{
    if(emitter->compiler->type == TINFUNC_CONSTRUCTOR)
    {
        tin_astemit_emitargedop(emitter, line, OP_GET_LOCAL, 0);
        tin_astemit_emit1op(emitter, line, OP_RETURN);
    }
    else if(emitter->prevwasexprstmt && emitter->chunk->count > 0)
    {
        emitter->chunk->count--;// Remove the OP_POP
        tin_astemit_emit1op(emitter, line, OP_RETURN);
    }
    else
    {
        tin_astemit_emit2ops(emitter, line, OP_VALNULL, OP_RETURN);
    }
}

static TinFunction* tin_compiler_end(TinAstEmitter* emitter, TinString* name)
{
    TinFunction* function;
    if(!emitter->compiler->skipreturn)
    {
        tin_astemit_emitreturn(emitter, emitter->lastline);
        emitter->compiler->skipreturn = true;
    }
    function = emitter->compiler->function;
    tin_loclist_destroy(emitter->state, &emitter->compiler->locals);
    emitter->compiler = (TinAstCompiler*)emitter->compiler->enclosing;
    emitter->chunk = emitter->compiler == NULL ? NULL : &emitter->compiler->function->chunk;
    if(name != NULL)
    {
        function->name = name;
    }
#ifdef TIN_TRACE_CHUNK
    tin_disassemble_chunk(&function->chunk, function->name->chars, NULL);
#endif
    return function;
}

static void tin_astemit_beginscope(TinAstEmitter* emitter)
{
    emitter->compiler->scope_depth++;
}

static void tin_astemit_endscope(TinAstEmitter* emitter, uint16_t line)
{
    TinAstLocList* locals;
    TinAstCompiler* compiler;
    emitter->compiler->scope_depth--;
    compiler = emitter->compiler;
    locals = &compiler->locals;
    while(locals->count > 0 && locals->values[locals->count - 1].depth > compiler->scope_depth)
    {
        if(locals->values[locals->count - 1].captured)
        {
            tin_astemit_emit1op(emitter, line, OP_CLOSE_UPVALUE);
        }
        else
        {
            tin_astemit_emit1op(emitter, line, OP_POP);
        }
        locals->count--;
    }
}

static uint16_t tin_astemit_addconstant(TinAstEmitter* emitter, size_t line, TinValue value)
{
    size_t constant;
    constant = tin_chunk_addconst(emitter->state, emitter->chunk, value);
    if(constant >= UINT16_MAX)
    {
        tin_astemit_raiseerror(emitter, line, "too many constants for one chunk");
    }
    return constant;
}

static size_t tin_astemit_emitconstant(TinAstEmitter* emitter, size_t line, TinValue value)
{
    size_t constant;
    constant = tin_chunk_addconst(emitter->state, emitter->chunk, value);
    if(constant < UINT8_MAX)
    {
        tin_astemit_emitargedop(emitter, line, OP_CONSTVALUE, constant);
    }
    else if(constant < UINT16_MAX)
    {
        tin_astemit_emit1op(emitter, line, OP_CONSTLONG);
        tin_astemit_emitshort(emitter, line, constant);
    }
    else
    {
        tin_astemit_raiseerror(emitter, line, "too many constants for one chunk");
    }
    return constant;
}

static int tin_astemit_addprivate(TinAstEmitter* emitter, const char* name, size_t length, size_t line, bool constant)
{
    int index;
    TinValue idxval;
    TinState* state;
    TinString* key;
    TinTable* privnames;
    TinAstPrivList* privates;
    privates = &emitter->privates;
    if(privates->count == UINT16_MAX)
    {
        tin_astemit_raiseerror(emitter, line, "too many private locals for one module");
    }
    privnames = &emitter->module->private_names->values;
    key = tin_table_find_string(privnames, name, length, tin_util_hashstring(name, length));
    if(key != NULL)
    {
        tin_astemit_raiseerror(emitter, line, "variable '%.*s' was already declared in this scope", length, name);
        tin_table_get(privnames, key, &idxval);
        return tin_value_asnumber(idxval);
    }
    state = emitter->state;
    index = (int)privates->count;
    tin_privlist_push(state, privates, (TinAstPrivate){ false, constant });
    tin_table_set(state, privnames, tin_string_copy(state, name, length), tin_value_makefixednumber(state, index));
    emitter->module->private_count++;
    return index;
}

static int tin_astemit_resolveprivate(TinAstEmitter* emitter, const char* name, size_t length, size_t line)
{
    int numberindex;
    TinValue index;
    TinString* key;
    TinTable* privnames;
    privnames = &emitter->module->private_names->values;
    key = tin_table_find_string(privnames, name, length, tin_util_hashstring(name, length));
    if(key != NULL)
    {
        tin_table_get(privnames, key, &index);
        numberindex = tin_value_asnumber(index);
        if(!emitter->privates.values[numberindex].initialized)
        {
            tin_astemit_raiseerror(emitter, line, "variable '%.*s' cannot use itself in its initializer", length, name);
        }
        return numberindex;
    }
    return -1;
}

static int tin_astemit_addlocal(TinAstEmitter* emitter, const char* name, size_t length, size_t line, bool constant)
{
    int i;
    TinAstLocal* local;
    TinAstLocList* locals;
    TinAstCompiler* compiler;
    compiler = emitter->compiler;
    locals = &compiler->locals;
    if(locals->count == UINT16_MAX)
    {
        tin_astemit_raiseerror(emitter, line, "too many local variables for one function");
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
            tin_astemit_raiseerror(emitter, line, "variable '%.*s' was already declared in this scope", length, name);
        }
    }
    tin_loclist_push(emitter->state, locals, (TinAstLocal){ name, length, UINT16_MAX, false, constant });
    return (((int)locals->count) - 1);
}

static int tin_astemit_resolvelocal(TinAstEmitter* emitter, TinAstCompiler* compiler, const char* name, size_t length, size_t line)
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
                tin_astemit_raiseerror(emitter, line, "variable '%.*s' cannot use itself in its initializer", length, name);
            }

            return i;
        }
    }

    return -1;
}

static int tin_astemit_addupvalue(TinAstEmitter* emitter, TinAstCompiler* compiler, uint8_t index, size_t line, bool islocal)
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
        tin_astemit_raiseerror(emitter, line, "too many upvalues for one function");
        return 0;
    }
    compiler->upvalues[upvalcnt].isLocal = islocal;
    compiler->upvalues[upvalcnt].index = index;
    return compiler->function->upvalue_count++;
}

static int tin_astemit_resolveupvalue(TinAstEmitter* emitter, TinAstCompiler* compiler, const char* name, size_t length, size_t line)
{
    int local;
    int upvalue;
    if(compiler->enclosing == NULL)
    {
        return -1;
    }
    local = tin_astemit_resolvelocal(emitter, (TinAstCompiler*)compiler->enclosing, name, length, line);
    if(local != -1)
    {
        ((TinAstCompiler*)compiler->enclosing)->locals.values[local].captured = true;
        return tin_astemit_addupvalue(emitter, compiler, (uint8_t)local, line, true);
    }
    upvalue = tin_astemit_resolveupvalue(emitter, (TinAstCompiler*)compiler->enclosing, name, length, line);
    if(upvalue != -1)
    {
        return tin_astemit_addupvalue(emitter, compiler, (uint8_t)upvalue, line, false);
    }
    return -1;
}

static void tin_astemit_marklocalinit(TinAstEmitter* emitter, size_t index)
{
    emitter->compiler->locals.values[index].depth = emitter->compiler->scope_depth;
}

static void tin_astemit_markprivateinit(TinAstEmitter* emitter, size_t index)
{
    emitter->privates.values[index].initialized = true;
}

static size_t tin_astemit_emitjump(TinAstEmitter* emitter, TinOpCode code, size_t line)
{
    tin_astemit_emit1op(emitter, line, code);
    tin_astemit_emit2bytes(emitter, line, 0xff, 0xff);
    return emitter->chunk->count - 2;
}

static void tin_astemit_patchjump(TinAstEmitter* emitter, size_t offset, size_t line)
{
    size_t jump;
    jump = emitter->chunk->count - offset - 2;
    if(jump > UINT16_MAX)
    {
        tin_astemit_raiseerror(emitter, line, "too much code to jump over");
    }
    emitter->chunk->code[offset] = (jump >> 8) & 0xff;
    emitter->chunk->code[offset + 1] = jump & 0xff;
}

static void tin_astemit_emitloop(TinAstEmitter* emitter, size_t start, size_t line)
{
    size_t offset;
    tin_astemit_emit1op(emitter, line, OP_JUMP_BACK);
    offset = emitter->chunk->count - start + 2;
    if(offset > UINT16_MAX)
    {
        tin_astemit_raiseerror(emitter, line, "too much code to jump over");
    }
    tin_astemit_emitshort(emitter, line, offset);
}

static void tin_astemit_patchloopjumps(TinAstEmitter* emitter, TinUintList* breaks, size_t line)
{
    size_t i;
    for(i = 0; i < tin_uintlist_count(breaks); i++)
    {
        tin_astemit_patchjump(emitter, tin_uintlist_get(breaks, i), line);
    }
    tin_uintlist_destroy(emitter->state, breaks);
}

static bool tin_astemit_emitparamlist(TinAstEmitter* emitter, TinAstParamList* parameters, size_t line)
{
    size_t i;
    size_t jump;
    int index;
    TinAstParameter* parameter;
    for(i = 0; i < parameters->count; i++)
    {
        parameter = &parameters->values[i];
        index = tin_astemit_addlocal(emitter, parameter->name, parameter->length, line, false);
        tin_astemit_marklocalinit(emitter, index);
        // Vararg ...
        if(parameter->length == 3 && memcmp(parameter->name, "...", 3) == 0)
        {
            return true;
        }
        if(parameter->defaultexpr != NULL)
        {
            tin_astemit_emitbyteorshort(emitter, line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, index);
            jump = tin_astemit_emitjump(emitter, OP_NULL_OR, line);
            tin_astemit_emitexpression(emitter, parameter->defaultexpr);
            tin_astemit_patchjump(emitter, jump, line);
            tin_astemit_emitbyteorshort(emitter, line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, index);
            tin_astemit_emit1op(emitter, line, OP_POP);
        }
    }
    return false;
}

static void tin_astemit_resolvestmt(TinAstEmitter* emitter, TinAstExpression* statement)
{
    TinAstFunctionExpr* funcstmt;
    TinAstAssignVarExpr* varstmt;
    switch(statement->type)
    {
        case TINEXPR_VARSTMT:
            {
                varstmt = (TinAstAssignVarExpr*)statement;
                tin_astemit_markprivateinit(emitter, tin_astemit_addprivate(emitter, varstmt->name, varstmt->length, statement->line, varstmt->constant));
            }
            break;
        case TINEXPR_FUNCTION:
            {
                funcstmt = (TinAstFunctionExpr*)statement;
                if(!funcstmt->exported)
                {
                    tin_astemit_markprivateinit(emitter, tin_astemit_addprivate(emitter, funcstmt->name, funcstmt->length, statement->line, false));
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

static bool tin_astemit_doemitliteral(TinAstEmitter* emitter, TinAstExpression* expr)
{
    TinValue value;
    value = ((TinAstLiteralExpr*)expr)->value;
    if(tin_value_isnumber(value) || tin_value_isstring(value))
    {
        tin_astemit_emitconstant(emitter, expr->line, value);
    }
    else if(tin_value_isbool(value))
    {
        tin_astemit_emit1op(emitter, expr->line, tin_value_asbool(value) ? OP_VALTRUE : OP_VALFALSE);
    }
    else if(tin_value_isnull(value))
    {
        tin_astemit_emit1op(emitter, expr->line, OP_VALNULL);
    }
    else
    {
        UNREACHABLE;
    }
    return true;
}

static bool tin_astemit_doemitbinary(TinAstEmitter* emitter, TinAstExpression* expr)
{
    size_t jump;
    TinAstTokType op;
    TinAstBinaryExpr* binexpr;
    binexpr = (TinAstBinaryExpr*)expr;
    tin_astemit_emitexpression(emitter, binexpr->left);
    if(binexpr->right == NULL)
    {
        return true;
    }
    op = binexpr->op;
    if(op == TINTOK_DOUBLEAMPERSAND || op == TINTOK_DOUBLEBAR || op == TINTOK_DOUBLEQUESTION)
    {
        //jump = tin_astemit_emitjump(emitter, op == TINTOK_DOUBLEBAR ? OP_OR : (op == TINTOK_DOUBLEQUESTION ? OP_NULL_OR : OP_AND), emitter->lastline);
        jump = 0;
        if(op == TINTOK_DOUBLEBAR)
        {
            jump = tin_astemit_emitjump(emitter, OP_OR, emitter->lastline);
        }
        else if(op == TINTOK_DOUBLEQUESTION)
        {
            jump = tin_astemit_emitjump(emitter, OP_NULL_OR, emitter->lastline);
        }
        else
        {
            jump = tin_astemit_emitjump(emitter, OP_AND, emitter->lastline);
        }
        tin_astemit_emitexpression(emitter, binexpr->right);
        tin_astemit_patchjump(emitter, jump, emitter->lastline);
        return true;
    }
    tin_astemit_emitexpression(emitter, binexpr->right);
    switch(op)
    {
        case TINTOK_PLUS:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_ADD);
            }
            break;
        case TINTOK_MINUS:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_SUBTRACT);
            }
            break;
        case TINTOK_STAR:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_MULTIPLY);
            }
            break;
        case TINTOK_DOUBLESTAR:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_POWER);
            }
            break;
        case TINTOK_SLASH:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_DIVIDE);
            }
            break;
        case TINTOK_SHARP:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_FLOOR_DIVIDE);
            }
            break;
        case TINTOK_PERCENT:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_MOD);
            }
            break;
        case TINTOK_KWIS:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_IS);
            }
            break;
        case TINTOK_EQUAL:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_EQUAL);
            }
            break;
        case TINTOK_BANGEQUAL:
            {
                tin_astemit_emit2ops(emitter, expr->line, OP_EQUAL, OP_NOT);
            }
            break;
        case TINTOK_GREATERTHAN:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_GREATER);
            }
            break;
        case TINTOK_GREATEREQUAL:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_GREATER_EQUAL);
            }
            break;
        case TINTOK_LESSTHAN:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_LESS);
            }
            break;
        case TINTOK_LESSEQUAL:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_LESS_EQUAL);
            }
            break;
        case TINTOK_SHIFTLEFT:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_LSHIFT);
            }
            break;
        case TINTOK_SHIFTRIGHT:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_RSHIFT);
            }
            break;
        case TINTOK_BAR:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_BOR);
            }
            break;
        case TINTOK_AMPERSAND:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_BAND);
            }
            break;
        case TINTOK_CARET:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_BXOR);
            }
            break;
        default:
            {
                fprintf(stderr, "in tin_astemit_emitexpression: binary expression #2 is NULL! might be a bug\n");
                //return;
                //UNREACHABLE;
            }
        break;
    }
    return true;
}

static bool tin_astemit_doemitunary(TinAstEmitter* emitter, TinAstExpression* expr)
{
    TinAstUnaryExpr* unexpr;
    unexpr = (TinAstUnaryExpr*)expr;
    tin_astemit_emitexpression(emitter, unexpr->right);
    switch(unexpr->op)
    {
        case TINTOK_MINUS:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_NEGATE);
            }
            break;
        case TINTOK_BANG:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_NOT);
            }
            break;
        case TINTOK_TILDE:
            {
                tin_astemit_emit1op(emitter, expr->line, OP_BNOT);
            }
            break;
        default:
            {
                fprintf(stderr, "in tin_astemit_emitexpression: unary expr is NULL! might be an internal bug\n");
                //return;
                //UNREACHABLE;
            }
            break;
    }
    return true;
}

static bool tin_astemit_doemitvarexpr(TinAstEmitter* emitter, TinAstExpression* expr)
{
    int index;
    bool ref;
    TinAstVarExpr* varexpr;
    varexpr = (TinAstVarExpr*)expr;
    ref = emitter->emitref > 0;
    if(ref)
    {
        emitter->emitref--;
    }
    index = tin_astemit_resolvelocal(emitter, emitter->compiler, varexpr->name, varexpr->length, expr->line);
    if(index == -1)
    {
        index = tin_astemit_resolveupvalue(emitter, emitter->compiler, varexpr->name, varexpr->length, expr->line);
        if(index == -1)
        {
            index = tin_astemit_resolveprivate(emitter, varexpr->name, varexpr->length, expr->line);
            if(index == -1)
            {
                tin_astemit_emit1op(emitter, expr->line, ref ? OP_REFERENCE_GLOBAL : OP_GET_GLOBAL);
                tin_astemit_emitshort(emitter, expr->line,
                           tin_astemit_addconstant(emitter, expr->line,
                                        tin_value_fromobject(tin_string_copy(emitter->state, varexpr->name, varexpr->length))));
            }
            else
            {
                if(ref)
                {
                    tin_astemit_emit1op(emitter, expr->line, OP_REFERENCE_PRIVATE);
                    tin_astemit_emitshort(emitter, expr->line, index);
                }
                else
                {
                    tin_astemit_emitbyteorshort(emitter, expr->line, OP_GET_PRIVATE, OP_GET_PRIVATE_LONG, index);
                }
            }
        }
        else
        {
            tin_astemit_emitargedop(emitter, expr->line, ref ? OP_REFERENCE_UPVALUE : OP_GET_UPVALUE, (uint8_t)index);
        }
    }
    else
    {
        if(ref)
        {
            tin_astemit_emit1op(emitter, expr->line, OP_REFERENCE_LOCAL);
            tin_astemit_emitshort(emitter, expr->line, index);
        }
        else
        {
            tin_astemit_emitbyteorshort(emitter, expr->line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, index);
        }
    }
    return true;
}

static bool tin_astemit_doemitassign(TinAstEmitter* emitter, TinAstExpression* expr)
{
    int index;
    TinAstVarExpr* e;
    TinAstGetExpr* getexpr;
    TinAstIndexExpr* indexexpr;
    TinAstAssignExpr* assignexpr;
    assignexpr = (TinAstAssignExpr*)expr;
    if(assignexpr->to->type == TINEXPR_VAREXPR)
    {
        tin_astemit_emitexpression(emitter, assignexpr->value);
        e = (TinAstVarExpr*)assignexpr->to;
        index = tin_astemit_resolvelocal(emitter, emitter->compiler, e->name, e->length, assignexpr->to->line);
        if(index == -1)
        {
            index = tin_astemit_resolveupvalue(emitter, emitter->compiler, e->name, e->length, assignexpr->to->line);
            if(index == -1)
            {
                index = tin_astemit_resolveprivate(emitter, e->name, e->length, assignexpr->to->line);
                if(index == -1)
                {
                    tin_astemit_emit1op(emitter, expr->line, OP_SET_GLOBAL);
                    tin_astemit_emitshort(emitter, expr->line,
                               tin_astemit_addconstant(emitter, expr->line,
                                            tin_value_fromobject(tin_string_copy(emitter->state, e->name, e->length))));
                }
                else
                {
                    if(emitter->privates.values[index].constant)
                    {
                        tin_astemit_raiseerror(emitter, expr->line, "attempt to modify constant '%.*s'", e->length, e->name);
                    }
                    tin_astemit_emitbyteorshort(emitter, expr->line, OP_SET_PRIVATE, OP_SET_PRIVATE_LONG, index);
                }
            }
            else
            {
                tin_astemit_emitargedop(emitter, expr->line, OP_SET_UPVALUE, (uint8_t)index);
            }
            return true;
        }
        else
        {
            if(emitter->compiler->locals.values[index].constant)
            {
                tin_astemit_raiseerror(emitter, expr->line, "attempt to modify constant '%.*s'", e->length, e->name);
            }

            tin_astemit_emitbyteorshort(emitter, expr->line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, index);
        }
    }
    else if(assignexpr->to->type == TINEXPR_GET)
    {
        tin_astemit_emitexpression(emitter, assignexpr->value);
        getexpr = (TinAstGetExpr*)assignexpr->to;
        tin_astemit_emitexpression(emitter, getexpr->where);
        tin_astemit_emitexpression(emitter, assignexpr->value);
        tin_astemit_emitconstant(emitter, emitter->lastline, tin_value_fromobject(tin_string_copy(emitter->state, getexpr->name, getexpr->length)));
        tin_astemit_emit2ops(emitter, emitter->lastline, OP_FIELDSET, OP_POP);
    }
    else if(assignexpr->to->type == TINEXPR_SUBSCRIPT)
    {
        indexexpr = (TinAstIndexExpr*)assignexpr->to;
        tin_astemit_emitexpression(emitter, indexexpr->array);
        tin_astemit_emitexpression(emitter, indexexpr->index);
        tin_astemit_emitexpression(emitter, assignexpr->value);
        tin_astemit_emit1op(emitter, emitter->lastline, OP_SUBSCRIPT_SET);
    }
    else if(assignexpr->to->type == TINEXPR_REFERENCE)
    {
        tin_astemit_emitexpression(emitter, assignexpr->value);
        tin_astemit_emitexpression(emitter, ((TinAstRefExpr*)assignexpr->to)->to);
        tin_astemit_emit1op(emitter, expr->line, OP_SET_REFERENCE);
    }
    else
    {
        tin_astemit_raiseerror(emitter, expr->line, "invalid assigment target");
    }
    return true;
}

static bool tin_astemit_doemitcall(TinAstEmitter* emitter, TinAstExpression* expr)
{
    size_t i;
    uint8_t index;
    bool issuper;
    bool ismethod;
    TinAstExpression* e;
    TinAstVarExpr* ee;
    TinAstObjectExpr* init;
    TinAstCallExpr* callexpr;
    TinAstSuperExpr* superexpr;
    TinAstExpression* get;
    TinAstGetExpr* getexpr;
    TinAstGetExpr* getter;
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
    tin_astemit_emitexpression(emitter, callexpr->callee);
    if(issuper)
    {
        tin_astemit_emitargedop(emitter, expr->line, OP_GET_LOCAL, 0);
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
                tin_astemit_emitargedop(emitter, e->line, OP_VARARG,
                              tin_astemit_resolvelocal(emitter, emitter->compiler, "...", 3, expr->line));
                break;
            }
        }
        tin_astemit_emitexpression(emitter, e);
    }
    if(ismethod || issuper)
    {
        if(ismethod)
        {
            getexpr = (TinAstGetExpr*)callexpr->callee;
            tin_astemit_emitvaryingop(emitter, expr->line,
                            ((TinAstGetExpr*)callexpr->callee)->ignresult ? OP_INVOKE_IGNORING : OP_INVOKE,
                            (uint8_t)callexpr->args.count);
            tin_astemit_emitshort(emitter, emitter->lastline,
                       tin_astemit_addconstant(emitter, emitter->lastline,
                                    tin_value_fromobject(tin_string_copy(emitter->state, getexpr->name, getexpr->length))));
        }
        else
        {
            superexpr = (TinAstSuperExpr*)callexpr->callee;
            index = tin_astemit_resolveupvalue(emitter, emitter->compiler, "super", 5, emitter->lastline);
            tin_astemit_emitargedop(emitter, expr->line, OP_GET_UPVALUE, index);
            tin_astemit_emitvaryingop(emitter, emitter->lastline,
                            ((TinAstSuperExpr*)callexpr->callee)->ignresult ? OP_INVOKE_SUPER_IGNORING : OP_INVOKE_SUPER,
                            (uint8_t)callexpr->args.count);
            tin_astemit_emitshort(emitter, emitter->lastline, tin_astemit_addconstant(emitter, emitter->lastline, tin_value_fromobject(superexpr->method)));
        }
    }
    else
    {
        tin_astemit_emitvaryingop(emitter, expr->line, OP_CALL, (uint8_t)callexpr->args.count);
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
                    tin_astemit_patchjump(emitter, getter->jump, emitter->lastline);
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
        emitter->lastline = e->line;
        tin_astemit_emitexpression(emitter, init->keys.values[i]);
        tin_astemit_emitexpression(emitter, e);
        tin_astemit_emit1op(emitter, emitter->lastline, OP_PUSH_OBJECT_FIELD);
    }
    return true;
}

static bool tin_astemit_doemitget(TinAstEmitter* emitter, TinAstExpression* expr)
{
    bool ref;
    TinAstGetExpr* getexpr;
    getexpr = (TinAstGetExpr*)expr;
    ref = emitter->emitref > 0;
    if(ref)
    {
        emitter->emitref--;
    }
    tin_astemit_emitexpression(emitter, getexpr->where);
    if(getexpr->jump == 0)
    {
        getexpr->jump = tin_astemit_emitjump(emitter, OP_JUMP_IF_NULL, emitter->lastline);
        if(!getexpr->ignemit)
        {
            tin_astemit_emitconstant(emitter, emitter->lastline,
                          tin_value_fromobject(tin_string_copy(emitter->state, getexpr->name, getexpr->length)));
            tin_astemit_emit1op(emitter, emitter->lastline, ref ? OP_REFERENCE_FIELD : OP_FIELDGET);
        }
        tin_astemit_patchjump(emitter, getexpr->jump, emitter->lastline);
    }
    else if(!getexpr->ignemit)
    {
        tin_astemit_emitconstant(emitter, emitter->lastline, tin_value_fromobject(tin_string_copy(emitter->state, getexpr->name, getexpr->length)));
        tin_astemit_emit1op(emitter, emitter->lastline, ref ? OP_REFERENCE_FIELD : OP_FIELDGET);
    }
    return true;
}

static bool tin_astemit_doemitset(TinAstEmitter* emitter, TinAstExpression* expr)
{
    TinAstSetExpr* setexpr;
    setexpr = (TinAstSetExpr*)expr;
    tin_astemit_emitexpression(emitter, setexpr->where);
    tin_astemit_emitexpression(emitter, setexpr->value);
    tin_astemit_emitconstant(emitter, emitter->lastline, tin_value_fromobject(tin_string_copy(emitter->state, setexpr->name, setexpr->length)));
    tin_astemit_emit1op(emitter, emitter->lastline, OP_FIELDSET);
    return true;
}

static bool tin_astemit_doemitlambda(TinAstEmitter* emitter, TinAstExpression* expr)
{
    size_t i;
    bool vararg;
    bool singleexpr;
    TinAstCompiler compiler;
    TinString* name;
    TinFunction* function;
    TinAstFunctionExpr* lambdaexpr;
    lambdaexpr = (TinAstFunctionExpr*)expr;
    name = tin_value_asstring(tin_string_format(emitter->state,
        "lambda @:@", tin_value_fromobject(emitter->module->name), tin_string_numbertostring(emitter->state, expr->line)));
    tin_compiler_compiler(emitter, &compiler, TINFUNC_REGULAR);
    tin_astemit_beginscope(emitter);
    vararg = tin_astemit_emitparamlist(emitter, &lambdaexpr->parameters, expr->line);
    if(lambdaexpr->body != NULL)
    {
        singleexpr = lambdaexpr->body->type == TINEXPR_EXPRESSION;
        if(singleexpr)
        {
            compiler.skipreturn = true;
            ((TinAstExprExpr*)lambdaexpr->body)->pop = false;
        }
        tin_astemit_emitexpression(emitter, lambdaexpr->body);
        if(singleexpr)
        {
            tin_astemit_emit1op(emitter, emitter->lastline, OP_RETURN);
        }
    }
    tin_astemit_endscope(emitter, emitter->lastline);
    function = tin_compiler_end(emitter, name);
    function->arg_count = lambdaexpr->parameters.count;
    function->maxslots += function->arg_count;
    function->vararg = vararg;
    if(function->upvalue_count > 0)
    {
        tin_astemit_emit1op(emitter, emitter->lastline, OP_CLOSURE);
        tin_astemit_emitshort(emitter, emitter->lastline, tin_astemit_addconstant(emitter, emitter->lastline, tin_value_fromobject(function)));
        for(i = 0; i < function->upvalue_count; i++)
        {
            tin_astemit_emit2bytes(emitter, emitter->lastline, compiler.upvalues[i].isLocal ? 1 : 0, compiler.upvalues[i].index);
        }
    }
    else
    {
        tin_astemit_emitconstant(emitter, emitter->lastline, tin_value_fromobject(function));
    }
    return true;
}

static bool tin_astemit_doemitarray(TinAstEmitter* emitter, TinAstExpression* expr)
{
    size_t i;
    TinAstArrayExpr* arrexpr;
    arrexpr = (TinAstArrayExpr*)expr;
    tin_astemit_emit1op(emitter, expr->line, OP_VALARRAY);
    for(i = 0; i < arrexpr->values.count; i++)
    {
        tin_astemit_emitexpression(emitter, arrexpr->values.values[i]);
        tin_astemit_emit1op(emitter, emitter->lastline, OP_PUSH_ARRAY_ELEMENT);
    }
    return true;
}

static bool tin_astemit_doemitobject(TinAstEmitter* emitter, TinAstExpression* expr)
{
    size_t i;
    TinAstObjectExpr* objexpr;
    objexpr = (TinAstObjectExpr*)expr;
    tin_astemit_emit1op(emitter, expr->line, OP_VALOBJECT);
    for(i = 0; i < objexpr->values.count; i++)
    {
        tin_astemit_emitexpression(emitter, objexpr->keys.values[i]);
        tin_astemit_emitexpression(emitter, objexpr->values.values[i]);
        tin_astemit_emit1op(emitter, emitter->lastline, OP_PUSH_OBJECT_FIELD);
    }
    return true;
}

static bool tin_astemit_doemitthis(TinAstEmitter* emitter, TinAstExpression* expr)
{
    int local;
    TinAstFuncType type;
    type = emitter->compiler->type;
    if(type == TINFUNC_STATICMETHOD)
    {
        tin_astemit_raiseerror(emitter, expr->line, "'this' cannot be used %s", "in static methods");
    }
    if(type == TINFUNC_CONSTRUCTOR || type == TINFUNC_METHOD)
    {
        tin_astemit_emitargedop(emitter, expr->line, OP_GET_LOCAL, 0);
    }
    else
    {
        if(emitter->compiler->enclosing == NULL)
        {
            tin_astemit_raiseerror(emitter, expr->line, "'this' cannot be used %s", "in functions outside of any class");
        }
        else
        {
            local = tin_astemit_resolvelocal(emitter, (TinAstCompiler*)emitter->compiler->enclosing, "this", 4, expr->line);
            tin_astemit_emitargedop(emitter, expr->line, OP_GET_UPVALUE,
                          tin_astemit_addupvalue(emitter, emitter->compiler, local, expr->line, true));
        }
    }
    return true;
}

static bool tin_astemit_doemitsuper(TinAstEmitter* emitter, TinAstExpression* expr)
{
    uint8_t index;
    TinAstSuperExpr* superexpr;
    if(emitter->compiler->type == TINFUNC_STATICMETHOD)
    {
        tin_astemit_raiseerror(emitter, expr->line, "'super' cannot be used %s", "in static methods");
    }
    else if(!emitter->classisinheriting)
    {
        tin_astemit_raiseerror(emitter, expr->line, "'super' cannot be used in class '%s', because it does not have a super class", emitter->classname->chars);
    }
    superexpr = (TinAstSuperExpr*)expr;
    if(!superexpr->ignemit)
    {
        index = tin_astemit_resolveupvalue(emitter, emitter->compiler, "super", 5, emitter->lastline);
        tin_astemit_emitargedop(emitter, expr->line, OP_GET_LOCAL, 0);
        tin_astemit_emitargedop(emitter, expr->line, OP_GET_UPVALUE, index);
        tin_astemit_emit1op(emitter, expr->line, OP_GET_SUPER_METHOD);
        tin_astemit_emitshort(emitter, expr->line, tin_astemit_addconstant(emitter, expr->line, tin_value_fromobject(superexpr->method)));
    }
    return true;
}

static bool tin_astemit_doemitternary(TinAstEmitter* emitter, TinAstExpression* expr)
{
    uint64_t endjump;
    uint64_t elsejump;
    TinAstTernaryExpr* ifexpr;
    ifexpr = (TinAstTernaryExpr*)expr;
    tin_astemit_emitexpression(emitter, ifexpr->condition);
    elsejump = tin_astemit_emitjump(emitter, OP_JUMP_IF_FALSE, expr->line);
    tin_astemit_emitexpression(emitter, ifexpr->ifbranch);
    endjump = tin_astemit_emitjump(emitter, OP_JUMP, emitter->lastline);
    tin_astemit_patchjump(emitter, elsejump, ifexpr->elsebranch->line);
    tin_astemit_emitexpression(emitter, ifexpr->elsebranch);
    tin_astemit_patchjump(emitter, endjump, emitter->lastline);
    return true;
}

static bool tin_astemit_doemitinterpolation(TinAstEmitter* emitter, TinAstExpression* expr)
{
    size_t i;
    TinAstStrInterExpr* ifexpr;
    ifexpr = (TinAstStrInterExpr*)expr;
    tin_astemit_emit1op(emitter, expr->line, OP_VALARRAY);
    for(i = 0; i < ifexpr->expressions.count; i++)
    {
        tin_astemit_emitexpression(emitter, ifexpr->expressions.values[i]);
        tin_astemit_emit1op(emitter, emitter->lastline, OP_PUSH_ARRAY_ELEMENT);
    }
    tin_astemit_emitvaryingop(emitter, emitter->lastline, OP_INVOKE, 0);
    tin_astemit_emitshort(emitter, emitter->lastline,
               tin_astemit_addconstant(emitter, emitter->lastline, tin_value_makestring(emitter->state, "join")));
    return true;
}

static bool tin_astemit_doemitreference(TinAstEmitter* emitter, TinAstExpression* expr)
{
    int old;
    TinAstExpression* to;
    to = ((TinAstRefExpr*)expr)->to;
    if(to->type != TINEXPR_VAREXPR && to->type != TINEXPR_GET && to->type != TINEXPR_THIS && to->type != TINEXPR_SUPER)
    {
        tin_astemit_raiseerror(emitter, expr->line, "invalid refence target");
        return false;
    }
    old = emitter->emitref;
    emitter->emitref++;
    tin_astemit_emitexpression(emitter, to);
    emitter->emitref = old;
    return true;
}

static bool tin_astemit_doemitvarstmt(TinAstEmitter* emitter, TinAstExpression* expr)
{
    bool isprivate;
    int index;
    size_t line;
    TinAstAssignVarExpr* varstmt;
    varstmt = (TinAstAssignVarExpr*)expr;
    line = expr->line;
    isprivate = emitter->compiler->enclosing == NULL && emitter->compiler->scope_depth == 0;
    index = isprivate ? tin_astemit_resolveprivate(emitter, varstmt->name, varstmt->length, expr->line) :
                          tin_astemit_addlocal(emitter, varstmt->name, varstmt->length, expr->line, varstmt->constant);
    if(varstmt->init == NULL)
    {
        tin_astemit_emit1op(emitter, line, OP_VALNULL);
    }
    else
    {
        tin_astemit_emitexpression(emitter, varstmt->init);
    }
    if(isprivate)
    {
        tin_astemit_markprivateinit(emitter, index);
    }
    else
    {
        tin_astemit_marklocalinit(emitter, index);
    }
    tin_astemit_emitbyteorshort(emitter, expr->line, isprivate ? OP_SET_PRIVATE : OP_SET_LOCAL,
                       isprivate ? OP_SET_PRIVATE_LONG : OP_SET_LOCAL_LONG, index);
    if(isprivate)
    {
        // Privates don't live on stack, so we need to pop them manually
        tin_astemit_emit1op(emitter, expr->line, OP_POP);
    }
    return true;
}

static bool tin_astemit_doemitifstmt(TinAstEmitter* emitter, TinAstExpression* expr)
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
        elsejump = tin_astemit_emitjump(emitter, OP_JUMP, expr->line);
    }
    else
    {
        tin_astemit_emitexpression(emitter, ifstmt->condition);
        elsejump = tin_astemit_emitjump(emitter, OP_JUMP_IF_FALSE, expr->line);
        tin_astemit_emitexpression(emitter, ifstmt->ifbranch);
        endjump = tin_astemit_emitjump(emitter, OP_JUMP, emitter->lastline);
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
            tin_astemit_patchjump(emitter, elsejump, e->line);
            tin_astemit_emitexpression(emitter, e);
            elsejump = tin_astemit_emitjump(emitter, OP_JUMP_IF_FALSE, emitter->lastline);
            tin_astemit_emitexpression(emitter, ifstmt->elseifbranches->values[i]);
            endjumps[i] = tin_astemit_emitjump(emitter, OP_JUMP, emitter->lastline);
        }
    }
    if(ifstmt->elsebranch != NULL)
    {
        tin_astemit_patchjump(emitter, elsejump, ifstmt->elsebranch->line);
        tin_astemit_emitexpression(emitter, ifstmt->elsebranch);
    }
    else
    {
        tin_astemit_patchjump(emitter, elsejump, emitter->lastline);
    }
    if(endjump != 0)
    {
        tin_astemit_patchjump(emitter, endjump, emitter->lastline);
    }
    if(ifstmt->elseifbranches != NULL)
    {
        for(i = 0; i < ifstmt->elseifbranches->count; i++)
        {
            if(ifstmt->elseifbranches->values[i] == NULL)
            {
                continue;
            }
            tin_astemit_patchjump(emitter, endjumps[i], ifstmt->elseifbranches->values[i]->line);
        }
    }
    free(endjumps);
    return true;
}

static bool tin_astemit_doemitblockstmt(TinAstEmitter* emitter, TinAstExpression* expr)
{
    size_t i;
    TinAstExpression* blockstmt;
    TinAstExprList* statements;
    statements = &((TinAstBlockExpr*)expr)->statements;
    tin_astemit_beginscope(emitter);
    {
        for(i = 0; i < statements->count; i++)
        {
            blockstmt = statements->values[i];
            if(tin_astemit_emitexpression(emitter, blockstmt))
            {
                break;
            }
        }
    }
    tin_astemit_endscope(emitter, emitter->lastline);
    return true;
}

static bool tin_astemit_doemitwhilestmt(TinAstEmitter* emitter, TinAstExpression* expr)
{
    size_t start;
    size_t exitjump;
    TinAstWhileExpr* whilestmt;
    whilestmt = (TinAstWhileExpr*)expr;
    start = emitter->chunk->count;
    emitter->loopstart = start;
    emitter->compiler->loopdepth++;
    tin_astemit_emitexpression(emitter, whilestmt->condition);
    exitjump = tin_astemit_emitjump(emitter, OP_JUMP_IF_FALSE, expr->line);
    tin_astemit_emitexpression(emitter, whilestmt->body);
    tin_astemit_patchloopjumps(emitter, &emitter->continues, emitter->lastline);
    tin_astemit_emitloop(emitter, start, emitter->lastline);
    tin_astemit_patchjump(emitter, exitjump, emitter->lastline);
    tin_astemit_patchloopjumps(emitter, &emitter->breaks, emitter->lastline);
    emitter->compiler->loopdepth--;
    return true;
}

static bool tin_astemit_doemitforstmt(TinAstEmitter* emitter, TinAstExpression* expr)
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
    tin_astemit_beginscope(emitter);
    emitter->compiler->loopdepth++;
    if(forstmt->cstyle)
    {
        if(forstmt->var != NULL)
        {
            tin_astemit_emitexpression(emitter, forstmt->var);
        }
        else if(forstmt->init != NULL)
        {
            tin_astemit_emitexpression(emitter, forstmt->init);
        }
        start = emitter->chunk->count;
        exitjump = 0;
        if(forstmt->condition != NULL)
        {
            tin_astemit_emitexpression(emitter, forstmt->condition);
            exitjump = tin_astemit_emitjump(emitter, OP_JUMP_IF_FALSE, emitter->lastline);
        }
        if(forstmt->increment != NULL)
        {
            bodyjump = tin_astemit_emitjump(emitter, OP_JUMP, emitter->lastline);
            incrstart = emitter->chunk->count;
            tin_astemit_emitexpression(emitter, forstmt->increment);
            tin_astemit_emit1op(emitter, emitter->lastline, OP_POP);
            tin_astemit_emitloop(emitter, start, emitter->lastline);
            start = incrstart;
            tin_astemit_patchjump(emitter, bodyjump, emitter->lastline);
        }
        emitter->loopstart = start;
        tin_astemit_beginscope(emitter);
        if(forstmt->body != NULL)
        {
            if(forstmt->body->type == TINEXPR_BLOCK)
            {
                statements = &((TinAstBlockExpr*)forstmt->body)->statements;
                for(i = 0; i < statements->count; i++)
                {
                    tin_astemit_emitexpression(emitter, statements->values[i]);
                }
            }
            else
            {
                tin_astemit_emitexpression(emitter, forstmt->body);
            }
        }
        tin_astemit_patchloopjumps(emitter, &emitter->continues, emitter->lastline);
        tin_astemit_endscope(emitter, emitter->lastline);
        tin_astemit_emitloop(emitter, start, emitter->lastline);
        if(forstmt->condition != NULL)
        {
            tin_astemit_patchjump(emitter, exitjump, emitter->lastline);
        }
    }
    else
    {
        {
            varstr = "seq ";
            sequence = tin_astemit_addlocal(emitter, varstr, strlen(varstr), expr->line, false);
            tin_astemit_marklocalinit(emitter, sequence);
            tin_astemit_emitexpression(emitter, forstmt->condition);
            tin_astemit_emitbyteorshort(emitter, emitter->lastline, OP_SET_LOCAL, OP_SET_LOCAL_LONG, sequence);
        }
        {
            varstr = "iter ";
            iterator = tin_astemit_addlocal(emitter, varstr, strlen(varstr), expr->line, false);
            tin_astemit_marklocalinit(emitter, iterator);
            tin_astemit_emit1op(emitter, emitter->lastline, OP_VALNULL);
            tin_astemit_emitbyteorshort(emitter, emitter->lastline, OP_SET_LOCAL, OP_SET_LOCAL_LONG, iterator);
        }
        start = emitter->chunk->count;
        emitter->loopstart = emitter->chunk->count;
        // iter = seq.iterator(iter)
        tin_astemit_emitbyteorshort(emitter, emitter->lastline, OP_GET_LOCAL, OP_GET_LOCAL_LONG, sequence);
        tin_astemit_emitbyteorshort(emitter, emitter->lastline, OP_GET_LOCAL, OP_GET_LOCAL_LONG, iterator);
        tin_astemit_emitvaryingop(emitter, emitter->lastline, OP_INVOKE, 1);
        tin_astemit_emitshort(emitter, emitter->lastline,
                   tin_astemit_addconstant(emitter, emitter->lastline, tin_value_makestring(emitter->state, "iterator")));
        tin_astemit_emitbyteorshort(emitter, emitter->lastline, OP_SET_LOCAL, OP_SET_LOCAL_LONG, iterator);
        // If iter is null, just get out of the loop
        exitjump = tin_astemit_emitjump(emitter, OP_JUMP_IF_NULL_POPPING, emitter->lastline);
        tin_astemit_beginscope(emitter);
        // var i = seq.iteratorValue(iter)
        var = (TinAstAssignVarExpr*)forstmt->var;
        localcnt = tin_astemit_addlocal(emitter, var->name, var->length, expr->line, false);
        tin_astemit_marklocalinit(emitter, localcnt);
        tin_astemit_emitbyteorshort(emitter, emitter->lastline, OP_GET_LOCAL, OP_GET_LOCAL_LONG, sequence);
        tin_astemit_emitbyteorshort(emitter, emitter->lastline, OP_GET_LOCAL, OP_GET_LOCAL_LONG, iterator);
        tin_astemit_emitvaryingop(emitter, emitter->lastline, OP_INVOKE, 1);
        tin_astemit_emitshort(emitter, emitter->lastline,
                   tin_astemit_addconstant(emitter, emitter->lastline, tin_value_makestring(emitter->state, "iteratorValue")));
        tin_astemit_emitbyteorshort(emitter, emitter->lastline, OP_SET_LOCAL, OP_SET_LOCAL_LONG, localcnt);
        if(forstmt->body != NULL)
        {
            if(forstmt->body->type == TINEXPR_BLOCK)
            {
                statements = &((TinAstBlockExpr*)forstmt->body)->statements;
                for(i = 0; i < statements->count; i++)
                {
                    tin_astemit_emitexpression(emitter, statements->values[i]);
                }
            }
            else
            {
                tin_astemit_emitexpression(emitter, forstmt->body);
            }
        }
        tin_astemit_patchloopjumps(emitter, &emitter->continues, emitter->lastline);
        tin_astemit_endscope(emitter, emitter->lastline);
        tin_astemit_emitloop(emitter, start, emitter->lastline);
        tin_astemit_patchjump(emitter, exitjump, emitter->lastline);
    }
    tin_astemit_patchloopjumps(emitter, &emitter->breaks, emitter->lastline);
    tin_astemit_endscope(emitter, emitter->lastline);
    emitter->compiler->loopdepth--;
    return true;
}

static bool tin_astemit_doemitbreak(TinAstEmitter* emitter, TinAstExpression* expr)
{
    int depth;
    int ii;
    uint16_t local_count;
    TinAstLocal* local;
    TinAstLocList* locals;
    if(emitter->compiler->loopdepth == 0)
    {
        tin_astemit_raiseerror(emitter, expr->line, "cannot use '%s' outside of loops", "break");
    }
    tin_astemit_emit1op(emitter, expr->line, OP_POP_LOCALS);
    depth = emitter->compiler->scope_depth;
    local_count = 0;
    locals = &emitter->compiler->locals;
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
    tin_astemit_emitshort(emitter, expr->line, local_count);
    tin_uintlist_push(emitter->state, &emitter->breaks, tin_astemit_emitjump(emitter, OP_JUMP, expr->line));
    return true;
}

static bool tin_astemit_doemitcontinue(TinAstEmitter* emitter, TinAstExpression* expr)
{
    if(emitter->compiler->loopdepth == 0)
    {
        tin_astemit_raiseerror(emitter, expr->line, "cannot use '%s' outside of loops", "continue");
    }
    tin_uintlist_push(emitter->state, &emitter->continues, tin_astemit_emitjump(emitter, OP_JUMP, expr->line));
    return true;
}

static bool tin_astemit_doemitreturn(TinAstEmitter* emitter, TinAstExpression* expr)
{
    TinAstExpression* expression;
    if(emitter->compiler->type == TINFUNC_CONSTRUCTOR)
    {
        tin_astemit_raiseerror(emitter, expr->line, "cannot use 'return' in constructors");
        return false;
    }
    expression = ((TinAstReturnExpr*)expr)->expression;
    if(expression == NULL)
    {
        tin_astemit_emit1op(emitter, emitter->lastline, OP_VALNULL);
    }
    else
    {
        tin_astemit_emitexpression(emitter, expression);
    }
    tin_astemit_emit1op(emitter, emitter->lastline, OP_RETURN);
    if(emitter->compiler->scope_depth == 0)
    {
        emitter->compiler->skipreturn = true;
    }
    return true;
}

static bool tin_astemit_doemitfunction(TinAstEmitter* emitter, TinAstExpression* expr)
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
    isprivate = !isexport && emitter->compiler->enclosing == NULL && emitter->compiler->scope_depth == 0;
    islocal = !(isexport || isprivate);
    index = 0;
    if(!isexport)
    {
        index = isprivate ? tin_astemit_resolveprivate(emitter, funcstmt->name, funcstmt->length, expr->line) :
                          tin_astemit_addlocal(emitter, funcstmt->name, funcstmt->length, expr->line, false);
    }
    name = tin_string_copy(emitter->state, funcstmt->name, funcstmt->length);
    if(islocal)
    {
        tin_astemit_marklocalinit(emitter, index);
    }
    else if(isprivate)
    {
        tin_astemit_markprivateinit(emitter, index);
    }
    tin_compiler_compiler(emitter, &compiler, TINFUNC_REGULAR);
    tin_astemit_beginscope(emitter);
    vararg = tin_astemit_emitparamlist(emitter, &funcstmt->parameters, expr->line);
    tin_astemit_emitexpression(emitter, funcstmt->body);
    tin_astemit_endscope(emitter, emitter->lastline);
    function = tin_compiler_end(emitter, name);
    function->arg_count = funcstmt->parameters.count;
    function->maxslots += function->arg_count;
    function->vararg = vararg;
    if(function->upvalue_count > 0)
    {
        tin_astemit_emit1op(emitter, emitter->lastline, OP_CLOSURE);
        tin_astemit_emitshort(emitter, emitter->lastline, tin_astemit_addconstant(emitter, emitter->lastline, tin_value_fromobject(function)));
        for(i = 0; i < function->upvalue_count; i++)
        {
            tin_astemit_emit2bytes(emitter, emitter->lastline, compiler.upvalues[i].isLocal ? 1 : 0, compiler.upvalues[i].index);
        }
    }
    else
    {
        tin_astemit_emitconstant(emitter, emitter->lastline, tin_value_fromobject(function));
    }
    if(isexport)
    {
        tin_astemit_emit1op(emitter, emitter->lastline, OP_SET_GLOBAL);
        tin_astemit_emitshort(emitter, emitter->lastline, tin_astemit_addconstant(emitter, emitter->lastline, tin_value_fromobject(name)));
    }
    else if(isprivate)
    {
        tin_astemit_emitbyteorshort(emitter, emitter->lastline, OP_SET_PRIVATE, OP_SET_PRIVATE_LONG, index);
    }
    else
    {
        tin_astemit_emitbyteorshort(emitter, emitter->lastline, OP_SET_LOCAL, OP_SET_LOCAL_LONG, index);
    }
    tin_astemit_emit1op(emitter, emitter->lastline, OP_POP);
    return true;
}

static bool tin_astemit_doemitmethod(TinAstEmitter* emitter, TinAstExpression* expr)
{
    bool vararg;
    bool constructor;
    size_t i;
    TinAstCompiler compiler;
    TinFunction* function;
    TinAstMethodExpr* mthstmt;
    mthstmt = (TinAstMethodExpr*)expr;
    constructor = memcmp(mthstmt->name->chars, "constructor", 11) == 0;
    if(constructor && mthstmt->isstatic)
    {
        tin_astemit_raiseerror(emitter, expr->line, "constructors cannot be static (at least for now)");
        return false;
    }
    tin_compiler_compiler(emitter, &compiler,
                  constructor ? TINFUNC_CONSTRUCTOR : (mthstmt->isstatic ? TINFUNC_STATICMETHOD : TINFUNC_METHOD));
    tin_astemit_beginscope(emitter);
    vararg = tin_astemit_emitparamlist(emitter, &mthstmt->parameters, expr->line);
    tin_astemit_emitexpression(emitter, mthstmt->body);
    tin_astemit_endscope(emitter, emitter->lastline);
    function = tin_compiler_end(emitter, tin_value_asstring(tin_string_format(emitter->state, "@:@", tin_value_fromobject(emitter->classname), tin_value_fromobject(mthstmt->name))));
    function->arg_count = mthstmt->parameters.count;
    function->maxslots += function->arg_count;
    function->vararg = vararg;
    if(function->upvalue_count > 0)
    {
        tin_astemit_emit1op(emitter, emitter->lastline, OP_CLOSURE);
        tin_astemit_emitshort(emitter, emitter->lastline, tin_astemit_addconstant(emitter, emitter->lastline, tin_value_fromobject(function)));
        for(i = 0; i < function->upvalue_count; i++)
        {
            tin_astemit_emit2bytes(emitter, emitter->lastline, compiler.upvalues[i].isLocal ? 1 : 0, compiler.upvalues[i].index);
        }
    }
    else
    {
        tin_astemit_emitconstant(emitter, emitter->lastline, tin_value_fromobject(function));
    }
    tin_astemit_emit1op(emitter, emitter->lastline, mthstmt->isstatic ? OP_STATIC_FIELD : OP_METHOD);
    tin_astemit_emitshort(emitter, emitter->lastline, tin_astemit_addconstant(emitter, expr->line, tin_value_fromobject(mthstmt->name)));
    return true;
}

static bool tin_astemit_doemitclass(TinAstEmitter* emitter, TinAstExpression* expr)
{
    size_t i;
    uint8_t superidx;
    const char* varstr;
    TinAstExpression* s;
    TinAstClassExpr* clstmt;
    TinAstAssignVarExpr* var;
    clstmt = (TinAstClassExpr*)expr;
    emitter->classname = clstmt->name;
    if(clstmt->parent != NULL)
    {
        tin_astemit_emit1op(emitter, emitter->lastline, OP_GET_GLOBAL);
        tin_astemit_emitshort(emitter, emitter->lastline, tin_astemit_addconstant(emitter, emitter->lastline, tin_value_fromobject(clstmt->parent)));
    }
    tin_astemit_emit1op(emitter, expr->line, OP_CLASS);
    tin_astemit_emitshort(emitter, emitter->lastline, tin_astemit_addconstant(emitter, emitter->lastline, tin_value_fromobject(clstmt->name)));
    if(clstmt->parent != NULL)
    {
        tin_astemit_emit1op(emitter, emitter->lastline, OP_INHERIT);
        emitter->classisinheriting = true;
        tin_astemit_beginscope(emitter);
        varstr = "super";
        superidx = tin_astemit_addlocal(emitter, varstr, strlen(varstr), emitter->lastline, false);
        tin_astemit_marklocalinit(emitter, superidx);
    }
    for(i = 0; i < clstmt->fields.count; i++)
    {
        s = clstmt->fields.values[i];
        if(s->type == TINEXPR_VARSTMT)
        {
            var = (TinAstAssignVarExpr*)s;
            tin_astemit_emitexpression(emitter, var->init);
            tin_astemit_emit1op(emitter, expr->line, OP_STATIC_FIELD);
            tin_astemit_emitshort(emitter, expr->line,
                       tin_astemit_addconstant(emitter, expr->line,
                                    tin_value_fromobject(tin_string_copy(emitter->state, var->name, var->length))));
        }
        else
        {
            tin_astemit_emitexpression(emitter, s);
        }
    }
    tin_astemit_emit1op(emitter, emitter->lastline, OP_POP);
    if(clstmt->parent != NULL)
    {
        tin_astemit_endscope(emitter, emitter->lastline);
    }
    emitter->classname = NULL;
    emitter->classisinheriting = false;
    return true;
}

static bool tin_astemit_doemitfield(TinAstEmitter* emitter, TinAstExpression* expr)
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
        tin_compiler_compiler(emitter, &compiler, fieldstmt->isstatic ? TINFUNC_STATICMETHOD : TINFUNC_METHOD);
        tin_astemit_beginscope(emitter);
        tin_astemit_emitexpression(emitter, fieldstmt->getter);
        tin_astemit_endscope(emitter, emitter->lastline);
        getter = tin_compiler_end(emitter,
            tin_value_asstring(tin_string_format(emitter->state, "@:get @", tin_value_fromobject(emitter->classname), fieldstmt->name)));
    }
    if(fieldstmt->setter != NULL)
    {
        valstr = "value";
        tin_compiler_compiler(emitter, &compiler, fieldstmt->isstatic ? TINFUNC_STATICMETHOD : TINFUNC_METHOD);
        tin_astemit_marklocalinit(emitter, tin_astemit_addlocal(emitter, valstr, strlen(valstr), expr->line, false));
        tin_astemit_beginscope(emitter);
        tin_astemit_emitexpression(emitter, fieldstmt->setter);
        tin_astemit_endscope(emitter, emitter->lastline);
        setter = tin_compiler_end(emitter,
            tin_value_asstring(tin_string_format(emitter->state, "@:set @", tin_value_fromobject(emitter->classname), fieldstmt->name)));
        setter->arg_count = 1;
        setter->maxslots++;
    }
    field = tin_create_field(emitter->state, (TinObject*)getter, (TinObject*)setter);
    tin_astemit_emitconstant(emitter, expr->line, tin_value_fromobject(field));
    tin_astemit_emit1op(emitter, expr->line, fieldstmt->isstatic ? OP_STATIC_FIELD : OP_DEFINE_FIELD);
    tin_astemit_emitshort(emitter, expr->line, tin_astemit_addconstant(emitter, expr->line, tin_value_fromobject(fieldstmt->name)));
    return true;
}

static bool tin_astemit_doemitrange(TinAstEmitter* emitter, TinAstExpression* expr)
{
    TinAstRangeExpr* rangeexpr;
    rangeexpr = (TinAstRangeExpr*)expr;
    tin_astemit_emitexpression(emitter, rangeexpr->to);
    tin_astemit_emitexpression(emitter, rangeexpr->from);
    tin_astemit_emit1op(emitter, expr->line, OP_RANGE);
    return true;
}

static bool tin_astemit_doemitsubscript(TinAstEmitter* emitter, TinAstExpression* expr)
{
    TinAstIndexExpr* subscrexpr;
    subscrexpr = (TinAstIndexExpr*)expr;
    tin_astemit_emitexpression(emitter, subscrexpr->array);
    tin_astemit_emitexpression(emitter, subscrexpr->index);
    tin_astemit_emit1op(emitter, expr->line, OP_SUBSCRIPT_GET);
    return true;
}

static bool tin_astemit_doemitexpression(TinAstEmitter* emitter, TinAstExpression* expr)
{
    TinAstExprExpr* stmtexpr;
    stmtexpr = (TinAstExprExpr*)expr;
    tin_astemit_emitexpression(emitter, stmtexpr->expression);
    if(stmtexpr->pop)
    {
        tin_astemit_emit1op(emitter, expr->line, OP_POP);
    }
    return true;
}

static bool tin_astemit_emitexpression(TinAstEmitter* emitter, TinAstExpression* expr)
{
    if(expr == NULL)
    {
        return false;
    }
    switch(expr->type)
    {
        case TINEXPR_LITERAL:
            {
                if(!tin_astemit_doemitliteral(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_BINARY:
            {
                if(!tin_astemit_doemitbinary(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_UNARY:
            {
                if(!tin_astemit_doemitunary(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_VAREXPR:
            {
                if(!tin_astemit_doemitvarexpr(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_ASSIGN:
            {
                if(!tin_astemit_doemitassign(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_CALL:
            {
                if(!tin_astemit_doemitcall(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_GET:
            {
                if(!tin_astemit_doemitget(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_SET:
            {
                if(!tin_astemit_doemitset(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_LAMBDA:
            {
                if(!tin_astemit_doemitlambda(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_ARRAY:
            {
                if(!tin_astemit_doemitarray(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_OBJECT:
            {
                if(!tin_astemit_doemitobject(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_SUBSCRIPT:
            {
                if(!tin_astemit_doemitsubscript(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_THIS:
            {
                if(!tin_astemit_doemitthis(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_SUPER:
            {
                if(!tin_astemit_doemitsuper(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_RANGE:
            {
                if(!tin_astemit_doemitrange(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_TERNARY:
            {
                if(!tin_astemit_doemitternary(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_INTERPOLATION:
            {
                if(!tin_astemit_doemitinterpolation(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_REFERENCE:
            {
                if(!tin_astemit_doemitreference(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_EXPRESSION:
            {
                if(!tin_astemit_doemitexpression(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_BLOCK:
            {
                if(!tin_astemit_doemitblockstmt(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_VARSTMT:
            {
                if(!tin_astemit_doemitvarstmt(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_IFSTMT:
            {
                if(!tin_astemit_doemitifstmt(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_WHILE:
            {
                if(!tin_astemit_doemitwhilestmt(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_FOR:
            {
                if(!tin_astemit_doemitforstmt(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_CONTINUE:
            {
                if(!tin_astemit_doemitcontinue(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_BREAK:
            {
                if(!tin_astemit_doemitbreak(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_FUNCTION:
            {
                if(!tin_astemit_doemitfunction(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_RETURN:
            {
                if(!tin_astemit_doemitreturn(emitter, expr))
                {
                    return false;
                }
                //return true;
            }
            break;
        case TINEXPR_METHOD:
            {
                if(!tin_astemit_doemitmethod(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_CLASS:
            {
                if(!tin_astemit_doemitclass(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case TINEXPR_FIELD:
            {
                if(!tin_astemit_doemitfield(emitter, expr))
                {
                    return false;
                }
            }
            break;
        default:
            {
                tin_astemit_raiseerror(emitter, expr->line, "unknown expression with id '%i'", (int)expr->type);
            }
            break;
    }
    emitter->prevwasexprstmt = expr->type == TINEXPR_EXPRESSION;
    return false;
}

TinModule* tin_astemit_modemit(TinAstEmitter* emitter, TinAstExprList* statements, TinString* module_name)
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
    emitter->lastline = 1;
    emitter->emitref = 0;
    state = emitter->state;
    isnew = false;
    if(tin_table_get(&emitter->state->vm->modules->values, module_name, &modulevalue))
    {
        module = tin_value_asmodule(modulevalue);
    }
    else
    {
        module = tin_object_makemodule(emitter->state, module_name);
        isnew = true;
    }
    emitter->module = module;
    oldprivatescnt = module->private_count;
    if(oldprivatescnt > 0)
    {
        privates = &emitter->privates;
        privates->count = oldprivatescnt - 1;
        tin_privlist_push(state, privates, (TinAstPrivate){ true, false });
        for(i = 0; i < oldprivatescnt; i++)
        {
            privates->values[i].initialized = true;
        }
    }
    tin_compiler_compiler(emitter, &compiler, TINFUNC_SCRIPT);
    emitter->chunk = &compiler.function->chunk;
    resolve_statements(emitter, statements);
    for(i = 0; i < statements->count; i++)
    {
        exstmt = statements->values[i];
        if(tin_astemit_emitexpression(emitter, exstmt))
        {
            break;
        }
    }
    tin_astemit_endscope(emitter, emitter->lastline);
    module->main_function = tin_compiler_end(emitter, module_name);
    if(isnew)
    {
        total = emitter->privates.count;
        module->privates = TIN_ALLOCATE(emitter->state, sizeof(TinValue), total);
        for(i = 0; i < total; i++)
        {
            module->privates[i] = NULL_VALUE;
        }
    }
    else
    {
        module->privates = TIN_GROW_ARRAY(emitter->state, module->privates, sizeof(TinValue), oldprivatescnt, module->private_count);
        for(i = oldprivatescnt; i < module->private_count; i++)
        {
            module->privates[i] = NULL_VALUE;
        }
    }
    tin_privlist_destroy(emitter->state, &emitter->privates);
    if(tin_astopt_isoptenabled(TINOPTSTATE_PRIVATENAMES))
    {
        tin_table_destroy(emitter->state, &emitter->module->private_names->values);
    }
    if(isnew && !state->haderror)
    {
        tin_table_set(state, &state->vm->modules->values, module_name, tin_value_fromobject(module));
    }
    module->ran = true;
    return module;
}
