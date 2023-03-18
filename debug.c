
#include <stdio.h>
#include "priv.h"

void tin_disassemble_module(TinState* state, TinModule* module, const char* source)
{
    tin_disassemble_chunk(state, &module->main_function->chunk, module->main_function->name->data, source);
}

void tin_disassemble_chunk(TinState* state, TinChunk* chunk, const char* name, const char* source)
{
    size_t i;
    size_t offset;
    TinValue value;
    TinValList* values;
    TinFunction* function;
    values = &chunk->constants;

    for(i = 0; i < tin_vallist_count(values); i++)
    {
        value = tin_vallist_get(values, i);
        if(tin_value_isfunction(value))
        {
            function = tin_value_asfunction(value);
            tin_disassemble_chunk(state, &function->chunk, function->name->data, source);
        }
    }
    tin_writer_writeformat(&state->debugwriter, "== %s ==\n", name);
    for(offset = 0; offset < chunk->count;)
    {
        offset = tin_disassemble_instruction(state, chunk, offset, source);
    }
}

static size_t print_simple_op(TinState* state, TinWriter* wr, const char* name, size_t offset)
{
    (void)state;
    tin_writer_writeformat(wr, "%s%s%s\n", COLOR_YELLOW, name, COLOR_RESET);
    return offset + 1;
}

static size_t print_constant_op(TinState* state, TinWriter* wr, const char* name, TinChunk* chunk, size_t offset, bool big)
{
    uint8_t constant;
    if(big)
    {
        constant = (uint16_t)(chunk->code[offset + 1] << 8);
        constant |= chunk->code[offset + 2];
    }
    else
    {
        constant = chunk->code[offset + 1];
    }
    tin_writer_writeformat(wr, "%s%-16s%s %4d '", COLOR_YELLOW, name, COLOR_RESET, constant);
    tin_towriter_value(state, wr, tin_vallist_get(&chunk->constants, constant), true);
    tin_writer_writeformat(wr, "'\n");
    return offset + (big ? 3 : 2);
}

static size_t print_byte_op(TinState* state, TinWriter* wr, const char* name, TinChunk* chunk, size_t offset)
{
    uint8_t slot;
    (void)state;
    slot = chunk->code[offset + 1];
    tin_writer_writeformat(wr, "%s%-16s%s %4d\n", COLOR_YELLOW, name, COLOR_RESET, slot);
    return offset + 2;
}

static size_t print_short_op(TinState* state, TinWriter* wr, const char* name, TinChunk* chunk, size_t offset)
{
    uint16_t slot;
    (void)state;
    slot = (uint16_t)(chunk->code[offset + 1] << 8);
    slot |= chunk->code[offset + 2];
    tin_writer_writeformat(wr, "%s%-16s%s %4d\n", COLOR_YELLOW, name, COLOR_RESET, slot);
    return offset + 2;
}

static size_t print_jump_op(TinState* state, TinWriter* wr, const char* name, int sign, TinChunk* chunk, size_t offset)
{
    uint16_t jump;
    (void)state;
    jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    tin_writer_writeformat(wr, "%s%-16s%s %4d -> %d\n", COLOR_YELLOW, name, COLOR_RESET, (int)offset, (int)(offset + 3 + sign * jump));
    return offset + 3;
}

static size_t print_invoke_op(TinState* state, TinWriter* wr, const char* name, TinChunk* chunk, size_t offset)
{
    uint8_t arg_count;
    uint8_t constant;
    (void)state;
    arg_count = chunk->code[offset + 1];
    constant = chunk->code[offset + 2];
    constant |= chunk->code[offset + 3];
    tin_writer_writeformat(wr, "%s%-16s%s (%d args) %4d '", COLOR_YELLOW, name, COLOR_RESET, arg_count, constant);
    tin_towriter_value(state, wr, tin_vallist_get(&chunk->constants, constant), true);
    tin_writer_writeformat(wr, "'\n");
    return offset + 4;
}

size_t tin_disassemble_instruction(TinState* state, TinChunk* chunk, size_t offset, const char* source)
{
    bool same;
    int islocal;
    size_t j;
    size_t line;
    size_t index;
    int16_t constant;
    uint8_t instruction;
    char c;
    char* nextline;
    char* prevline;
    char* outputline;
    char* currentline;
    TinWriter* wr;
    TinFunction* function;
    wr = &state->debugwriter;
    line = tin_chunk_getline(chunk, offset);
    same = !chunk->has_line_info || (offset > 0 && line == tin_chunk_getline(chunk, offset - 1));
    if(!same && source != NULL)
    {
        index = 0;
        currentline = (char*)source;
        while(currentline)
        {
            nextline = strchr(currentline, '\n');
            prevline = currentline;
            index++;
            currentline = nextline ? (nextline + 1) : NULL;
            if(index == line)
            {
                outputline = prevline ? prevline : nextline;
                while((c = *outputline) && (c == '\t' || c == ' '))
                {
                    outputline++;
                }
                tin_writer_writeformat(wr, "%s        %.*s%s\n", COLOR_RED, nextline ? (int)(nextline - outputline) : (int)strlen(prevline), outputline, COLOR_RESET);
                break;
            }
        }
    }
    tin_writer_writeformat(wr, "%04d ", (int)offset);
    if(same)
    {
        tin_writer_writestring(wr, "   | ");
    }
    else
    {
        tin_writer_writeformat(wr, "%s%4d%s ", COLOR_BLUE, (int)line, COLOR_RESET);
    }
    instruction = chunk->code[offset];
    switch(instruction)
    {
        case OP_POP:
            return print_simple_op(state, wr, "OP_POP", offset);
        case OP_POPLOCALS:
            return print_constant_op(state, wr, "OP_POPLOCALS", chunk, offset, true);
        case OP_RETURN:
            return print_simple_op(state, wr, "OP_RETURN", offset);
        case OP_CONSTVALUE:
            return print_constant_op(state, wr, "OP_CONSTVALUE", chunk, offset, false);
        case OP_CONSTLONG:
            return print_constant_op(state, wr, "OP_CONSTLONG", chunk, offset, true);
        case OP_VALTRUE:
            return print_simple_op(state, wr, "OP_VALTRUE", offset);
        case OP_VALFALSE:
            return print_simple_op(state, wr, "OP_VALFALSE", offset);
        case OP_VALNULL:
            return print_simple_op(state, wr, "OP_VALNULL", offset);
        case OP_NEGATE:
            return print_simple_op(state, wr, "OP_NEGATE", offset);
        case OP_NOT:
            return print_simple_op(state, wr, "OP_NOT", offset);
        case OP_MATHADD:
            return print_simple_op(state, wr, "OP_MATHADD", offset);
        case OP_MATHSUB:
            return print_simple_op(state, wr, "OP_MATHSUB", offset);
        case OP_MATHMULT:
            return print_simple_op(state, wr, "OP_MATHMULT", offset);
        case OP_MATHPOWER:
            return print_simple_op(state, wr, "OP_MATHPOWER", offset);
        case OP_MATHDIV:
            return print_simple_op(state, wr, "OP_MATHDIV", offset);
        case OP_MATHFLOORDIV:
            return print_simple_op(state, wr, "OP_MATHFLOORDIV", offset);
        case OP_MATHMOD:
            return print_simple_op(state, wr, "OP_MATHMOD", offset);
        case OP_BINAND:
            return print_simple_op(state, wr, "OP_BINAND", offset);
        case OP_BINOR:
            return print_simple_op(state, wr, "OP_BINOR", offset);
        case OP_BINXOR:
            return print_simple_op(state, wr, "OP_BINXOR", offset);
        case OP_LEFTSHIFT:
            return print_simple_op(state, wr, "OP_LEFTSHIFT", offset);
        case OP_RIGHTSHIFT:
            return print_simple_op(state, wr, "OP_RIGHTSHIFT", offset);
        case OP_BINNOT:
            return print_simple_op(state, wr, "OP_BINNOT", offset);
        case OP_EQUAL:
            return print_simple_op(state, wr, "OP_EQUAL", offset);
        case OP_GREATERTHAN:
            return print_simple_op(state, wr, "OP_GREATERTHAN", offset);
        case OP_GREATEREQUAL:
            return print_simple_op(state, wr, "OP_GREATEREQUAL", offset);
        case OP_LESSTHAN:
            return print_simple_op(state, wr, "OP_LESSTHAN", offset);
        case OP_LESSEQUAL:
            return print_simple_op(state, wr, "OP_LESSEQUAL", offset);
        case OP_GLOBALSET:
            return print_constant_op(state, wr, "OP_GLOBALSET", chunk, offset, true);
        case OP_GLOBALGET:
            return print_constant_op(state, wr, "OP_GLOBALGET", chunk, offset, true);
        case OP_LOCALSET:
            return print_byte_op(state, wr, "OP_LOCALSET", chunk, offset);
        case OP_LOCALGET:
            return print_byte_op(state, wr, "OP_LOCALGET", chunk, offset);
        case OP_LOCALLONGSET:
            return print_short_op(state, wr, "OP_LOCALLONGSET", chunk, offset);
        case OP_LOCALLONGGET:
            return print_short_op(state, wr, "OP_LOCALLONGGET", chunk, offset);
        case OP_PRIVATESET:
            return print_byte_op(state, wr, "OP_PRIVATESET", chunk, offset);
        case OP_PRIVATEGET:
            return print_byte_op(state, wr, "OP_PRIVATEGET", chunk, offset);
        case OP_PRIVATELONGSET:
            return print_short_op(state, wr, "OP_PRIVATELONGSET", chunk, offset);
        case OP_PRIVATELONGGET:
            return print_short_op(state, wr, "OP_PRIVATELONGGET", chunk, offset);
        case OP_UPVALSET:
            return print_byte_op(state, wr, "OP_UPVALSET", chunk, offset);
        case OP_UPVALGET:
            return print_byte_op(state, wr, "OP_UPVALGET", chunk, offset);
        case OP_JUMPIFFALSE:
            return print_jump_op(state, wr, "OP_JUMPIFFALSE", 1, chunk, offset);
        case OP_JUMPIFNULL:
            return print_jump_op(state, wr, "OP_JUMPIFNULL", 1, chunk, offset);
        case OP_JUMPIFNULLPOP:
            return print_jump_op(state, wr, "OP_JUMPIFNULLPOP", 1, chunk, offset);
        case OP_JUMPALWAYS:
            return print_jump_op(state, wr, "OP_JUMPALWAYS", 1, chunk, offset);
        case OP_JUMPBACK:
            return print_jump_op(state, wr, "OP_JUMPBACK", -1, chunk, offset);
        case OP_AND:
            return print_jump_op(state, wr, "OP_AND", 1, chunk, offset);
        case OP_OR:
            return print_jump_op(state, wr, "OP_OR", 1, chunk, offset);
        case OP_NULLOR:
            return print_jump_op(state, wr, "OP_NULLOR", 1, chunk, offset);
        case OP_CALLFUNCTION:
            return print_byte_op(state, wr, "OP_CALLFUNCTION", chunk, offset);
        case OP_MAKECLOSURE:
            {
                offset++;
                constant = (uint16_t)(chunk->code[offset] << 8);
                offset++;
                constant |= chunk->code[offset];
                tin_writer_writeformat(wr, "%-16s %4d ", "OP_MAKECLOSURE", constant);
                tin_towriter_value(state, wr, tin_vallist_get(&chunk->constants, constant), true);
                tin_writer_writeformat(wr, "\n");
                function = tin_value_asfunction(tin_vallist_get(&chunk->constants, constant));
                for(j = 0; j < function->upvalue_count; j++)
                {
                    islocal = chunk->code[offset++];
                    index = chunk->code[offset++];
                    tin_writer_writeformat(wr, "%04d      |                     %s %d\n", (int)(offset - 2), islocal ? "local" : "upvalue", (int)index);
                }
                return offset;
            }
            break;
        case OP_UPVALCLOSE:
            return print_simple_op(state, wr, "OP_UPVALCLOSE", offset);
        case OP_MAKECLASS:
            return print_constant_op(state, wr, "OP_MAKECLASS", chunk, offset, true);

        case OP_FIELDGET:
            return print_simple_op(state, wr, "OP_FIELDGET", offset);
        case OP_FIELDSET:
            return print_simple_op(state, wr, "OP_FIELDSET", offset);

        case OP_GETINDEX:
            return print_simple_op(state, wr, "OP_GETINDEX", offset);
        case OP_SETINDEX:
            return print_simple_op(state, wr, "OP_SETINDEX", offset);
        case OP_VALARRAY:
            return print_simple_op(state, wr, "OP_VALARRAY", offset);
        case OP_ARRAYPUSHVALUE:
            return print_simple_op(state, wr, "OP_ARRAYPUSHVALUE", offset);
        case OP_VALOBJECT:
            return print_simple_op(state, wr, "OP_VALOBJECT", offset);
        case OP_OBJECTPUSHFIELD:
            return print_simple_op(state, wr, "OP_OBJECTPUSHFIELD", offset);
        case OP_RANGE:
            return print_simple_op(state, wr, "OP_RANGE", offset);
        case OP_MAKEMETHOD:
            return print_constant_op(state, wr, "OP_MAKEMETHOD", chunk, offset, true);
        case OP_FIELDSTATIC:
            return print_constant_op(state, wr, "OP_FIELDSTATIC", chunk, offset, true);
        case OP_FIELDDEFINE:
            return print_constant_op(state, wr, "OP_FIELDDEFINE", chunk, offset, true);
        case OP_INVOKEMETHOD:
            return print_invoke_op(state, wr, "OP_INVOKEMETHOD", chunk, offset);
        case OP_INVOKESUPER:
            return print_invoke_op(state, wr, "OP_INVOKESUPER", chunk, offset);
        case OP_INVOKEIGNORING:
            return print_invoke_op(state, wr, "OP_INVOKEIGNORING", chunk, offset);
        case OP_INVOKESUPERIGNORING:
            return print_invoke_op(state, wr, "OP_INVOKESUPERIGNORING", chunk, offset);
        case OP_CLASSINHERIT:
            return print_simple_op(state, wr, "OP_CLASSINHERIT", offset);
        case OP_ISCLASS:
            return print_simple_op(state, wr, "OP_ISCLASS", offset);
        case OP_GETSUPERMETHOD:
            return print_constant_op(state, wr, "OP_GETSUPERMETHOD", chunk, offset, true);
        case OP_VARARG:
            return print_byte_op(state, wr, "OP_VARARG", chunk, offset);
        case OP_REFFIELD:
            return print_simple_op(state, wr, "OP_REFFIELD", offset);
        case OP_REFUPVAL:
            return print_byte_op(state, wr, "OP_REFUPVAL", chunk, offset);
        case OP_REFPRIVATE:
            return print_short_op(state, wr, "OP_REFPRIVATE", chunk, offset);
        case OP_REFLOCAL:
            return print_short_op(state, wr, "OP_REFLOCAL", chunk, offset);
        case OP_REFGLOBAL:
            return print_constant_op(state, wr, "OP_REFGLOBAL", chunk, offset, true);
        case OP_REFSET:
            return print_simple_op(state, wr, "OP_REFSET", offset);
        default:
            {
                tin_writer_writeformat(wr, "Unknown opcode %d\n", instruction);
                return offset + 1;
            }
            break;
    }
}

void tin_trace_frame(TinFiber* fiber, TinWriter* wr)
{
    TinCallFrame* frame;
    (void)fiber;
    (void)frame;
    (void)wr;
#ifdef TIN_TRACE_STACK
    if(fiber == NULL)
    {
        return;
    }
    frame = &fiber->frames[fiber->frame_count - 1];
    tin_writer_writeformat(wr, "== fiber %p f%i %s (expects %i, max %i, added %i, current %i, exits %i) ==\n", fiber,
           fiber->frame_count - 1, frame->function->name->data, frame->function->arg_count, frame->function->maxslots,
           frame->function->maxslots + (int)(fiber->stack_top - fiber->stack), fiber->stack_capacity, frame->return_to_c);
#endif
}
