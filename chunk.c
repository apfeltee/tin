
#include "priv.h"

void tin_chunk_init(TinChunk* chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->has_line_info = true;
    chunk->line_count = 0;
    chunk->line_capacity = 0;
    chunk->lines = NULL;
    tin_vallist_init(&chunk->constants);
}

void tin_chunk_destroy(TinState* state, TinChunk* chunk)
{
    tin_gcmem_freearray(state, sizeof(uint8_t), chunk->code, chunk->capacity);
    tin_gcmem_freearray(state, sizeof(uint16_t), chunk->lines, chunk->line_capacity);
    tin_vallist_destroy(state, &chunk->constants);
    tin_chunk_init(chunk);
}

void tin_chunk_push(TinState* state, TinChunk* chunk, uint8_t byte, uint16_t line)
{
    size_t value;
    size_t lineindex;
    size_t oldcapacity;
    if(chunk->capacity < chunk->count + 1)
    {
        oldcapacity = chunk->capacity;
        chunk->capacity = TIN_GROW_CAPACITY(oldcapacity + 2);
        chunk->code = (uint8_t*)tin_gcmem_growarray(state, chunk->code, sizeof(uint8_t), oldcapacity, chunk->capacity + 2);
    }
    chunk->code[chunk->count] = byte;
    chunk->count++;
    if(!chunk->has_line_info)
    {
        return;
    }
    if(chunk->line_capacity < chunk->line_count + 2)
    {
        oldcapacity = chunk->line_capacity;
        chunk->line_capacity = TIN_GROW_CAPACITY(chunk->line_capacity + 2);
        chunk->lines = (uint16_t*)tin_gcmem_growarray(state, chunk->lines, sizeof(uint16_t), oldcapacity, chunk->line_capacity + 2);
        if(oldcapacity == 0)
        {
            chunk->lines[0] = 0;
            chunk->lines[1] = 0;
        }
    }
    lineindex = chunk->line_count;
    value = chunk->lines[lineindex];
    if(value != 0 && value != line)
    {
        chunk->line_count += 2;
        lineindex = chunk->line_count;
        chunk->lines[lineindex + 1] = 0;
    }
    chunk->lines[lineindex] = line;
    chunk->lines[lineindex + 1]++;
}

size_t tin_chunk_addconst(TinState* state, TinChunk* chunk, TinValue constant)
{
    size_t i;
    TinValue itm;
    TinState** cst;
    cst = &state;
    for(i = 0; i < tin_vallist_count(&chunk->constants); i++)
    {
        itm = tin_vallist_get(&chunk->constants, i);
        if(&itm == &constant)
        {
            return i;
        }
    }
    tin_state_pushvalueroot(state, constant);
    tin_vallist_push(*cst, &chunk->constants, constant);
    tin_state_poproot(state);
    return tin_vallist_count(&chunk->constants) - 1;
}

size_t tin_chunk_getline(TinChunk* chunk, size_t offset)
{
    size_t i;
    size_t rle;
    size_t line;
    size_t index;
    if(!chunk->has_line_info)
    {
        return 0;
    }
    rle = 0;
    line = 0;
    index = 0;
    for(i = 0; i < offset + 1; i++)
    {
        if(rle > 0)
        {
            rle--;
            continue;
        }
        line = chunk->lines[index];
        rle = chunk->lines[index + 1];
        if(rle > 0)
        {
            rle--;
        }
        index += 2;
    }
    return line;
}

void tin_chunk_shrink(TinState* state, TinChunk* chunk)
{
    size_t oldcapacity;
    if(chunk->capacity > chunk->count)
    {
        oldcapacity = chunk->capacity;
        chunk->capacity = chunk->count;
        chunk->code = (uint8_t*)tin_gcmem_growarray(state, chunk->code, sizeof(uint8_t), oldcapacity, chunk->capacity);
    }
    if(chunk->line_capacity > chunk->line_count)
    {
        oldcapacity = chunk->line_capacity;
        chunk->line_capacity = chunk->line_count + 2;
        chunk->lines = (uint16_t*)tin_gcmem_growarray(state, chunk->lines, sizeof(uint16_t), oldcapacity, chunk->line_capacity);
    }
}

void tin_chunk_emitbyte(TinState* state, TinChunk* chunk, uint8_t byte)
{
    tin_chunk_push(state, chunk, byte, 1);
}

void tin_chunk_emit2bytes(TinState* state, TinChunk* chunk, uint8_t a, uint8_t b)
{
    tin_chunk_push(state, chunk, a, 1);
    tin_chunk_push(state, chunk, b, 1);
}

void tin_chunk_emitshort(TinState* state, TinChunk* chunk, uint16_t value)
{
    tin_chunk_emit2bytes(state, chunk, (uint8_t)((value >> 8) & 0xff), (uint8_t)(value & 0xff));
}
