
#include <string.h>
#include "priv.h"

static bool lit_astemit_emitexpression(LitAstEmitter* emitter, LitAstExpression* expression);
static void lit_astemit_resolvestmt(LitAstEmitter* emitter, LitAstExpression* statement);

static inline void lit_uintlist_init(LitUintList* array)
{
    lit_datalist_init(&array->list, sizeof(size_t));
}

static inline void lit_uintlist_destroy(LitState* state, LitUintList* array)
{
    lit_datalist_destroy(state, &array->list);
}

static inline void lit_uintlist_push(LitState* state, LitUintList* array, size_t value)
{
    lit_datalist_push(state, &array->list, value);
}

static inline size_t lit_uintlist_get(LitUintList* array, size_t idx)
{
    return (size_t)lit_datalist_get(&array->list, idx);
}

static inline size_t lit_uintlist_count(LitUintList* array)
{
    return lit_datalist_count(&array->list);
}

static void resolve_statements(LitAstEmitter* emitter, LitAstExprList* statements)
{
    size_t i;
    for(i = 0; i < statements->count; i++)
    {
        lit_astemit_resolvestmt(emitter, statements->values[i]);
    }
}

void lit_privlist_init(LitAstPrivList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_privlist_destroy(LitState* state, LitAstPrivList* array)
{
    LIT_FREE_ARRAY(state, sizeof(LitAstPrivate), array->values, array->capacity);
    lit_privlist_init(array);
}

void lit_privlist_push(LitState* state, LitAstPrivList* array, LitAstPrivate value)
{
    if(array->capacity < array->count + 1)
    {
        size_t oldcapacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(oldcapacity);
        array->values = LIT_GROW_ARRAY(state, array->values, sizeof(LitAstPrivate), oldcapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}
void lit_loclist_init(LitAstLocList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void lit_loclist_destroy(LitState* state, LitAstLocList* array)
{
    LIT_FREE_ARRAY(state, sizeof(LitAstLocal), array->values, array->capacity);
    lit_loclist_init(array);
}

void lit_loclist_push(LitState* state, LitAstLocList* array, LitAstLocal value)
{
    if(array->capacity < array->count + 1)
    {
        size_t oldcapacity = array->capacity;
        array->capacity = LIT_GROW_CAPACITY(oldcapacity);
        array->values = LIT_GROW_ARRAY(state, array->values, sizeof(LitAstLocal), oldcapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

static void lit_astemit_raiseerror(LitAstEmitter* emitter, size_t line, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    lit_state_raiseerror(emitter->state, COMPILE_ERROR, lit_vformat_error(emitter->state, line, fmt, args)->chars);
    va_end(args);
}


void lit_astemit_init(LitState* state, LitAstEmitter* emitter)
{
    emitter->state = state;
    emitter->loop_start = 0;
    emitter->emit_reference = 0;
    emitter->class_name = NULL;
    emitter->compiler = NULL;
    emitter->chunk = NULL;
    emitter->module = NULL;
    emitter->previous_was_expression_statement = false;
    emitter->class_has_super = false;

    lit_privlist_init(&emitter->privates);
    lit_uintlist_init(&emitter->breaks);
    lit_uintlist_init(&emitter->continues);
}

void lit_astemit_destroy(LitAstEmitter* emitter)
{
    lit_uintlist_destroy(emitter->state, &emitter->breaks);
    lit_uintlist_destroy(emitter->state, &emitter->continues);
}

static void lit_astemit_emit1byte(LitAstEmitter* emitter, uint16_t line, uint8_t byte)
{
    if(line < emitter->last_line)
    {
        // Egor-fail proofing
        line = emitter->last_line;
    }

    lit_chunk_push(emitter->state, emitter->chunk, byte, line);
    emitter->last_line = line;
}

static const int8_t stack_effects[] = {
#define OPCODE(_, effect) effect,
#include "opcodes.inc"
#undef OPCODE
};

static void lit_astemit_emit2bytes(LitAstEmitter* emitter, uint16_t line, uint8_t a, uint8_t b)
{
    if(line < emitter->last_line)
    {
        // Egor-fail proofing
        line = emitter->last_line;
    }
    lit_chunk_push(emitter->state, emitter->chunk, a, line);
    lit_chunk_push(emitter->state, emitter->chunk, b, line);
    emitter->last_line = line;
}

static void lit_astemit_emit1op(LitAstEmitter* emitter, uint16_t line, LitOpCode op)
{
    LitAstCompiler* compiler;
    compiler = emitter->compiler;
    lit_astemit_emit1byte(emitter, line, (uint8_t)op);
    compiler->slots += stack_effects[(int)op];

    if(compiler->slots > (int)compiler->function->max_slots)
    {
        compiler->function->max_slots = (size_t)compiler->slots;
    }
}

static void lit_astemit_emit2ops(LitAstEmitter* emitter, uint16_t line, LitOpCode a, LitOpCode b)
{
    LitAstCompiler* compiler;
    compiler = emitter->compiler;
    lit_astemit_emit2bytes(emitter, line, (uint8_t)a, (uint8_t)b);
    compiler->slots += stack_effects[(int)a] + stack_effects[(int)b];
    if(compiler->slots > (int)compiler->function->max_slots)
    {
        compiler->function->max_slots = (size_t)compiler->slots;
    }
}

static void lit_astemit_emitvaryingop(LitAstEmitter* emitter, uint16_t line, LitOpCode op, uint8_t arg)
{
    LitAstCompiler* compiler;
    compiler = emitter->compiler;
    lit_astemit_emit2bytes(emitter, line, (uint8_t)op, arg);
    compiler->slots -= arg;
    if(compiler->slots > (int)compiler->function->max_slots)
    {
        compiler->function->max_slots = (size_t)compiler->slots;
    }
}

static void lit_astemit_emitargedop(LitAstEmitter* emitter, uint16_t line, LitOpCode op, uint8_t arg)
{
    LitAstCompiler* compiler = emitter->compiler;

    lit_astemit_emit2bytes(emitter, line, (uint8_t)op, arg);
    compiler->slots += stack_effects[(int)op];

    if(compiler->slots > (int)compiler->function->max_slots)
    {
        compiler->function->max_slots = (size_t)compiler->slots;
    }
}

static void lit_astemit_emitshort(LitAstEmitter* emitter, uint16_t line, uint16_t value)
{
    lit_astemit_emit2bytes(emitter, line, (uint8_t)((value >> 8) & 0xff), (uint8_t)(value & 0xff));
}

static void lit_astemit_emitbyteorshort(LitAstEmitter* emitter, uint16_t line, uint8_t a, uint8_t b, uint16_t index)
{
    if(index > UINT8_MAX)
    {
        lit_astemit_emit1op(emitter, line, (LitOpCode)b);
        lit_astemit_emitshort(emitter, line, (uint16_t)index);
    }
    else
    {
        lit_astemit_emitargedop(emitter, line, (LitOpCode)a, (uint8_t)index);
    }
}

static void lit_compiler_compiler(LitAstEmitter* emitter, LitAstCompiler* compiler, LitAstFuncType type)
{
    lit_loclist_init(&compiler->locals);

    compiler->type = type;
    compiler->scope_depth = 0;
    compiler->enclosing = (struct LitAstCompiler*)emitter->compiler;
    compiler->skip_return = false;
    compiler->function = lit_object_makefunction(emitter->state, emitter->module);
    compiler->loop_depth = 0;

    emitter->compiler = compiler;

    const char* name = emitter->state->scanner->file_name;

    if(emitter->compiler == NULL)
    {
        compiler->function->name = lit_string_copy(emitter->state, name, strlen(name));
    }

    emitter->chunk = &compiler->function->chunk;

    if(lit_astopt_isoptenabled(LITOPTSTATE_LINE_INFO))
    {
        emitter->chunk->has_line_info = false;
    }

    if(type == LITFUNC_METHOD || type == LITFUNC_STATIC_METHOD || type == LITFUNC_CONSTRUCTOR)
    {
        lit_loclist_push(emitter->state, &compiler->locals, (LitAstLocal){ "this", 4, -1, false, false });
    }
    else
    {
        lit_loclist_push(emitter->state, &compiler->locals, (LitAstLocal){ "", 0, -1, false, false });
    }

    compiler->slots = 1;
    compiler->max_slots = 1;
}

static void lit_astemit_emitreturn(LitAstEmitter* emitter, size_t line)
{
    if(emitter->compiler->type == LITFUNC_CONSTRUCTOR)
    {
        lit_astemit_emitargedop(emitter, line, OP_GET_LOCAL, 0);
        lit_astemit_emit1op(emitter, line, OP_RETURN);
    }
    else if(emitter->previous_was_expression_statement && emitter->chunk->count > 0)
    {
        emitter->chunk->count--;// Remove the OP_POP
        lit_astemit_emit1op(emitter, line, OP_RETURN);
    }
    else
    {
        lit_astemit_emit2ops(emitter, line, OP_NULL, OP_RETURN);
    }
}

static LitFunction* lit_compiler_end(LitAstEmitter* emitter, LitString* name)
{
    if(!emitter->compiler->skip_return)
    {
        lit_astemit_emitreturn(emitter, emitter->last_line);
        emitter->compiler->skip_return = true;
    }

    LitFunction* function = emitter->compiler->function;

    lit_loclist_destroy(emitter->state, &emitter->compiler->locals);

    emitter->compiler = (LitAstCompiler*)emitter->compiler->enclosing;
    emitter->chunk = emitter->compiler == NULL ? NULL : &emitter->compiler->function->chunk;

    if(name != NULL)
    {
        function->name = name;
    }

#ifdef LIT_TRACE_CHUNK
    lit_disassemble_chunk(&function->chunk, function->name->chars, NULL);
#endif

    return function;
}

static void lit_astemit_beginscope(LitAstEmitter* emitter)
{
    emitter->compiler->scope_depth++;
}

static void lit_astemit_endscope(LitAstEmitter* emitter, uint16_t line)
{
    emitter->compiler->scope_depth--;

    LitAstCompiler* compiler = emitter->compiler;
    LitAstLocList* locals = &compiler->locals;

    while(locals->count > 0 && locals->values[locals->count - 1].depth > compiler->scope_depth)
    {
        if(locals->values[locals->count - 1].captured)
        {
            lit_astemit_emit1op(emitter, line, OP_CLOSE_UPVALUE);
        }
        else
        {
            lit_astemit_emit1op(emitter, line, OP_POP);
        }

        locals->count--;
    }
}


static uint16_t lit_astemit_addconstant(LitAstEmitter* emitter, size_t line, LitValue value)
{
    size_t constant = lit_chunk_addconst(emitter->state, emitter->chunk, value);

    if(constant >= UINT16_MAX)
    {
        lit_astemit_raiseerror(emitter, line, "too many constants for one chunk");
    }

    return constant;
}

static size_t lit_astemit_emitconstant(LitAstEmitter* emitter, size_t line, LitValue value)
{
    size_t constant = lit_chunk_addconst(emitter->state, emitter->chunk, value);

    if(constant < UINT8_MAX)
    {
        lit_astemit_emitargedop(emitter, line, OP_CONSTANT, constant);
    }
    else if(constant < UINT16_MAX)
    {
        lit_astemit_emit1op(emitter, line, OP_CONSTANT_LONG);
        lit_astemit_emitshort(emitter, line, constant);
    }
    else
    {
        lit_astemit_raiseerror(emitter, line, "too many constants for one chunk");
    }

    return constant;
}

static int lit_astemit_addprivate(LitAstEmitter* emitter, const char* name, size_t length, size_t line, bool constant)
{
    LitAstPrivList* privates = &emitter->privates;

    if(privates->count == UINT16_MAX)
    {
        lit_astemit_raiseerror(emitter, line, "too many private locals for one module");
    }

    LitTable* private_names = &emitter->module->private_names->values;
    LitString* key = lit_table_find_string(private_names, name, length, lit_util_hashstring(name, length));

    if(key != NULL)
    {
        lit_astemit_raiseerror(emitter, line, "variable '%.*s' was already declared in this scope", length, name);

        LitValue index;
        lit_table_get(private_names, key, &index);

        return lit_value_asnumber(index);
    }

    LitState* state = emitter->state;
    int index = (int)privates->count;

    lit_privlist_push(state, privates, (LitAstPrivate){ false, constant });

    lit_table_set(state, private_names, lit_string_copy(state, name, length), lit_value_makenumber(state, index));
    emitter->module->private_count++;

    return index;
}

static int lit_astemit_resolveprivate(LitAstEmitter* emitter, const char* name, size_t length, size_t line)
{
    LitTable* private_names = &emitter->module->private_names->values;
    LitString* key = lit_table_find_string(private_names, name, length, lit_util_hashstring(name, length));

    if(key != NULL)
    {
        LitValue index;
        lit_table_get(private_names, key, &index);

        int numberindex = lit_value_asnumber(index);

        if(!emitter->privates.values[numberindex].initialized)
        {
            lit_astemit_raiseerror(emitter, line, "variable '%.*s' cannot use itself in its initializer", length, name);
        }

        return numberindex;
    }

    return -1;
}

static int lit_astemit_addlocal(LitAstEmitter* emitter, const char* name, size_t length, size_t line, bool constant)
{
    LitAstCompiler* compiler = emitter->compiler;
    LitAstLocList* locals = &compiler->locals;

    if(locals->count == UINT16_MAX)
    {
        lit_astemit_raiseerror(emitter, line, "too many local variables for one function");
    }

    for(int i = (int)locals->count - 1; i >= 0; i--)
    {
        LitAstLocal* local = &locals->values[i];

        if(local->depth != UINT16_MAX && local->depth < compiler->scope_depth)
        {
            break;
        }

        if(length == local->length && memcmp(local->name, name, length) == 0)
        {
            lit_astemit_raiseerror(emitter, line, "variable '%.*s' was already declared in this scope", length, name);
        }
    }

    lit_loclist_push(emitter->state, locals, (LitAstLocal){ name, length, UINT16_MAX, false, constant });

    return (int)locals->count - 1;
}

static int lit_astemit_resolvelocal(LitAstEmitter* emitter, LitAstCompiler* compiler, const char* name, size_t length, size_t line)
{
    int i;
    LitAstLocal* local;
    LitAstLocList* locals;
    locals = &compiler->locals;
    for(i = (int)locals->count - 1; i >= 0; i--)
    {
        local = &locals->values[i];

        if(local->length == length && memcmp(local->name, name, length) == 0)
        {
            if(local->depth == UINT16_MAX)
            {
                lit_astemit_raiseerror(emitter, line, "variable '%.*s' cannot use itself in its initializer", length, name);
            }

            return i;
        }
    }

    return -1;
}

static int lit_astemit_addupvalue(LitAstEmitter* emitter, LitAstCompiler* compiler, uint8_t index, size_t line, bool islocal)
{
    size_t i;
    size_t upvalcnt;
    LitAstCompUpvalue* upvalue;
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
        lit_astemit_raiseerror(emitter, line, "too many upvalues for one function");
        return 0;
    }
    compiler->upvalues[upvalcnt].isLocal = islocal;
    compiler->upvalues[upvalcnt].index = index;
    return compiler->function->upvalue_count++;
}

static int lit_astemit_resolveupvalue(LitAstEmitter* emitter, LitAstCompiler* compiler, const char* name, size_t length, size_t line)
{
    int local;
    int upvalue;
    if(compiler->enclosing == NULL)
    {
        return -1;
    }
    local = lit_astemit_resolvelocal(emitter, (LitAstCompiler*)compiler->enclosing, name, length, line);
    if(local != -1)
    {
        ((LitAstCompiler*)compiler->enclosing)->locals.values[local].captured = true;
        return lit_astemit_addupvalue(emitter, compiler, (uint8_t)local, line, true);
    }
    upvalue = lit_astemit_resolveupvalue(emitter, (LitAstCompiler*)compiler->enclosing, name, length, line);
    if(upvalue != -1)
    {
        return lit_astemit_addupvalue(emitter, compiler, (uint8_t)upvalue, line, false);
    }
    return -1;
}

static void lit_astemit_marklocalinit(LitAstEmitter* emitter, size_t index)
{
    emitter->compiler->locals.values[index].depth = emitter->compiler->scope_depth;
}

static void lit_astemit_markprivateinit(LitAstEmitter* emitter, size_t index)
{
    emitter->privates.values[index].initialized = true;
}

static size_t lit_astemit_emitjump(LitAstEmitter* emitter, LitOpCode code, size_t line)
{
    lit_astemit_emit1op(emitter, line, code);
    lit_astemit_emit2bytes(emitter, line, 0xff, 0xff);
    return emitter->chunk->count - 2;
}

static void lit_astemit_patchjump(LitAstEmitter* emitter, size_t offset, size_t line)
{
    size_t jump;
    jump = emitter->chunk->count - offset - 2;
    if(jump > UINT16_MAX)
    {
        lit_astemit_raiseerror(emitter, line, "too much code to jump over");
    }
    emitter->chunk->code[offset] = (jump >> 8) & 0xff;
    emitter->chunk->code[offset + 1] = jump & 0xff;
}

static void lit_astemit_emitloop(LitAstEmitter* emitter, size_t start, size_t line)
{
    size_t offset;
    lit_astemit_emit1op(emitter, line, OP_JUMP_BACK);
    offset = emitter->chunk->count - start + 2;
    if(offset > UINT16_MAX)
    {
        lit_astemit_raiseerror(emitter, line, "too much code to jump over");
    }
    lit_astemit_emitshort(emitter, line, offset);
}

static void lit_astemit_patchloopjumps(LitAstEmitter* emitter, LitUintList* breaks, size_t line)
{
    size_t i;
    for(i = 0; i < lit_uintlist_count(breaks); i++)
    {
        lit_astemit_patchjump(emitter, lit_uintlist_get(breaks, i), line);
    }
    lit_uintlist_destroy(emitter->state, breaks);
}

static bool lit_astemit_emitparamlist(LitAstEmitter* emitter, LitAstParamList* parameters, size_t line)
{
    size_t i;
    size_t jump;
    int index;
    LitAstParameter* parameter;
    for(i = 0; i < parameters->count; i++)
    {
        parameter = &parameters->values[i];
        index = lit_astemit_addlocal(emitter, parameter->name, parameter->length, line, false);
        lit_astemit_marklocalinit(emitter, index);
        // Vararg ...
        if(parameter->length == 3 && memcmp(parameter->name, "...", 3) == 0)
        {
            return true;
        }
        if(parameter->default_value != NULL)
        {
            lit_astemit_emitbyteorshort(emitter, line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, index);
            jump = lit_astemit_emitjump(emitter, OP_NULL_OR, line);
            lit_astemit_emitexpression(emitter, parameter->default_value);
            lit_astemit_patchjump(emitter, jump, line);
            lit_astemit_emitbyteorshort(emitter, line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, index);
            lit_astemit_emit1op(emitter, line, OP_POP);
        }
    }
    return false;
}

static void lit_astemit_resolvestmt(LitAstEmitter* emitter, LitAstExpression* statement)
{
    LitAstFunctionExpr* funcstmt;
    LitAstAssignVarExpr* varstmt;
    switch(statement->type)
    {
        case LITEXPR_VARSTMT:
            {
                varstmt = (LitAstAssignVarExpr*)statement;
                lit_astemit_markprivateinit(emitter, lit_astemit_addprivate(emitter, varstmt->name, varstmt->length, statement->line, varstmt->constant));
            }
            break;
        case LITEXPR_FUNCTION:
            {
                funcstmt = (LitAstFunctionExpr*)statement;
                if(!funcstmt->exported)
                {
                    lit_astemit_markprivateinit(emitter, lit_astemit_addprivate(emitter, funcstmt->name, funcstmt->length, statement->line, false));
                }
            }
            break;
        case LITEXPR_CLASS:
        case LITEXPR_BLOCK:
        case LITEXPR_FOR:
        case LITEXPR_WHILE:
        case LITEXPR_IFSTMT:
        case LITEXPR_CONTINUE:
        case LITEXPR_BREAK:
        case LITEXPR_RETURN:
        case LITEXPR_METHOD:
        case LITEXPR_FIELD:
        case LITEXPR_EXPRESSION:
            {
            }
            break;
        default:
            {
                
            }
            break;
    }
}

static bool lit_astemit_doemitliteral(LitAstEmitter* emitter, LitAstExpression* expr)
{
    LitValue value;
    value = ((LitAstLiteralExpr*)expr)->value;
    if(lit_value_isnumber(value) || lit_value_isstring(value))
    {
        lit_astemit_emitconstant(emitter, expr->line, value);
    }
    else if(lit_value_isbool(value))
    {
        lit_astemit_emit1op(emitter, expr->line, lit_value_asbool(value) ? OP_TRUE : OP_FALSE);
    }
    else if(lit_value_isnull(value))
    {
        lit_astemit_emit1op(emitter, expr->line, OP_NULL);
    }
    else
    {
        UNREACHABLE;
    }
    return true;
}

static bool lit_astemit_doemitbinary(LitAstEmitter* emitter, LitAstExpression* expr)
{
    size_t jump;
    LitAstTokType op;
    LitAstBinaryExpr* binexpr;
    binexpr = (LitAstBinaryExpr*)expr;
    lit_astemit_emitexpression(emitter, binexpr->left);
    if(binexpr->right == NULL)
    {
        return true;
    }
    op = binexpr->op;
    if(op == LITTOK_AMPERSAND_AMPERSAND || op == LITTOK_BAR_BAR || op == LITTOK_QUESTION_QUESTION)
    {
        //jump = lit_astemit_emitjump(emitter, op == LITTOK_BAR_BAR ? OP_OR : (op == LITTOK_QUESTION_QUESTION ? OP_NULL_OR : OP_AND), emitter->last_line);
        jump = 0;
        if(op == LITTOK_BAR_BAR)
        {
            jump = lit_astemit_emitjump(emitter, OP_OR, emitter->last_line);
        }
        else if(op == LITTOK_QUESTION_QUESTION)
        {
            jump = lit_astemit_emitjump(emitter, OP_NULL_OR, emitter->last_line);
        }
        else
        {
            jump = lit_astemit_emitjump(emitter, OP_AND, emitter->last_line);
        }
        lit_astemit_emitexpression(emitter, binexpr->right);
        lit_astemit_patchjump(emitter, jump, emitter->last_line);
        return true;
    }
    lit_astemit_emitexpression(emitter, binexpr->right);
    switch(op)
    {
        case LITTOK_PLUS:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_ADD);
            }
            break;
        case LITTOK_MINUS:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_SUBTRACT);
            }
            break;
        case LITTOK_STAR:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_MULTIPLY);
            }
            break;
        case LITTOK_STAR_STAR:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_POWER);
            }
            break;
        case LITTOK_SLASH:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_DIVIDE);
            }
            break;
        case LITTOK_SHARP:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_FLOOR_DIVIDE);
            }
            break;
        case LITTOK_PERCENT:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_MOD);
            }
            break;
        case LITTOK_IS:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_IS);
            }
            break;
        case LITTOK_EQUAL_EQUAL:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_EQUAL);
            }
            break;
        case LITTOK_BANG_EQUAL:
            {
                lit_astemit_emit2ops(emitter, expr->line, OP_EQUAL, OP_NOT);
            }
            break;
        case LITTOK_GREATER:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_GREATER);
            }
            break;
        case LITTOK_GREATER_EQUAL:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_GREATER_EQUAL);
            }
            break;
        case LITTOK_LESS:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_LESS);
            }
            break;
        case LITTOK_LESS_EQUAL:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_LESS_EQUAL);
            }
            break;
        case LITTOK_LESS_LESS:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_LSHIFT);
            }
            break;
        case LITTOK_GREATER_GREATER:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_RSHIFT);
            }
            break;
        case LITTOK_BAR:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_BOR);
            }
            break;
        case LITTOK_AMPERSAND:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_BAND);
            }
            break;
        case LITTOK_CARET:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_BXOR);
            }
            break;
        default:
            {
                fprintf(stderr, "in lit_astemit_emitexpression: binary expression #2 is NULL! might be a bug\n");
                //return;
                //UNREACHABLE;
            }
        break;
    }
    return true;
}

static bool lit_astemit_doemitunary(LitAstEmitter* emitter, LitAstExpression* expr)
{
    LitAstUnaryExpr* unexpr;
    unexpr = (LitAstUnaryExpr*)expr;
    lit_astemit_emitexpression(emitter, unexpr->right);
    switch(unexpr->op)
    {
        case LITTOK_MINUS:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_NEGATE);
            }
            break;
        case LITTOK_BANG:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_NOT);
            }
            break;
        case LITTOK_TILDE:
            {
                lit_astemit_emit1op(emitter, expr->line, OP_BNOT);
            }
            break;
        default:
            {
                fprintf(stderr, "in lit_astemit_emitexpression: unary expr is NULL! might be an internal bug\n");
                //return;
                //UNREACHABLE;
            }
            break;
    }
    return true;
}

static bool lit_astemit_doemitvarexpr(LitAstEmitter* emitter, LitAstExpression* expr)
{
    int index;
    bool ref;
    LitAstVarExpr* varexpr;
    varexpr = (LitAstVarExpr*)expr;
    ref = emitter->emit_reference > 0;
    if(ref)
    {
        emitter->emit_reference--;
    }
    index = lit_astemit_resolvelocal(emitter, emitter->compiler, varexpr->name, varexpr->length, expr->line);
    if(index == -1)
    {
        index = lit_astemit_resolveupvalue(emitter, emitter->compiler, varexpr->name, varexpr->length, expr->line);
        if(index == -1)
        {
            index = lit_astemit_resolveprivate(emitter, varexpr->name, varexpr->length, expr->line);
            if(index == -1)
            {
                lit_astemit_emit1op(emitter, expr->line, ref ? OP_REFERENCE_GLOBAL : OP_GET_GLOBAL);
                lit_astemit_emitshort(emitter, expr->line,
                           lit_astemit_addconstant(emitter, expr->line,
                                        lit_value_fromobject(lit_string_copy(emitter->state, varexpr->name, varexpr->length))));
            }
            else
            {
                if(ref)
                {
                    lit_astemit_emit1op(emitter, expr->line, OP_REFERENCE_PRIVATE);
                    lit_astemit_emitshort(emitter, expr->line, index);
                }
                else
                {
                    lit_astemit_emitbyteorshort(emitter, expr->line, OP_GET_PRIVATE, OP_GET_PRIVATE_LONG, index);
                }
            }
        }
        else
        {
            lit_astemit_emitargedop(emitter, expr->line, ref ? OP_REFERENCE_UPVALUE : OP_GET_UPVALUE, (uint8_t)index);
        }
    }
    else
    {
        if(ref)
        {
            lit_astemit_emit1op(emitter, expr->line, OP_REFERENCE_LOCAL);
            lit_astemit_emitshort(emitter, expr->line, index);
        }
        else
        {
            lit_astemit_emitbyteorshort(emitter, expr->line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, index);
        }
    }
    return true;
}

static bool lit_astemit_doemitassign(LitAstEmitter* emitter, LitAstExpression* expr)
{
    int index;
    LitAstVarExpr* e;
    LitAstGetExpr* getexpr;
    LitAstIndexExpr* indexexpr;
    LitAstAssignExpr* assignexpr;
    assignexpr = (LitAstAssignExpr*)expr;
    if(assignexpr->to->type == LITEXPR_VAREXPR)
    {
        lit_astemit_emitexpression(emitter, assignexpr->value);
        e = (LitAstVarExpr*)assignexpr->to;
        index = lit_astemit_resolvelocal(emitter, emitter->compiler, e->name, e->length, assignexpr->to->line);
        if(index == -1)
        {
            index = lit_astemit_resolveupvalue(emitter, emitter->compiler, e->name, e->length, assignexpr->to->line);
            if(index == -1)
            {
                index = lit_astemit_resolveprivate(emitter, e->name, e->length, assignexpr->to->line);
                if(index == -1)
                {
                    lit_astemit_emit1op(emitter, expr->line, OP_SET_GLOBAL);
                    lit_astemit_emitshort(emitter, expr->line,
                               lit_astemit_addconstant(emitter, expr->line,
                                            lit_value_fromobject(lit_string_copy(emitter->state, e->name, e->length))));
                }
                else
                {
                    if(emitter->privates.values[index].constant)
                    {
                        lit_astemit_raiseerror(emitter, expr->line, "attempt to modify constant '%.*s'", e->length, e->name);
                    }
                    lit_astemit_emitbyteorshort(emitter, expr->line, OP_SET_PRIVATE, OP_SET_PRIVATE_LONG, index);
                }
            }
            else
            {
                lit_astemit_emitargedop(emitter, expr->line, OP_SET_UPVALUE, (uint8_t)index);
            }
            return true;
        }
        else
        {
            if(emitter->compiler->locals.values[index].constant)
            {
                lit_astemit_raiseerror(emitter, expr->line, "attempt to modify constant '%.*s'", e->length, e->name);
            }

            lit_astemit_emitbyteorshort(emitter, expr->line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, index);
        }
    }
    else if(assignexpr->to->type == LITEXPR_GET)
    {
        lit_astemit_emitexpression(emitter, assignexpr->value);
        getexpr = (LitAstGetExpr*)assignexpr->to;
        lit_astemit_emitexpression(emitter, getexpr->where);
        lit_astemit_emitexpression(emitter, assignexpr->value);
        lit_astemit_emitconstant(emitter, emitter->last_line, lit_value_fromobject(lit_string_copy(emitter->state, getexpr->name, getexpr->length)));
        lit_astemit_emit2ops(emitter, emitter->last_line, OP_SET_FIELD, OP_POP);
    }
    else if(assignexpr->to->type == LITEXPR_SUBSCRIPT)
    {
        indexexpr = (LitAstIndexExpr*)assignexpr->to;
        lit_astemit_emitexpression(emitter, indexexpr->array);
        lit_astemit_emitexpression(emitter, indexexpr->index);
        lit_astemit_emitexpression(emitter, assignexpr->value);
        lit_astemit_emit1op(emitter, emitter->last_line, OP_SUBSCRIPT_SET);
    }
    else if(assignexpr->to->type == LITEXPR_REFERENCE)
    {
        lit_astemit_emitexpression(emitter, assignexpr->value);
        lit_astemit_emitexpression(emitter, ((LitAstRefExpr*)assignexpr->to)->to);
        lit_astemit_emit1op(emitter, expr->line, OP_SET_REFERENCE);
    }
    else
    {
        lit_astemit_raiseerror(emitter, expr->line, "invalid assigment target");
    }
    return true;
}

static bool lit_astemit_doemitcall(LitAstEmitter* emitter, LitAstExpression* expr)
{
    size_t i;
    uint8_t index;
    bool issuper;
    bool ismethod;
    LitAstExpression* e;
    LitAstVarExpr* ee;
    LitAstObjectExpr* init;
    LitAstCallExpr* callexpr;
    LitAstSuperExpr* superexpr;
    LitAstExpression* get;
    LitAstGetExpr* getexpr;
    LitAstGetExpr* getter;
    callexpr = (LitAstCallExpr*)expr;
    ismethod = callexpr->callee->type == LITEXPR_GET;
    issuper = callexpr->callee->type == LITEXPR_SUPER;
    if(ismethod)
    {
        ((LitAstGetExpr*)callexpr->callee)->ignore_emit = true;
    }
    else if(issuper)
    {
        ((LitAstSuperExpr*)callexpr->callee)->ignore_emit = true;
    }
    lit_astemit_emitexpression(emitter, callexpr->callee);
    if(issuper)
    {
        lit_astemit_emitargedop(emitter, expr->line, OP_GET_LOCAL, 0);
    }
    for(i = 0; i < callexpr->args.count; i++)
    {
        e = callexpr->args.values[i];
        if(e->type == LITEXPR_VAREXPR)
        {
            ee = (LitAstVarExpr*)e;
            // Vararg ...
            if(ee->length == 3 && memcmp(ee->name, "...", 3) == 0)
            {
                lit_astemit_emitargedop(emitter, e->line, OP_VARARG,
                              lit_astemit_resolvelocal(emitter, emitter->compiler, "...", 3, expr->line));
                break;
            }
        }
        lit_astemit_emitexpression(emitter, e);
    }
    if(ismethod || issuper)
    {
        if(ismethod)
        {
            getexpr = (LitAstGetExpr*)callexpr->callee;
            lit_astemit_emitvaryingop(emitter, expr->line,
                            ((LitAstGetExpr*)callexpr->callee)->ignore_result ? OP_INVOKE_IGNORING : OP_INVOKE,
                            (uint8_t)callexpr->args.count);
            lit_astemit_emitshort(emitter, emitter->last_line,
                       lit_astemit_addconstant(emitter, emitter->last_line,
                                    lit_value_fromobject(lit_string_copy(emitter->state, getexpr->name, getexpr->length))));
        }
        else
        {
            superexpr = (LitAstSuperExpr*)callexpr->callee;
            index = lit_astemit_resolveupvalue(emitter, emitter->compiler, "super", 5, emitter->last_line);
            lit_astemit_emitargedop(emitter, expr->line, OP_GET_UPVALUE, index);
            lit_astemit_emitvaryingop(emitter, emitter->last_line,
                            ((LitAstSuperExpr*)callexpr->callee)->ignore_result ? OP_INVOKE_SUPER_IGNORING : OP_INVOKE_SUPER,
                            (uint8_t)callexpr->args.count);
            lit_astemit_emitshort(emitter, emitter->last_line, lit_astemit_addconstant(emitter, emitter->last_line, lit_value_fromobject(superexpr->method)));
        }
    }
    else
    {
        lit_astemit_emitvaryingop(emitter, expr->line, OP_CALL, (uint8_t)callexpr->args.count);
    }
    if(ismethod)
    {
        get = callexpr->callee;
        while(get != NULL)
        {
            if(get->type == LITEXPR_GET)
            {
                getter = (LitAstGetExpr*)get;
                if(getter->jump > 0)
                {
                    lit_astemit_patchjump(emitter, getter->jump, emitter->last_line);
                }
                get = getter->where;
            }
            else if(get->type == LITEXPR_SUBSCRIPT)
            {
                get = ((LitAstIndexExpr*)get)->array;
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
    init = (LitAstObjectExpr*)callexpr->init;
    for(i = 0; i < init->values.count; i++)
    {
        e = init->values.values[i];
        emitter->last_line = e->line;
        lit_astemit_emitconstant(emitter, emitter->last_line, lit_vallist_get(&init->keys, i));
        lit_astemit_emitexpression(emitter, e);
        lit_astemit_emit1op(emitter, emitter->last_line, OP_PUSH_OBJECT_FIELD);
    }
    return true;
}

static bool lit_astemit_doemitget(LitAstEmitter* emitter, LitAstExpression* expr)
{
    bool ref;
    LitAstGetExpr* getexpr;
    getexpr = (LitAstGetExpr*)expr;
    ref = emitter->emit_reference > 0;
    if(ref)
    {
        emitter->emit_reference--;
    }
    lit_astemit_emitexpression(emitter, getexpr->where);
    if(getexpr->jump == 0)
    {
        getexpr->jump = lit_astemit_emitjump(emitter, OP_JUMP_IF_NULL, emitter->last_line);
        if(!getexpr->ignore_emit)
        {
            lit_astemit_emitconstant(emitter, emitter->last_line,
                          lit_value_fromobject(lit_string_copy(emitter->state, getexpr->name, getexpr->length)));
            lit_astemit_emit1op(emitter, emitter->last_line, ref ? OP_REFERENCE_FIELD : OP_GET_FIELD);
        }
        lit_astemit_patchjump(emitter, getexpr->jump, emitter->last_line);
    }
    else if(!getexpr->ignore_emit)
    {
        lit_astemit_emitconstant(emitter, emitter->last_line, lit_value_fromobject(lit_string_copy(emitter->state, getexpr->name, getexpr->length)));
        lit_astemit_emit1op(emitter, emitter->last_line, ref ? OP_REFERENCE_FIELD : OP_GET_FIELD);
    }
    return true;
}

static bool lit_astemit_doemitset(LitAstEmitter* emitter, LitAstExpression* expr)
{
    LitAstSetExpr* setexpr;
    setexpr = (LitAstSetExpr*)expr;
    lit_astemit_emitexpression(emitter, setexpr->where);
    lit_astemit_emitexpression(emitter, setexpr->value);
    lit_astemit_emitconstant(emitter, emitter->last_line, lit_value_fromobject(lit_string_copy(emitter->state, setexpr->name, setexpr->length)));
    lit_astemit_emit1op(emitter, emitter->last_line, OP_SET_FIELD);
    return true;
}

static bool lit_astemit_doemitlambda(LitAstEmitter* emitter, LitAstExpression* expr)
{
    size_t i;
    bool vararg;
    bool singleexpr;
    LitAstCompiler compiler;
    LitString* name;
    LitFunction* function;
    LitAstFunctionExpr* lambdaexpr;
    lambdaexpr = (LitAstFunctionExpr*)expr;
    name = lit_value_asstring(lit_string_format(emitter->state,
        "lambda @:@", lit_value_fromobject(emitter->module->name), lit_string_numbertostring(emitter->state, expr->line)));
    lit_compiler_compiler(emitter, &compiler, LITFUNC_REGULAR);
    lit_astemit_beginscope(emitter);
    vararg = lit_astemit_emitparamlist(emitter, &lambdaexpr->parameters, expr->line);
    if(lambdaexpr->body != NULL)
    {
        singleexpr = lambdaexpr->body->type == LITEXPR_EXPRESSION;
        if(singleexpr)
        {
            compiler.skip_return = true;
            ((LitAstExprExpr*)lambdaexpr->body)->pop = false;
        }
        lit_astemit_emitexpression(emitter, lambdaexpr->body);
        if(singleexpr)
        {
            lit_astemit_emit1op(emitter, emitter->last_line, OP_RETURN);
        }
    }
    lit_astemit_endscope(emitter, emitter->last_line);
    function = lit_compiler_end(emitter, name);
    function->arg_count = lambdaexpr->parameters.count;
    function->max_slots += function->arg_count;
    function->vararg = vararg;
    if(function->upvalue_count > 0)
    {
        lit_astemit_emit1op(emitter, emitter->last_line, OP_CLOSURE);
        lit_astemit_emitshort(emitter, emitter->last_line, lit_astemit_addconstant(emitter, emitter->last_line, lit_value_fromobject(function)));
        for(i = 0; i < function->upvalue_count; i++)
        {
            lit_astemit_emit2bytes(emitter, emitter->last_line, compiler.upvalues[i].isLocal ? 1 : 0, compiler.upvalues[i].index);
        }
    }
    else
    {
        lit_astemit_emitconstant(emitter, emitter->last_line, lit_value_fromobject(function));
    }
    return true;
}

static bool lit_astemit_doemitarray(LitAstEmitter* emitter, LitAstExpression* expr)
{
    size_t i;
    LitAstArrayExpr* arrexpr;
    arrexpr = (LitAstArrayExpr*)expr;
    lit_astemit_emit1op(emitter, expr->line, OP_ARRAY);
    for(i = 0; i < arrexpr->values.count; i++)
    {
        lit_astemit_emitexpression(emitter, arrexpr->values.values[i]);
        lit_astemit_emit1op(emitter, emitter->last_line, OP_PUSH_ARRAY_ELEMENT);
    }
    return true;
}

static bool lit_astemit_doemitobject(LitAstEmitter* emitter, LitAstExpression* expr)
{
    size_t i;
    LitAstObjectExpr* objexpr;
    objexpr = (LitAstObjectExpr*)expr;
    lit_astemit_emit1op(emitter, expr->line, OP_OBJECT);
    for(i = 0; i < objexpr->values.count; i++)
    {
        lit_astemit_emitconstant(emitter, emitter->last_line, lit_vallist_get(&objexpr->keys, i));
        lit_astemit_emitexpression(emitter, objexpr->values.values[i]);
        lit_astemit_emit1op(emitter, emitter->last_line, OP_PUSH_OBJECT_FIELD);
    }
    return true;
}

static bool lit_astemit_doemitthis(LitAstEmitter* emitter, LitAstExpression* expr)
{
    int local;
    LitAstFuncType type;
    type = emitter->compiler->type;
    if(type == LITFUNC_STATIC_METHOD)
    {
        lit_astemit_raiseerror(emitter, expr->line, "'this' cannot be used %s", "in static methods");
    }
    if(type == LITFUNC_CONSTRUCTOR || type == LITFUNC_METHOD)
    {
        lit_astemit_emitargedop(emitter, expr->line, OP_GET_LOCAL, 0);
    }
    else
    {
        if(emitter->compiler->enclosing == NULL)
        {
            lit_astemit_raiseerror(emitter, expr->line, "'this' cannot be used %s", "in functions outside of any class");
        }
        else
        {
            local = lit_astemit_resolvelocal(emitter, (LitAstCompiler*)emitter->compiler->enclosing, "this", 4, expr->line);
            lit_astemit_emitargedop(emitter, expr->line, OP_GET_UPVALUE,
                          lit_astemit_addupvalue(emitter, emitter->compiler, local, expr->line, true));
        }
    }
    return true;
}

static bool lit_astemit_doemitsuper(LitAstEmitter* emitter, LitAstExpression* expr)
{
    uint8_t index;
    LitAstSuperExpr* superexpr;
    if(emitter->compiler->type == LITFUNC_STATIC_METHOD)
    {
        lit_astemit_raiseerror(emitter, expr->line, "'super' cannot be used %s", "in static methods");
    }
    else if(!emitter->class_has_super)
    {
        lit_astemit_raiseerror(emitter, expr->line, "'super' cannot be used in class '%s', because it does not have a super class", emitter->class_name->chars);
    }
    superexpr = (LitAstSuperExpr*)expr;
    if(!superexpr->ignore_emit)
    {
        index = lit_astemit_resolveupvalue(emitter, emitter->compiler, "super", 5, emitter->last_line);
        lit_astemit_emitargedop(emitter, expr->line, OP_GET_LOCAL, 0);
        lit_astemit_emitargedop(emitter, expr->line, OP_GET_UPVALUE, index);
        lit_astemit_emit1op(emitter, expr->line, OP_GET_SUPER_METHOD);
        lit_astemit_emitshort(emitter, expr->line, lit_astemit_addconstant(emitter, expr->line, lit_value_fromobject(superexpr->method)));
    }
    return true;
}

static bool lit_astemit_doemitternary(LitAstEmitter* emitter, LitAstExpression* expr)
{
    uint64_t endjump;
    uint64_t elsejump;
    LitAstTernaryExpr* ifexpr;
    ifexpr = (LitAstTernaryExpr*)expr;
    lit_astemit_emitexpression(emitter, ifexpr->condition);
    elsejump = lit_astemit_emitjump(emitter, OP_JUMP_IF_FALSE, expr->line);
    lit_astemit_emitexpression(emitter, ifexpr->if_branch);
    endjump = lit_astemit_emitjump(emitter, OP_JUMP, emitter->last_line);
    lit_astemit_patchjump(emitter, elsejump, ifexpr->else_branch->line);
    lit_astemit_emitexpression(emitter, ifexpr->else_branch);
    lit_astemit_patchjump(emitter, endjump, emitter->last_line);
    return true;
}

static bool lit_astemit_doemitinterpolation(LitAstEmitter* emitter, LitAstExpression* expr)
{
    size_t i;
    LitAstStrInterExpr* ifexpr;
    ifexpr = (LitAstStrInterExpr*)expr;
    lit_astemit_emit1op(emitter, expr->line, OP_ARRAY);
    for(i = 0; i < ifexpr->expressions.count; i++)
    {
        lit_astemit_emitexpression(emitter, ifexpr->expressions.values[i]);
        lit_astemit_emit1op(emitter, emitter->last_line, OP_PUSH_ARRAY_ELEMENT);
    }
    lit_astemit_emitvaryingop(emitter, emitter->last_line, OP_INVOKE, 0);
    lit_astemit_emitshort(emitter, emitter->last_line,
               lit_astemit_addconstant(emitter, emitter->last_line, lit_value_makestring(emitter->state, "join")));
    return true;
}

static bool lit_astemit_doemitreference(LitAstEmitter* emitter, LitAstExpression* expr)
{
    int old;
    LitAstExpression* to;
    to = ((LitAstRefExpr*)expr)->to;
    if(to->type != LITEXPR_VAREXPR && to->type != LITEXPR_GET && to->type != LITEXPR_THIS && to->type != LITEXPR_SUPER)
    {
        lit_astemit_raiseerror(emitter, expr->line, "invalid refence target");
        return false;
    }
    old = emitter->emit_reference;
    emitter->emit_reference++;
    lit_astemit_emitexpression(emitter, to);
    emitter->emit_reference = old;
    return true;
}

static bool lit_astemit_doemitvarstmt(LitAstEmitter* emitter, LitAstExpression* expr)
{
    bool isprivate;
    int index;
    size_t line;
    LitAstAssignVarExpr* varstmt;
    varstmt = (LitAstAssignVarExpr*)expr;
    line = expr->line;
    isprivate = emitter->compiler->enclosing == NULL && emitter->compiler->scope_depth == 0;
    index = isprivate ? lit_astemit_resolveprivate(emitter, varstmt->name, varstmt->length, expr->line) :
                          lit_astemit_addlocal(emitter, varstmt->name, varstmt->length, expr->line, varstmt->constant);
    if(varstmt->init == NULL)
    {
        lit_astemit_emit1op(emitter, line, OP_NULL);
    }
    else
    {
        lit_astemit_emitexpression(emitter, varstmt->init);
    }
    if(isprivate)
    {
        lit_astemit_markprivateinit(emitter, index);
    }
    else
    {
        lit_astemit_marklocalinit(emitter, index);
    }
    lit_astemit_emitbyteorshort(emitter, expr->line, isprivate ? OP_SET_PRIVATE : OP_SET_LOCAL,
                       isprivate ? OP_SET_PRIVATE_LONG : OP_SET_LOCAL_LONG, index);
    if(isprivate)
    {
        // Privates don't live on stack, so we need to pop them manually
        lit_astemit_emit1op(emitter, expr->line, OP_POP);
    }
    return true;
}

static bool lit_astemit_doemitifstmt(LitAstEmitter* emitter, LitAstExpression* expr)
{
    size_t i;
    size_t elsejump;
    size_t endjump;
    uint64_t* endjumps;
    LitAstExpression* e;
    LitAstIfExpr* ifstmt;
    ifstmt = (LitAstIfExpr*)expr;
    elsejump = 0;
    endjump = 0;
    if(ifstmt->condition == NULL)
    {
        elsejump = lit_astemit_emitjump(emitter, OP_JUMP, expr->line);
    }
    else
    {
        lit_astemit_emitexpression(emitter, ifstmt->condition);
        elsejump = lit_astemit_emitjump(emitter, OP_JUMP_IF_FALSE, expr->line);
        lit_astemit_emitexpression(emitter, ifstmt->if_branch);
        endjump = lit_astemit_emitjump(emitter, OP_JUMP, emitter->last_line);
    }
    /* important: endjumps must be N*sizeof(uint64_t) - merely allocating N isn't enough! */
    //uint64_t endjumps[ifstmt->elseif_branches == NULL ? 1 : ifstmt->elseif_branches->count];
    endjumps = (uint64_t*)malloc(sizeof(uint64_t) * (ifstmt->elseif_branches == NULL ? 1 : ifstmt->elseif_branches->count));
    if(ifstmt->elseif_branches != NULL)
    {
        for(i = 0; i < ifstmt->elseif_branches->count; i++)
        {
            e = ifstmt->elseif_conditions->values[i];
            if(e == NULL)
            {
                continue;
            }
            lit_astemit_patchjump(emitter, elsejump, e->line);
            lit_astemit_emitexpression(emitter, e);
            elsejump = lit_astemit_emitjump(emitter, OP_JUMP_IF_FALSE, emitter->last_line);
            lit_astemit_emitexpression(emitter, ifstmt->elseif_branches->values[i]);
            endjumps[i] = lit_astemit_emitjump(emitter, OP_JUMP, emitter->last_line);
        }
    }
    if(ifstmt->else_branch != NULL)
    {
        lit_astemit_patchjump(emitter, elsejump, ifstmt->else_branch->line);
        lit_astemit_emitexpression(emitter, ifstmt->else_branch);
    }
    else
    {
        lit_astemit_patchjump(emitter, elsejump, emitter->last_line);
    }
    if(endjump != 0)
    {
        lit_astemit_patchjump(emitter, endjump, emitter->last_line);
    }
    if(ifstmt->elseif_branches != NULL)
    {
        for(i = 0; i < ifstmt->elseif_branches->count; i++)
        {
            if(ifstmt->elseif_branches->values[i] == NULL)
            {
                continue;
            }
            lit_astemit_patchjump(emitter, endjumps[i], ifstmt->elseif_branches->values[i]->line);
        }
    }
    free(endjumps);
    return true;
}

static bool lit_astemit_doemitblockstmt(LitAstEmitter* emitter, LitAstExpression* expr)
{
    size_t i;
    LitAstExpression* blockstmt;
    LitAstExprList* statements;
    statements = &((LitAstBlockExpr*)expr)->statements;
    lit_astemit_beginscope(emitter);
    {
        for(i = 0; i < statements->count; i++)
        {
            blockstmt = statements->values[i];
            if(lit_astemit_emitexpression(emitter, blockstmt))
            {
                break;
            }
        }
    }
    lit_astemit_endscope(emitter, emitter->last_line);
    return true;
}

static bool lit_astemit_doemitwhilestmt(LitAstEmitter* emitter, LitAstExpression* expr)
{
    size_t start;
    size_t exitjump;
    LitAstWhileExpr* whilestmt;
    whilestmt = (LitAstWhileExpr*)expr;
    start = emitter->chunk->count;
    emitter->loop_start = start;
    emitter->compiler->loop_depth++;
    lit_astemit_emitexpression(emitter, whilestmt->condition);
    exitjump = lit_astemit_emitjump(emitter, OP_JUMP_IF_FALSE, expr->line);
    lit_astemit_emitexpression(emitter, whilestmt->body);
    lit_astemit_patchloopjumps(emitter, &emitter->continues, emitter->last_line);
    lit_astemit_emitloop(emitter, start, emitter->last_line);
    lit_astemit_patchjump(emitter, exitjump, emitter->last_line);
    lit_astemit_patchloopjumps(emitter, &emitter->breaks, emitter->last_line);
    emitter->compiler->loop_depth--;
    return true;
}

static bool lit_astemit_doemitforstmt(LitAstEmitter* emitter, LitAstExpression* expr)
{
    size_t i;
    size_t start;
    size_t exitjump;
    size_t bodyjump;
    size_t incrstart;
    size_t sequence;
    size_t iterator;
    size_t localcnt;
    LitAstExprList* statements;
    LitAstAssignVarExpr* var;
    LitAstForExpr* forstmt;
    forstmt = (LitAstForExpr*)expr;
    lit_astemit_beginscope(emitter);
    emitter->compiler->loop_depth++;
    if(forstmt->c_style)
    {
        if(forstmt->var != NULL)
        {
            lit_astemit_emitexpression(emitter, forstmt->var);
        }
        else if(forstmt->init != NULL)
        {
            lit_astemit_emitexpression(emitter, forstmt->init);
        }
        start = emitter->chunk->count;
        exitjump = 0;
        if(forstmt->condition != NULL)
        {
            lit_astemit_emitexpression(emitter, forstmt->condition);
            exitjump = lit_astemit_emitjump(emitter, OP_JUMP_IF_FALSE, emitter->last_line);
        }
        if(forstmt->increment != NULL)
        {
            bodyjump = lit_astemit_emitjump(emitter, OP_JUMP, emitter->last_line);
            incrstart = emitter->chunk->count;
            lit_astemit_emitexpression(emitter, forstmt->increment);
            lit_astemit_emit1op(emitter, emitter->last_line, OP_POP);
            lit_astemit_emitloop(emitter, start, emitter->last_line);
            start = incrstart;
            lit_astemit_patchjump(emitter, bodyjump, emitter->last_line);
        }
        emitter->loop_start = start;
        lit_astemit_beginscope(emitter);
        if(forstmt->body != NULL)
        {
            if(forstmt->body->type == LITEXPR_BLOCK)
            {
                statements = &((LitAstBlockExpr*)forstmt->body)->statements;
                for(i = 0; i < statements->count; i++)
                {
                    lit_astemit_emitexpression(emitter, statements->values[i]);
                }
            }
            else
            {
                lit_astemit_emitexpression(emitter, forstmt->body);
            }
        }
        lit_astemit_patchloopjumps(emitter, &emitter->continues, emitter->last_line);
        lit_astemit_endscope(emitter, emitter->last_line);
        lit_astemit_emitloop(emitter, start, emitter->last_line);
        if(forstmt->condition != NULL)
        {
            lit_astemit_patchjump(emitter, exitjump, emitter->last_line);
        }
    }
    else
    {
        sequence = lit_astemit_addlocal(emitter, "seq ", 4, expr->line, false);
        lit_astemit_marklocalinit(emitter, sequence);
        lit_astemit_emitexpression(emitter, forstmt->condition);
        lit_astemit_emitbyteorshort(emitter, emitter->last_line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, sequence);
        iterator = lit_astemit_addlocal(emitter, "iter ", 5, expr->line, false);
        lit_astemit_marklocalinit(emitter, iterator);
        lit_astemit_emit1op(emitter, emitter->last_line, OP_NULL);
        lit_astemit_emitbyteorshort(emitter, emitter->last_line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, iterator);
        start = emitter->chunk->count;
        emitter->loop_start = emitter->chunk->count;
        // iter = seq.iterator(iter)
        lit_astemit_emitbyteorshort(emitter, emitter->last_line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, sequence);
        lit_astemit_emitbyteorshort(emitter, emitter->last_line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, iterator);
        lit_astemit_emitvaryingop(emitter, emitter->last_line, OP_INVOKE, 1);
        lit_astemit_emitshort(emitter, emitter->last_line,
                   lit_astemit_addconstant(emitter, emitter->last_line, lit_value_makestring(emitter->state, "iterator")));
        lit_astemit_emitbyteorshort(emitter, emitter->last_line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, iterator);
        // If iter is null, just get out of the loop
        exitjump = lit_astemit_emitjump(emitter, OP_JUMP_IF_NULL_POPPING, emitter->last_line);
        lit_astemit_beginscope(emitter);
        // var i = seq.iteratorValue(iter)
        var = (LitAstAssignVarExpr*)forstmt->var;
        localcnt = lit_astemit_addlocal(emitter, var->name, var->length, expr->line, false);
        lit_astemit_marklocalinit(emitter, localcnt);
        lit_astemit_emitbyteorshort(emitter, emitter->last_line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, sequence);
        lit_astemit_emitbyteorshort(emitter, emitter->last_line, OP_GET_LOCAL, OP_GET_LOCAL_LONG, iterator);
        lit_astemit_emitvaryingop(emitter, emitter->last_line, OP_INVOKE, 1);
        lit_astemit_emitshort(emitter, emitter->last_line,
                   lit_astemit_addconstant(emitter, emitter->last_line, lit_value_makestring(emitter->state, "iteratorValue")));
        lit_astemit_emitbyteorshort(emitter, emitter->last_line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, localcnt);
        if(forstmt->body != NULL)
        {
            if(forstmt->body->type == LITEXPR_BLOCK)
            {
                statements = &((LitAstBlockExpr*)forstmt->body)->statements;
                for(i = 0; i < statements->count; i++)
                {
                    lit_astemit_emitexpression(emitter, statements->values[i]);
                }
            }
            else
            {
                lit_astemit_emitexpression(emitter, forstmt->body);
            }
        }
        lit_astemit_patchloopjumps(emitter, &emitter->continues, emitter->last_line);
        lit_astemit_endscope(emitter, emitter->last_line);
        lit_astemit_emitloop(emitter, start, emitter->last_line);
        lit_astemit_patchjump(emitter, exitjump, emitter->last_line);
    }
    lit_astemit_patchloopjumps(emitter, &emitter->breaks, emitter->last_line);
    lit_astemit_endscope(emitter, emitter->last_line);
    emitter->compiler->loop_depth--;
    return true;
}

static bool lit_astemit_doemitbreak(LitAstEmitter* emitter, LitAstExpression* expr)
{
    int depth;
    int ii;
    uint16_t local_count;
    LitAstLocal* local;
    LitAstLocList* locals;
    if(emitter->compiler->loop_depth == 0)
    {
        lit_astemit_raiseerror(emitter, expr->line, "cannot use '%s' outside of loops", "break");
    }
    lit_astemit_emit1op(emitter, expr->line, OP_POP_LOCALS);
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
    lit_astemit_emitshort(emitter, expr->line, local_count);
    lit_uintlist_push(emitter->state, &emitter->breaks, lit_astemit_emitjump(emitter, OP_JUMP, expr->line));
    return true;
}

static bool lit_astemit_doemitcontinue(LitAstEmitter* emitter, LitAstExpression* expr)
{
    if(emitter->compiler->loop_depth == 0)
    {
        lit_astemit_raiseerror(emitter, expr->line, "cannot use '%s' outside of loops", "continue");
    }
    lit_uintlist_push(emitter->state, &emitter->continues, lit_astemit_emitjump(emitter, OP_JUMP, expr->line));
    return true;
}

static bool lit_astemit_doemitreturn(LitAstEmitter* emitter, LitAstExpression* expr)
{
    LitAstExpression* expression;
    if(emitter->compiler->type == LITFUNC_CONSTRUCTOR)
    {
        lit_astemit_raiseerror(emitter, expr->line, "cannot use 'return' in constructors");
        return false;
    }
    expression = ((LitAstReturnExpr*)expr)->expression;
    if(expression == NULL)
    {
        lit_astemit_emit1op(emitter, emitter->last_line, OP_NULL);
    }
    else
    {
        lit_astemit_emitexpression(emitter, expression);
    }
    lit_astemit_emit1op(emitter, emitter->last_line, OP_RETURN);
    if(emitter->compiler->scope_depth == 0)
    {
        emitter->compiler->skip_return = true;
    }
    return true;
}

static bool lit_astemit_doemitfunction(LitAstEmitter* emitter, LitAstExpression* expr)
{
    int index;
    size_t i;
    bool isprivate;
    bool islocal;
    bool isexport;
    bool vararg;
    LitString* name;
    LitAstCompiler compiler;
    LitFunction* function;
    LitAstFunctionExpr* funcstmt;
    funcstmt = (LitAstFunctionExpr*)expr;
    isexport = funcstmt->exported;
    isprivate = !isexport && emitter->compiler->enclosing == NULL && emitter->compiler->scope_depth == 0;
    islocal = !(isexport || isprivate);
    index = 0;
    if(!isexport)
    {
        index = isprivate ? lit_astemit_resolveprivate(emitter, funcstmt->name, funcstmt->length, expr->line) :
                          lit_astemit_addlocal(emitter, funcstmt->name, funcstmt->length, expr->line, false);
    }
    name = lit_string_copy(emitter->state, funcstmt->name, funcstmt->length);
    if(islocal)
    {
        lit_astemit_marklocalinit(emitter, index);
    }
    else if(isprivate)
    {
        lit_astemit_markprivateinit(emitter, index);
    }
    lit_compiler_compiler(emitter, &compiler, LITFUNC_REGULAR);
    lit_astemit_beginscope(emitter);
    vararg = lit_astemit_emitparamlist(emitter, &funcstmt->parameters, expr->line);
    lit_astemit_emitexpression(emitter, funcstmt->body);
    lit_astemit_endscope(emitter, emitter->last_line);
    function = lit_compiler_end(emitter, name);
    function->arg_count = funcstmt->parameters.count;
    function->max_slots += function->arg_count;
    function->vararg = vararg;
    if(function->upvalue_count > 0)
    {
        lit_astemit_emit1op(emitter, emitter->last_line, OP_CLOSURE);
        lit_astemit_emitshort(emitter, emitter->last_line, lit_astemit_addconstant(emitter, emitter->last_line, lit_value_fromobject(function)));
        for(i = 0; i < function->upvalue_count; i++)
        {
            lit_astemit_emit2bytes(emitter, emitter->last_line, compiler.upvalues[i].isLocal ? 1 : 0, compiler.upvalues[i].index);
        }
    }
    else
    {
        lit_astemit_emitconstant(emitter, emitter->last_line, lit_value_fromobject(function));
    }
    if(isexport)
    {
        lit_astemit_emit1op(emitter, emitter->last_line, OP_SET_GLOBAL);
        lit_astemit_emitshort(emitter, emitter->last_line, lit_astemit_addconstant(emitter, emitter->last_line, lit_value_fromobject(name)));
    }
    else if(isprivate)
    {
        lit_astemit_emitbyteorshort(emitter, emitter->last_line, OP_SET_PRIVATE, OP_SET_PRIVATE_LONG, index);
    }
    else
    {
        lit_astemit_emitbyteorshort(emitter, emitter->last_line, OP_SET_LOCAL, OP_SET_LOCAL_LONG, index);
    }
    lit_astemit_emit1op(emitter, emitter->last_line, OP_POP);
    return true;
}

static bool lit_astemit_doemitmethod(LitAstEmitter* emitter, LitAstExpression* expr)
{
    bool vararg;
    bool constructor;
    size_t i;
    LitAstCompiler compiler;
    LitFunction* function;
    LitAstMethodExpr* mthstmt;
    mthstmt = (LitAstMethodExpr*)expr;
    constructor = memcmp(mthstmt->name->chars, "constructor", 11) == 0;
    if(constructor && mthstmt->is_static)
    {
        lit_astemit_raiseerror(emitter, expr->line, "constructors cannot be static (at least for now)");
        return false;
    }
    lit_compiler_compiler(emitter, &compiler,
                  constructor ? LITFUNC_CONSTRUCTOR : (mthstmt->is_static ? LITFUNC_STATIC_METHOD : LITFUNC_METHOD));
    lit_astemit_beginscope(emitter);
    vararg = lit_astemit_emitparamlist(emitter, &mthstmt->parameters, expr->line);
    lit_astemit_emitexpression(emitter, mthstmt->body);
    lit_astemit_endscope(emitter, emitter->last_line);
    function = lit_compiler_end(emitter, lit_value_asstring(lit_string_format(emitter->state, "@:@", lit_value_fromobject(emitter->class_name), lit_value_fromobject(mthstmt->name))));
    function->arg_count = mthstmt->parameters.count;
    function->max_slots += function->arg_count;
    function->vararg = vararg;
    if(function->upvalue_count > 0)
    {
        lit_astemit_emit1op(emitter, emitter->last_line, OP_CLOSURE);
        lit_astemit_emitshort(emitter, emitter->last_line, lit_astemit_addconstant(emitter, emitter->last_line, lit_value_fromobject(function)));
        for(i = 0; i < function->upvalue_count; i++)
        {
            lit_astemit_emit2bytes(emitter, emitter->last_line, compiler.upvalues[i].isLocal ? 1 : 0, compiler.upvalues[i].index);
        }
    }
    else
    {
        lit_astemit_emitconstant(emitter, emitter->last_line, lit_value_fromobject(function));
    }
    lit_astemit_emit1op(emitter, emitter->last_line, mthstmt->is_static ? OP_STATIC_FIELD : OP_METHOD);
    lit_astemit_emitshort(emitter, emitter->last_line, lit_astemit_addconstant(emitter, expr->line, lit_value_fromobject(mthstmt->name)));
    return true;
}

static bool lit_astemit_doemitclass(LitAstEmitter* emitter, LitAstExpression* expr)
{
    size_t i;
    uint8_t superidx;
    LitAstExpression* s;
    LitAstClassExpr* clstmt;
    LitAstAssignVarExpr* var;
    clstmt = (LitAstClassExpr*)expr;
    emitter->class_name = clstmt->name;
    if(clstmt->parent != NULL)
    {
        lit_astemit_emit1op(emitter, emitter->last_line, OP_GET_GLOBAL);
        lit_astemit_emitshort(emitter, emitter->last_line, lit_astemit_addconstant(emitter, emitter->last_line, lit_value_fromobject(clstmt->parent)));
    }
    lit_astemit_emit1op(emitter, expr->line, OP_CLASS);
    lit_astemit_emitshort(emitter, emitter->last_line, lit_astemit_addconstant(emitter, emitter->last_line, lit_value_fromobject(clstmt->name)));
    if(clstmt->parent != NULL)
    {
        lit_astemit_emit1op(emitter, emitter->last_line, OP_INHERIT);
        emitter->class_has_super = true;
        lit_astemit_beginscope(emitter);
        superidx = lit_astemit_addlocal(emitter, "super", 5, emitter->last_line, false);
        
        lit_astemit_marklocalinit(emitter, superidx);
    }
    for(i = 0; i < clstmt->fields.count; i++)
    {
        s = clstmt->fields.values[i];
        if(s->type == LITEXPR_VARSTMT)
        {
            var = (LitAstAssignVarExpr*)s;
            lit_astemit_emitexpression(emitter, var->init);
            lit_astemit_emit1op(emitter, expr->line, OP_STATIC_FIELD);
            lit_astemit_emitshort(emitter, expr->line,
                       lit_astemit_addconstant(emitter, expr->line,
                                    lit_value_fromobject(lit_string_copy(emitter->state, var->name, var->length))));
        }
        else
        {
            lit_astemit_emitexpression(emitter, s);
        }
    }
    lit_astemit_emit1op(emitter, emitter->last_line, OP_POP);
    if(clstmt->parent != NULL)
    {
        lit_astemit_endscope(emitter, emitter->last_line);
    }
    emitter->class_name = NULL;
    emitter->class_has_super = false;
    return true;
}

static bool lit_astemit_doemitfield(LitAstEmitter* emitter, LitAstExpression* expr)
{
    LitAstCompiler compiler;
    LitField* field;
    LitFunction* getter;
    LitFunction* setter;
    LitAstFieldExpr* fieldstmt;
    fieldstmt = (LitAstFieldExpr*)expr;
    getter = NULL;
    setter = NULL;
    if(fieldstmt->getter != NULL)
    {
        lit_compiler_compiler(emitter, &compiler, fieldstmt->is_static ? LITFUNC_STATIC_METHOD : LITFUNC_METHOD);
        lit_astemit_beginscope(emitter);
        lit_astemit_emitexpression(emitter, fieldstmt->getter);
        lit_astemit_endscope(emitter, emitter->last_line);
        getter = lit_compiler_end(emitter,
            lit_value_asstring(lit_string_format(emitter->state, "@:get @", lit_value_fromobject(emitter->class_name), fieldstmt->name)));
    }
    if(fieldstmt->setter != NULL)
    {
        lit_compiler_compiler(emitter, &compiler, fieldstmt->is_static ? LITFUNC_STATIC_METHOD : LITFUNC_METHOD);
        lit_astemit_marklocalinit(emitter, lit_astemit_addlocal(emitter, "value", 5, expr->line, false));
        lit_astemit_beginscope(emitter);
        lit_astemit_emitexpression(emitter, fieldstmt->setter);
        lit_astemit_endscope(emitter, emitter->last_line);
        setter = lit_compiler_end(emitter,
            lit_value_asstring(lit_string_format(emitter->state, "@:set @", lit_value_fromobject(emitter->class_name), fieldstmt->name)));
        setter->arg_count = 1;
        setter->max_slots++;
    }
    field = lit_create_field(emitter->state, (LitObject*)getter, (LitObject*)setter);
    lit_astemit_emitconstant(emitter, expr->line, lit_value_fromobject(field));
    lit_astemit_emit1op(emitter, expr->line, fieldstmt->is_static ? OP_STATIC_FIELD : OP_DEFINE_FIELD);
    lit_astemit_emitshort(emitter, expr->line, lit_astemit_addconstant(emitter, expr->line, lit_value_fromobject(fieldstmt->name)));
    return true;
}

static bool lit_astemit_doemitrange(LitAstEmitter* emitter, LitAstExpression* expr)
{
    LitAstRangeExpr* rangeexpr;
    rangeexpr = (LitAstRangeExpr*)expr;
    lit_astemit_emitexpression(emitter, rangeexpr->to);
    lit_astemit_emitexpression(emitter, rangeexpr->from);
    lit_astemit_emit1op(emitter, expr->line, OP_RANGE);
    return true;
}

static bool lit_astemit_doemitsubscript(LitAstEmitter* emitter, LitAstExpression* expr)
{
    LitAstIndexExpr* subscrexpr;
    subscrexpr = (LitAstIndexExpr*)expr;
    lit_astemit_emitexpression(emitter, subscrexpr->array);
    lit_astemit_emitexpression(emitter, subscrexpr->index);
    lit_astemit_emit1op(emitter, expr->line, OP_SUBSCRIPT_GET);
    return true;
}

static bool lit_astemit_doemitexpression(LitAstEmitter* emitter, LitAstExpression* expr)
{
    LitAstExprExpr* stmtexpr;
    stmtexpr = (LitAstExprExpr*)expr;
    lit_astemit_emitexpression(emitter, stmtexpr->expression);
    if(stmtexpr->pop)
    {
        lit_astemit_emit1op(emitter, expr->line, OP_POP);
    }
    return true;
}

static bool lit_astemit_emitexpression(LitAstEmitter* emitter, LitAstExpression* expr)
{
    if(expr == NULL)
    {
        return false;
    }
    switch(expr->type)
    {
        case LITEXPR_LITERAL:
            {
                if(!lit_astemit_doemitliteral(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_BINARY:
            {
                if(!lit_astemit_doemitbinary(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_UNARY:
            {
                if(!lit_astemit_doemitunary(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_VAREXPR:
            {
                if(!lit_astemit_doemitvarexpr(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_ASSIGN:
            {
                if(!lit_astemit_doemitassign(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_CALL:
            {
                if(!lit_astemit_doemitcall(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_GET:
            {
                if(!lit_astemit_doemitget(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_SET:
            {
                if(!lit_astemit_doemitset(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_LAMBDA:
            {
                if(!lit_astemit_doemitlambda(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_ARRAY:
            {
                if(!lit_astemit_doemitarray(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_OBJECT:
            {
                if(!lit_astemit_doemitobject(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_SUBSCRIPT:
            {
                if(!lit_astemit_doemitsubscript(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_THIS:
            {
                if(!lit_astemit_doemitthis(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_SUPER:
            {
                if(!lit_astemit_doemitsuper(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_RANGE:
            {
                if(!lit_astemit_doemitrange(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_TERNARY:
            {
                if(!lit_astemit_doemitternary(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_INTERPOLATION:
            {
                if(!lit_astemit_doemitinterpolation(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_REFERENCE:
            {
                if(!lit_astemit_doemitreference(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_EXPRESSION:
            {
                if(!lit_astemit_doemitexpression(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_BLOCK:
            {
                if(!lit_astemit_doemitblockstmt(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_VARSTMT:
            {
                if(!lit_astemit_doemitvarstmt(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_IFSTMT:
            {
                if(!lit_astemit_doemitifstmt(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_WHILE:
            {
                if(!lit_astemit_doemitwhilestmt(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_FOR:
            {
                if(!lit_astemit_doemitforstmt(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_CONTINUE:
            {
                if(!lit_astemit_doemitcontinue(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_BREAK:
            {
                if(!lit_astemit_doemitbreak(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_FUNCTION:
            {
                if(!lit_astemit_doemitfunction(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_RETURN:
            {
                if(!lit_astemit_doemitreturn(emitter, expr))
                {
                    return false;
                }
                //return true;
            }
            break;
        case LITEXPR_METHOD:
            {
                if(!lit_astemit_doemitmethod(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_CLASS:
            {
                if(!lit_astemit_doemitclass(emitter, expr))
                {
                    return false;
                }
            }
            break;
        case LITEXPR_FIELD:
            {
                if(!lit_astemit_doemitfield(emitter, expr))
                {
                    return false;
                }
            }
            break;
        default:
            {
                lit_astemit_raiseerror(emitter, expr->line, "unknown expression with id '%i'", (int)expr->type);
            }
            break;
    }
    emitter->previous_was_expression_statement = expr->type == LITEXPR_EXPRESSION;
    return false;
}

LitModule* lit_astemit_modemit(LitAstEmitter* emitter, LitAstExprList* statements, LitString* module_name)
{
    size_t i;
    size_t total;
    size_t oldprivatescnt;
    bool isnew;
    LitState* state;        
    LitValue modulevalue;
    LitModule* module;
    LitAstPrivList* privates;
    LitAstCompiler compiler;
    LitAstExpression* exstmt;
    emitter->last_line = 1;
    emitter->emit_reference = 0;
    state = emitter->state;
    isnew = false;
    if(lit_table_get(&emitter->state->vm->modules->values, module_name, &modulevalue))
    {
        module = lit_value_asmodule(modulevalue);
    }
    else
    {
        module = lit_object_makemodule(emitter->state, module_name);
        isnew = true;
    }
    emitter->module = module;
    oldprivatescnt = module->private_count;
    if(oldprivatescnt > 0)
    {
        privates = &emitter->privates;
        privates->count = oldprivatescnt - 1;
        lit_privlist_push(state, privates, (LitAstPrivate){ true, false });
        for(i = 0; i < oldprivatescnt; i++)
        {
            privates->values[i].initialized = true;
        }
    }
    lit_compiler_compiler(emitter, &compiler, LITFUNC_SCRIPT);
    emitter->chunk = &compiler.function->chunk;
    resolve_statements(emitter, statements);
    for(i = 0; i < statements->count; i++)
    {
        exstmt = statements->values[i];
        if(lit_astemit_emitexpression(emitter, exstmt))
        {
            break;
        }
    }
    lit_astemit_endscope(emitter, emitter->last_line);
    module->main_function = lit_compiler_end(emitter, module_name);
    if(isnew)
    {
        total = emitter->privates.count;
        module->privates = LIT_ALLOCATE(emitter->state, sizeof(LitValue), total);
        for(i = 0; i < total; i++)
        {
            module->privates[i] = NULL_VALUE;
        }
    }
    else
    {
        module->privates = LIT_GROW_ARRAY(emitter->state, module->privates, sizeof(LitValue), oldprivatescnt, module->private_count);
        for(i = oldprivatescnt; i < module->private_count; i++)
        {
            module->privates[i] = NULL_VALUE;
        }
    }
    lit_privlist_destroy(emitter->state, &emitter->privates);
    if(lit_astopt_isoptenabled(LITOPTSTATE_PRIVATE_NAMES))
    {
        lit_table_destroy(emitter->state, &emitter->module->private_names->values);
    }
    if(isnew && !state->had_error)
    {
        lit_table_set(state, &state->vm->modules->values, module_name, lit_value_fromobject(module));
    }
    module->ran = true;
    return module;
}
