
#include "priv.h"


#define TIN_CHUNK_GROWCAPACITY(cap) \
    (((cap) < 8) ? (8) : ((cap) * 2))


void tin_chunk_init(TinState* state, TinChunk* chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->haslineinfo = true;
    chunk->linecount = 0;
    chunk->linecap = 0;
    chunk->lines = NULL;
    tin_vallist_init(state, &chunk->constants);
}

void tin_chunk_destroy(TinState* state, TinChunk* chunk)
{
    tin_gcmem_freearray(state, sizeof(uint8_t), chunk->code, chunk->capacity);
    tin_gcmem_freearray(state, sizeof(uint16_t), chunk->lines, chunk->linecap);
    tin_vallist_destroy(state, &chunk->constants);
    tin_chunk_init(state, chunk);
}

void tin_chunk_push(TinState* state, TinChunk* chunk, uint8_t byte, uint16_t line)
{
    size_t value;
    size_t lineindex;
    size_t oldcapacity;
    if(chunk->capacity < chunk->count + 1)
    {
        oldcapacity = chunk->capacity;
        chunk->capacity = TIN_CHUNK_GROWCAPACITY(oldcapacity + 2);
        chunk->code = (uint8_t*)tin_gcmem_growarray(state, chunk->code, sizeof(uint8_t), oldcapacity, chunk->capacity + 2);
    }
    chunk->code[chunk->count] = byte;
    chunk->count++;
    if(!chunk->haslineinfo)
    {
        return;
    }
    if(chunk->linecap < chunk->linecount + 2)
    {
        oldcapacity = chunk->linecap;
        chunk->linecap = TIN_CHUNK_GROWCAPACITY(chunk->linecap + 2);
        chunk->lines = (uint16_t*)tin_gcmem_growarray(state, chunk->lines, sizeof(uint16_t), oldcapacity, chunk->linecap + 2);
        if(oldcapacity == 0)
        {
            chunk->lines[0] = 0;
            chunk->lines[1] = 0;
        }
    }
    lineindex = chunk->linecount;
    value = chunk->lines[lineindex];
    if(value != 0 && value != line)
    {
        chunk->linecount += 2;
        lineindex = chunk->linecount;
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
    if(!chunk->haslineinfo)
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
    if(chunk->linecap > chunk->linecount)
    {
        oldcapacity = chunk->linecap;
        chunk->linecap = chunk->linecount + 2;
        chunk->lines = (uint16_t*)tin_gcmem_growarray(state, chunk->lines, sizeof(uint16_t), oldcapacity, chunk->linecap);
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
