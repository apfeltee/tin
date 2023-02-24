
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#ifdef TIN_OS_WINDOWS
    #include <windows.h>
    #define stat _stat
#endif
#include "dirwrap.h"
#include "priv.h"
#include "sds.h"

#if defined (S_IFDIR) && !defined (S_ISDIR)
    #define	S_ISDIR(m)	(((m)&S_IFMT) == S_IFDIR)	/* directory */
#endif
#if defined (S_IFREG) && !defined (S_ISREG)
    #define	S_ISREG(m)	(((m)&S_IFMT) == S_IFREG)	/* file */
#endif

typedef struct TinFileData TinFileData;
typedef struct TinStdioHandle TinStdioHandle;

struct TinFileData
{
    char* path;
    FILE* handle;
    bool isopen;
};

struct TinStdioHandle
{
    FILE* handle;
    const char* name;
    bool canread;
    bool canwrite;
};

typedef void(*CleanupFunc)(TinState*, TinUserdata*, bool);

static uint8_t g_tmpbyte;
static uint16_t g_tmpshort;
static uint32_t g_tmpint;
static double g_tmpdouble;

static void* tin_util_instancedataset(TinVM* vm, TinValue instance, size_t typsz, CleanupFunc cleanup)
{
    TinUserdata* userdata = tin_object_makeuserdata(vm->state, typsz, false);
    userdata->cleanup_fn = cleanup;
    tin_table_set(vm->state, &tin_value_asinstance(instance)->fields, tin_string_copyconst(vm->state, "_data"), tin_value_fromobject(userdata));
    return userdata->data;
}

static void* tin_util_instancedataget(TinVM* vm, TinValue instance)
{
    TinValue _d;
    if(!tin_table_get(&tin_value_asinstance(instance)->fields, tin_string_copyconst(vm->state, "_data"), &_d))
    {
        tin_vm_raiseexitingerror(vm, "failed to extract userdata");
    }
    return tin_value_asuserdata(_d)->data;
}

char* tin_util_readfile(const char* path, size_t* dlen)
{
    size_t fileSize;
    char* buffer;
    size_t bytesread;
    FILE* file;
    file = fopen(path, "rb");
    if(file == NULL)
    {
        return NULL;
    }
    fseek(file, 0L, SEEK_END);
    fileSize = ftell(file);
    rewind(file);
    buffer = (char*)malloc(fileSize + 1);
    bytesread = fread(buffer, sizeof(char), fileSize, file);
    buffer[bytesread] = '\0';
    fclose(file);
    *dlen = bytesread;
    return buffer;
}

bool tin_fs_fileexists(const char* path)
{
    struct stat buffer;
    if(stat(path, &buffer) != -1)
    {
        return S_ISREG(buffer.st_mode);
    }
    return false;
}

bool tin_fs_direxists(const char* path)
{
    struct stat buffer;
    if(stat(path, &buffer) != -1)
    {
        return S_ISDIR(buffer.st_mode);
    }
    return false;
}

size_t tin_ioutil_writeuint8(FILE* file, uint8_t byte)
{
    return fwrite(&byte, sizeof(uint8_t), 1, file);
}

size_t tin_ioutil_writeuint16(FILE* file, uint16_t byte)
{
    return fwrite(&byte, sizeof(uint16_t), 1, file);
}

size_t tin_ioutil_writeuint32(FILE* file, uint32_t byte)
{
    return fwrite(&byte, sizeof(uint32_t), 1, file);
}

size_t tin_ioutil_writedouble(FILE* file, double byte)
{
    return fwrite(&byte, sizeof(double), 1, file);
}

size_t tin_ioutil_writestring(FILE* file, TinString* string)
{
    uint16_t i;
    uint16_t c;
    size_t rt;
    c = tin_string_getlength(string);
    rt = fwrite(&c, 2, 1, file);
    for(i = 0; i < c; i++)
    {
        tin_ioutil_writeuint8(file, (uint8_t)string->chars[i] ^ TIN_STRING_KEY);
    }
    return (rt + i);
}

uint8_t tin_ioutil_readuint8(FILE* file)
{
    size_t rt;
    (void)rt;
    rt = fread(&g_tmpbyte, sizeof(uint8_t), 1, file);
    return g_tmpbyte;
}

uint16_t tin_ioutil_readuint16(FILE* file)
{
    size_t rt;
    (void)rt;
    rt = fread(&g_tmpshort, sizeof(uint16_t), 1, file);
    return g_tmpshort;
}

uint32_t tin_ioutil_readuint32(FILE* file)
{
    size_t rt;
    (void)rt;
    rt = fread(&g_tmpint, sizeof(uint32_t), 1, file);
    return g_tmpint;
}

double tin_ioutil_readdouble(FILE* file)
{
    size_t rt;
    (void)rt;
    rt = fread(&g_tmpdouble, sizeof(double), 1, file);
    return g_tmpdouble;
}

TinString* tin_ioutil_readstring(TinState* state, FILE* file)
{
    size_t rt;
    uint16_t i;
    uint16_t length;
    char* line;
    (void)rt;
    rt = fread(&length, 2, 1, file);
    if(length < 1)
    {
        return NULL;
    }
    line = (char*)malloc(length + 1);
    for(i = 0; i < length; i++)
    {
        line[i] = (char)tin_ioutil_readuint8(file) ^ TIN_STRING_KEY;
    }
    return tin_string_take(state, line, length, false);
}

void tin_emufile_init(TinEmulatedFile* file, const char* source, size_t len)
{
    file->source = source;
    file->length = len;
    file->position = 0;
}

uint8_t tin_emufile_readuint8(TinEmulatedFile* file)
{
    return (uint8_t)file->source[file->position++];
}

uint16_t tin_emufile_readuint16(TinEmulatedFile* file)
{
    return (uint16_t)(tin_emufile_readuint8(file) | (tin_emufile_readuint8(file) << 8u));
}

uint32_t tin_emufile_readuint32(TinEmulatedFile* file)
{
    return (uint32_t)(
        tin_emufile_readuint8(file) |
        (tin_emufile_readuint8(file) << 8u) |
        (tin_emufile_readuint8(file) << 16u) |
        (tin_emufile_readuint8(file) << 24u)
    );
}

double tin_emufile_readdouble(TinEmulatedFile* file)
{
    size_t i;
    double result;
    uint8_t values[8];
    for(i = 0; i < 8; i++)
    {
        values[i] = tin_emufile_readuint8(file);
    }
    memcpy(&result, values, 8);
    return result;
}

TinString* tin_emufile_readstring(TinState* state, TinEmulatedFile* file)
{
    uint16_t i;
    uint16_t length;
    char* line;
    length = tin_emufile_readuint16(file);
    if(length < 1)
    {
        return NULL;
    }
    line = (char*)malloc(length + 1);
    for(i = 0; i < length; i++)
    {
        line[i] = (char)tin_emufile_readuint8(file) ^ TIN_STRING_KEY;
    }
    return tin_string_take(state, line, length, false);
}

static void tin_ioutil_writechunk(FILE* file, TinChunk* chunk);
static void tin_ioutil_readchunk(TinState* state, TinEmulatedFile* file, TinModule* module, TinChunk* chunk);

static void tin_ioutil_writefunction(FILE* file, TinFunction* function)
{
    tin_ioutil_writechunk(file, &function->chunk);
    tin_ioutil_writestring(file, function->name);
    tin_ioutil_writeuint8(file, function->arg_count);
    tin_ioutil_writeuint16(file, function->upvalue_count);
    tin_ioutil_writeuint8(file, (uint8_t)function->vararg);
    tin_ioutil_writeuint16(file, (uint16_t)function->maxslots);
}

static TinFunction* tin_ioutil_readfunction(TinState* state, TinEmulatedFile* file, TinModule* module)
{
    TinFunction* function;
    function = tin_object_makefunction(state, module);
    tin_ioutil_readchunk(state, file, module, &function->chunk);
    function->name = tin_emufile_readstring(state, file);
    function->arg_count = tin_emufile_readuint8(file);
    function->upvalue_count = tin_emufile_readuint16(file);
    function->vararg = (bool)tin_emufile_readuint8(file);
    function->maxslots = tin_emufile_readuint16(file);
    return function;
}

static void tin_ioutil_writechunk(FILE* file, TinChunk* chunk)
{
    size_t i;
    size_t c;
    TinObjType type;
    TinValue constant;
    tin_ioutil_writeuint32(file, chunk->count);
    for(i = 0; i < chunk->count; i++)
    {
        tin_ioutil_writeuint8(file, chunk->code[i]);
    }
    if(chunk->has_line_info)
    {
        c = chunk->line_count * 2 + 2;
        tin_ioutil_writeuint32(file, c);
        for(i = 0; i < c; i++)
        {
            tin_ioutil_writeuint16(file, chunk->lines[i]);
        }
    }
    else
    {
        tin_ioutil_writeuint32(file, 0);
    }
    tin_ioutil_writeuint32(file, tin_vallist_count(&chunk->constants));
    for(i = 0; i < tin_vallist_count(&chunk->constants); i++)
    {
        constant = tin_vallist_get(&chunk->constants, i);
        if(tin_value_isobject(constant))
        {
            type = tin_value_asobject(constant)->type;
            tin_ioutil_writeuint8(file, (uint8_t)(type + 1));
            switch(type)
            {
                case TINTYPE_STRING:
                    {
                        tin_ioutil_writestring(file, tin_value_asstring(constant));
                    }
                    break;
                case TINTYPE_FUNCTION:
                    {
                        tin_ioutil_writefunction(file, tin_value_asfunction(constant));
                    }
                    break;
                default:
                    {
                        UNREACHABLE
                    }
                    break;
            }
        }
        else
        {
            tin_ioutil_writeuint8(file, 0);
            tin_ioutil_writedouble(file, tin_value_asnumber(constant));
        }
    }
}

static void tin_ioutil_readchunk(TinState* state, TinEmulatedFile* file, TinModule* module, TinChunk* chunk)
{
    size_t i;
    size_t count;
    uint8_t type;
    tin_chunk_init(chunk);
    count = tin_emufile_readuint32(file);
    chunk->code = (uint8_t*)tin_gcmem_memrealloc(state, NULL, 0, sizeof(uint8_t) * count);
    chunk->count = count;
    chunk->capacity = count;
    for(i = 0; i < count; i++)
    {
        chunk->code[i] = tin_emufile_readuint8(file);
    }
    count = tin_emufile_readuint32(file);
    if(count > 0)
    {
        chunk->lines = (uint16_t*)tin_gcmem_memrealloc(state, NULL, 0, sizeof(uint16_t) * count);
        chunk->line_count = count;
        chunk->line_capacity = count;
        for(i = 0; i < count; i++)
        {
            chunk->lines[i] = tin_emufile_readuint16(file);
        }
    }
    else
    {
        chunk->has_line_info = false;
    }
    count = tin_emufile_readuint32(file);
    /*
    chunk->constants.values = (TinValue*)tin_gcmem_memrealloc(state, NULL, 0, sizeof(TinValue) * count);
    chunk->constants.count = count;
    chunk->constants.capacity = count;
    */
    tin_vallist_init(&chunk->constants);
    tin_vallist_ensuresize(state, &chunk->constants, count);
    for(i = 0; i < count; i++)
    {
        type = tin_emufile_readuint8(file);
        if(type == 0)
        {
            //chunk->constants.values[i] = tin_value_makenumber(vm->state, tin_emufile_readdouble(file));
            tin_vallist_set(&chunk->constants, i, tin_value_makefloatnumber(state, tin_emufile_readdouble(file)));
        }
        else
        {
            switch((TinObjType)(type - 1))
            {
                case TINTYPE_STRING:
                    {
                        //chunk->constants.values[i] = tin_value_fromobject(tin_emufile_readstring(state, file));
                        tin_vallist_set(&chunk->constants, i, tin_value_fromobject(tin_emufile_readstring(state, file)));

                    }
                    break;
                case TINTYPE_FUNCTION:
                    {
                        //chunk->constants.values[i] = tin_value_fromobject(tin_ioutil_readfunction(state, file, module));
                        tin_vallist_set(&chunk->constants, i, tin_value_fromobject(tin_ioutil_readfunction(state, file, module)));
                    }
                    break;
                default:
                    {
                        UNREACHABLE
                    }
                    break;
            }
        }
    }
}

void tin_ioutil_writemodule(TinModule* module, FILE* file)
{
    size_t i;
    bool disabled;
    TinTable* privates;
    disabled = tin_astopt_isoptenabled(TINOPTSTATE_PRIVATE_NAMES);
    tin_ioutil_writestring(file, module->name);
    tin_ioutil_writeuint16(file, module->private_count);
    tin_ioutil_writeuint8(file, (uint8_t)disabled);
    if(!disabled)
    {
        privates = &module->private_names->values;
        for(i = 0; i < module->private_count; i++)
        {
            if(privates->entries[i].key != NULL)
            {
                tin_ioutil_writestring(file, privates->entries[i].key);
                tin_ioutil_writeuint16(file, (uint16_t)tin_value_asnumber(privates->entries[i].value));
            }
        }
    }
    tin_ioutil_writefunction(file, module->main_function);
}

TinModule* tin_ioutil_readmodule(TinState* state, const char* input, size_t len)
{
    bool enabled;
    uint16_t i;
    uint16_t j;
    uint16_t modulecount;
    uint16_t privatescount;
    uint8_t bytecodeversion;
    TinString* name;
    TinTable* privates;
    TinModule* module;
    TinEmulatedFile file;
    tin_emufile_init(&file, input, len);
    if(tin_emufile_readuint16(&file) != TIN_BYTECODE_MAGIC_NUMBER)
    {
        tin_state_raiseerror(state, COMPILE_ERROR, "Failed to read compiled code, unknown magic number");
        return NULL;
    }
    bytecodeversion = tin_emufile_readuint8(&file);
    if(bytecodeversion > TIN_BYTECODE_VERSION)
    {
        tin_state_raiseerror(state, COMPILE_ERROR, "Failed to read compiled code, unknown bytecode version '%i'", (int)bytecodeversion);
        return NULL;
    }
    modulecount = tin_emufile_readuint16(&file);
    TinModule* first = NULL;
    for(j = 0; j < modulecount; j++)
    {
        module = tin_object_makemodule(state, tin_emufile_readstring(state, &file));
        privates = &module->private_names->values;
        privatescount = tin_emufile_readuint16(&file);
        enabled = !((bool)tin_emufile_readuint8(&file));
        module->privates = TIN_ALLOCATE(state, sizeof(TinValue), privatescount);
        module->private_count = privatescount;
        for(i = 0; i < privatescount; i++)
        {
            module->privates[i] = NULL_VALUE;
            if(enabled)
            {
                name = tin_emufile_readstring(state, &file);
                tin_table_set(state, privates, name, tin_value_makefixednumber(state, tin_emufile_readuint16(&file)));
            }
        }
        module->main_function = tin_ioutil_readfunction(state, &file, module);
        tin_table_set(state, &state->vm->modules->values, module->name, tin_value_fromobject(module));
        if(j == 0)
        {
            first = module;
        }
    }
    if(tin_emufile_readuint16(&file) != TIN_BYTECODE_END_NUMBER)
    {
        tin_state_raiseerror(state, COMPILE_ERROR, "Failed to read compiled code, unknown end number");
        return NULL;
    }
    return first;
}


/*
 * File
 */
void tin_userfile_cleanup(TinState* state, TinUserdata* data, bool mark)
{
    (void)state;
    TinFileData* fd;
    if(mark)
    {
        return;
    }
    if(data != NULL)
    {
        fd = ((TinFileData*)data->data);
        if(fd != NULL)
        {
            if((fd->handle != NULL) && (fd->isopen == true))
            {
                fclose(fd->handle);
                fd->handle = NULL;
                fd->isopen = false;
            }
        }
    }
}

static TinValue objmethod_file_constructor(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)argc;
    (void)argv;
    const char* path;
    const char* mode;
    FILE* hnd;
    TinFileData* data;
    TinStdioHandle* hstd;    
    if(argc > 1)
    {
        if(tin_value_isuserdata(argv[0]))
        {
            hstd = (TinStdioHandle*)(tin_value_asuserdata(argv[0])->data);
            hnd = hstd->handle;
            fprintf(stderr, "FILE: hnd=%p name=%s\n", hstd->handle, hstd->name);
            data = (TinFileData*)tin_util_instancedataset(vm, instance, sizeof(TinFileData), NULL);
            data->path = NULL;
            data->handle = hnd;
            data->isopen = true;
        }
        else
        {
            path = tin_value_checkstring(vm, argv, argc, 0);
            mode = tin_value_getstring(vm, argv, argc, 1, "r");
            hnd = fopen(path, mode);
            if(hnd == NULL)
            {
                tin_vm_raiseexitingerror(vm, "Failed to open file %s with mode %s (C error: %s)", path, mode, strerror(errno));
            }
            data = (TinFileData*)tin_util_instancedataset(vm, instance, sizeof(TinFileData), tin_userfile_cleanup);
            data->path = (char*)path;
            data->handle = hnd;
            data->isopen = true;
        }
    }
    else
    {
        tin_vm_raiseexitingerror(vm, "File() expects either string|string, or userdata|string");
        return NULL_VALUE;
    }
    return instance;
}


static TinValue objmethod_file_close(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    TinFileData* data;
    data = (TinFileData*)tin_util_instancedataget(vm, instance);
    fclose(data->handle);
    data->handle = NULL;
    data->isopen = false;
    return NULL_VALUE;
}

static TinValue objmethod_file_exists(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    char* filename;
    filename = NULL;
    if(tin_value_isinstance(instance))
    {
        filename = ((TinFileData*)tin_util_instancedataget(vm, instance))->path;
    }
    else
    {
        filename = (char*)tin_value_checkstring(vm, argv, argc, 0);
    }
    return tin_value_makebool(vm->state, tin_fs_fileexists(filename));
}

/*
 * ==
 * File writing
 */

static TinValue objmethod_file_write(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TIN_ENSURE_ARGS(vm->state, 1)
    size_t rt;
    TinString* value;
    value = tin_value_tostring(vm->state, argv[0]);
    rt = fwrite(value->chars, tin_string_getlength(value), 1, ((TinFileData*)tin_util_instancedataget(vm, instance))->handle);
    return tin_value_makefixednumber(vm->state, rt);
}

static TinValue objmethod_file_writebyte(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    uint8_t rt;
    uint8_t byte;
    byte = (uint8_t)tin_value_checknumber(vm, argv, argc, 0);
    rt = tin_ioutil_writeuint8(((TinFileData*)tin_util_instancedataget(vm, instance))->handle, byte);
    return tin_value_makefixednumber(vm->state, rt);
}

static TinValue objmethod_file_writeshort(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    uint16_t rt;
    uint16_t shrt;
    shrt = (uint16_t)tin_value_checknumber(vm, argv, argc, 0);
    rt = tin_ioutil_writeuint16(((TinFileData*)tin_util_instancedataget(vm, instance))->handle, shrt);
    return tin_value_makefixednumber(vm->state, rt);
}

static TinValue objmethod_file_writenumber(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    uint32_t rt;
    float num;
    num = (float)tin_value_checknumber(vm, argv, argc, 0);
    rt = tin_ioutil_writeuint32(((TinFileData*)tin_util_instancedataget(vm, instance))->handle, num);
    return tin_value_makefixednumber(vm->state, rt);
}

static TinValue objmethod_file_writebool(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    bool value;
    uint8_t rt;
    value = tin_value_checkbool(vm, argv, argc, 0);
    rt = tin_ioutil_writeuint8(((TinFileData*)tin_util_instancedataget(vm, instance))->handle, (uint8_t)value ? '1' : '0');
    return tin_value_makefixednumber(vm->state, rt);
}

static TinValue objmethod_file_writestring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinString* string;
    TinFileData* data;
    if(tin_value_checkstring(vm, argv, argc, 0) == NULL)
    {
        return NULL_VALUE;
    }
    string = tin_value_asstring(argv[0]);
    data = (TinFileData*)tin_util_instancedataget(vm, instance);
    tin_ioutil_writestring(data->handle, string);
    return NULL_VALUE;
}

/*
* ==
* File reading
*/

static long tin_util_filesize(FILE* fh)
{
    int fd;
    struct stat sb;
    fd = fileno(fh);
    if(fd == -1)
    {
        return -1;
    }
    if(fstat(fd, &sb) == -1)
    {
        return -1;
    }
    return sb.st_size;
}

static TinValue objmethod_file_readall(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    char c;
    long filelen;
    long actuallen;
    TinFileData* data;
    TinString* result;
    data = (TinFileData*)tin_util_instancedataget(vm, instance);
    if(fseek(data->handle, 0, SEEK_END) == -1)
    {
        /*
        * cannot seek, must read each byte.
        */
        result = tin_string_makeempty(vm->state, 0, false);
        actuallen = 0;
        while((c = fgetc(data->handle)) != EOF)
        {
            result->chars = sdscatlen(result->chars, &c, 1);
            actuallen++;
        }
    }
    else
    {
        filelen = ftell(data->handle);
        fseek(data->handle, 0, SEEK_SET);
        result = tin_string_makeempty(vm->state, filelen, false);
        actuallen = fread(result->chars, sizeof(char), filelen, data->handle);
        /*
        * after reading, THIS actually sets the correct length.
        * before that, it would be 0.
        */
        sdsIncrLen(result->chars, actuallen);
    }
    result->hash = tin_util_hashstring(result->chars, actuallen);
    tin_state_regstring(vm->state, result);
    return tin_value_fromobject(result);
}

static TinValue objmethod_file_readamount(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)instance;
    (void)argc;
    (void)argv;
    long wantlen;
    long storelen;
    long filelen;
    long actuallen;
    TinFileData* data;
    TinString* result;
    /* if no arguments given, just forward to readAll() */
    if(argc == 0)
    {
        return objmethod_file_readall(vm, instance, argc, argv);
    }
    data = (TinFileData*)tin_util_instancedataget(vm, instance);
    wantlen = tin_value_checknumber(vm, argv, argc, 0);
    /* storelen is the amount that is ultimately allocated */
    storelen = wantlen;
    /* try to get filesize (WITHOUT seeking, as that could deadlock) */
    filelen = tin_util_filesize(data->handle);
    /* if that succeeded, try to calculate size to avoid allocating too much  ... */
    if(filelen > 0)
    {
        /* if wanted amount is more than is actually available .... */
        if(wantlen > filelen)
        {
            /* re-calculate the amount to fit how much is actually available */
            storelen = filelen % wantlen;
            if(storelen == 0)
            {
                storelen = wantlen;
            }
        }
    }
    /* make room for NUL. this will still be overriden by fread() */
    storelen = storelen + 1;
    result = tin_string_makeempty(vm->state, storelen + 1, false);
    actuallen = fread(result->chars, sizeof(char), storelen, data->handle);
    /* if that didn't work, it's an error */
    if(actuallen == 0)
    {
        tin_state_raiseerror(vm->state, RUNTIME_ERROR, "fread failed");
        return NULL_VALUE;
    }
    /* important: until sdsIncrLen is called, the string is zero-length. */
    sdsIncrLen(result->chars, actuallen);
    /* insert string into the gc matrix. */
    result->hash = tin_util_hashstring(result->chars, actuallen);
    tin_state_regstring(vm->state, result);
    return tin_value_fromobject(result);
}

static TinValue objmethod_file_readline(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    size_t maxlength;
    char* line;
    TinFileData* data;
    maxlength = (size_t)tin_value_getnumber(vm, argv, argc, 0, 128);
    data = (TinFileData*)tin_util_instancedataget(vm, instance);
    line = TIN_ALLOCATE(vm->state, sizeof(char), maxlength + 1);
    if(!fgets(line, maxlength, data->handle))
    {
        TIN_FREE(vm->state, sizeof(char), line);
        return NULL_VALUE;
    }
    return tin_value_fromobject(tin_string_take(vm->state, line, strlen(line) - 1, false));
}

static TinValue objmethod_file_readbyte(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return tin_value_makefixednumber(vm->state, tin_ioutil_readuint8(((TinFileData*)tin_util_instancedataget(vm, instance))->handle));
}

static TinValue objmethod_file_readshort(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return tin_value_makefixednumber(vm->state, tin_ioutil_readuint16(((TinFileData*)tin_util_instancedataget(vm, instance))->handle));
}

static TinValue objmethod_file_readnumber(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return tin_value_makefixednumber(vm->state, tin_ioutil_readuint32(((TinFileData*)tin_util_instancedataget(vm, instance))->handle));
}

static TinValue objmethod_file_readbool(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return tin_value_makebool(vm->state, (char)tin_ioutil_readuint8(((TinFileData*)tin_util_instancedataget(vm, instance))->handle) == '1');
}

static TinValue objmethod_file_readstring(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    TinFileData* data = (TinFileData*)tin_util_instancedataget(vm, instance);
    TinString* string = tin_ioutil_readstring(vm->state, data->handle);

    return string == NULL ? NULL_VALUE : tin_value_fromobject(string);
}

static TinValue objmethod_file_getlastmodified(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    struct stat buffer;
    char* filename = NULL;
    (void)vm;
    (void)argc;
    (void)argv;
    if(tin_value_isinstance(instance))
    {
        filename = ((TinFileData*)tin_util_instancedataget(vm, instance))->path;
    }
    else
    {
        filename = (char*)tin_value_checkstring(vm, argv, argc, 0);
    }

    if(stat(filename, &buffer) != 0)
    {
        return tin_value_makefixednumber(vm->state, 0);
    }
    #if defined(__unix__) || defined(__linux__)
        return tin_value_makefixednumber(vm->state, buffer.st_mtim.tv_sec);
    #else
        return tin_value_makefixednumber(vm->state, 0);
    #endif
}


/*
* Directory
*/

static TinValue objfunction_directory_exists(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    const char* directoryname = tin_value_checkstring(vm, argv, argc, 0);
    struct stat buffer;
    (void)vm;
    (void)instance;
    (void)argc;
    (void)argv;
    return tin_value_makebool(vm->state, stat(directoryname, &buffer) == 0 && S_ISDIR(buffer.st_mode));
}

static TinValue objfunction_directory_listfiles(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinState* state;
    TinArray* array;
    (void)instance;
    state = vm->state;
    array = tin_create_array(state);
    #if defined(__unix__) || defined(__linux__)
    {
        struct dirent* ep;
        DIR* dir = opendir(tin_value_checkstring(vm, argv, argc, 0));
        if(dir == NULL)
        {
            return tin_value_fromobject(array);
        }
        while((ep = readdir(dir)))
        {
            if(ep->d_type == DT_REG)
            {
                tin_vallist_push(state, &array->list, tin_value_makestring(state, ep->d_name));
            }
        }
        closedir(dir);
    }
    #endif
    return tin_value_fromobject(array);
}

static TinValue objfunction_directory_listdirs(TinVM* vm, TinValue instance, size_t argc, TinValue* argv)
{
    TinArray* array;
    TinState* state;
    TinDirReader rd;
    TinDirItem ent;
    (void)instance;
    state = vm->state;
    array = tin_create_array(state);

    if(tin_fs_diropen(&rd, tin_value_checkstring(vm, argv, argc, 0)))
    {
        while(true)
        {
            if(tin_fs_dirread(&rd, &ent))
            {
                if(ent.isdir && ((strcmp(ent.name, ".") != 0) || (strcmp(ent.name, "..") != 0)))
                {
                    tin_vallist_push(state, &array->list, tin_value_makestring(state, ent.name));
                }
            }
            else
            {
                break;
            }
        }
        tin_fs_dirclose(&rd);
    }
    return tin_value_fromobject(array);
}

static void tin_userfile_destroyhandle(TinState* state, TinUserdata* userdata, bool mark)
{
    TinStdioHandle* hstd;
    (void)mark;
    hstd = (TinStdioHandle*)(userdata->data);
    TIN_FREE(state, sizeof(TinStdioHandle), hstd);
}

static void tin_userfile_makehandle(TinState* state, TinValue fileval, const char* name, FILE* hnd, bool canread, bool canwrite)
{
    TinUserdata* userhnd;
    TinValue args[5];
    TinString* varname;
    TinString* descname;
    TinInterpretResult res;
    TinFiber* oldfiber;
    TinStdioHandle* hstd;
    oldfiber = state->vm->fiber;
    state->vm->fiber = tin_create_fiber(state, state->last_module, NULL);
    {
        hstd = TIN_ALLOCATE(state, sizeof(TinStdioHandle), 1);
        hstd->handle = hnd;
        hstd->name = name;
        hstd->canread = canread;
        hstd->canwrite = canwrite; 
        userhnd = tin_object_makeuserdata(state, sizeof(TinStdioHandle), true);
        userhnd->data = hstd;
        userhnd->canfree = false;
        userhnd->cleanup_fn = tin_userfile_destroyhandle;
        varname = tin_string_copyconst(state, name);
        descname = tin_string_copyconst(state, name);
        args[0] = tin_value_fromobject(userhnd);
        args[1] = tin_value_fromobject(descname);
        res = tin_state_callvalue(state, fileval, args, 2, false);
        //fprintf(stderr, "tin_userfile_makehandle(%s, hnd=%p): res.type=%d, res.result=%s\n", name, hnd, res.type, tin_tostring_typename(res.result));
        tin_state_setglobal(state, varname, res.result);
    }
    state->vm->fiber = oldfiber;
}

static void tin_userfile_makestdhandles(TinState* state)
{
    TinValue fileval;
    fileval = tin_state_getglobalvalue(state, tin_string_copyconst(state, "File"));
    fprintf(stderr, "fileval=%s\n", tin_tostring_typename(fileval));
    {
        tin_userfile_makehandle(state, fileval, "STDIN", stdin, true, false);
        tin_userfile_makehandle(state, fileval, "STDOUT", stdout, false, true);
        tin_userfile_makehandle(state, fileval, "STDERR", stderr, false, true);
    }
}

void tin_open_file_library(TinState* state)
{
    TinClass* klass;
    {
        klass = tin_create_classobject(state, "File");
        {
            tin_class_bindstaticmethod(state, klass, "exists", objmethod_file_exists);
            tin_class_bindstaticmethod(state, klass, "getLastModified", objmethod_file_getlastmodified);
            tin_class_bindconstructor(state, klass, objmethod_file_constructor);
            tin_class_bindmethod(state, klass, "close", objmethod_file_close);
            tin_class_bindmethod(state, klass, "write", objmethod_file_write);
            tin_class_bindmethod(state, klass, "writeByte", objmethod_file_writebyte);
            tin_class_bindmethod(state, klass, "writeShort", objmethod_file_writeshort);
            tin_class_bindmethod(state, klass, "writeNumber", objmethod_file_writenumber);
            tin_class_bindmethod(state, klass, "writeBool", objmethod_file_writebool);
            tin_class_bindmethod(state, klass, "writeString", objmethod_file_writestring);
            tin_class_bindmethod(state, klass, "read", objmethod_file_readamount);
            tin_class_bindmethod(state, klass, "readAll", objmethod_file_readall);
            tin_class_bindmethod(state, klass, "readLine", objmethod_file_readline);
            tin_class_bindmethod(state, klass, "readByte", objmethod_file_readbyte);
            tin_class_bindmethod(state, klass, "readShort", objmethod_file_readshort);
            tin_class_bindmethod(state, klass, "readNumber", objmethod_file_readnumber);
            tin_class_bindmethod(state, klass, "readBool", objmethod_file_readbool);
            tin_class_bindmethod(state, klass, "readString", objmethod_file_readstring);

            tin_class_bindmethod(state, klass, "getLastModified", objmethod_file_getlastmodified);
            tin_class_bindgetset(state, klass, "exists", objmethod_file_exists, NULL, false);
        }
        tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
        if(klass->super == NULL)
        {
            tin_class_inheritfrom(state, klass, state->primobjectclass);
        };
    }
    {
        klass = tin_create_classobject(state, "Directory");
        {
            tin_class_bindstaticmethod(state, klass, "exists", objfunction_directory_exists);
            tin_class_bindstaticmethod(state, klass, "listFiles", objfunction_directory_listfiles);
            tin_class_bindstaticmethod(state, klass, "listDirectories", objfunction_directory_listdirs);
        }
        tin_state_setglobal(state, klass->name, tin_value_fromobject(klass));
        if(klass->super == NULL)
        {
            tin_class_inheritfrom(state, klass, state->primobjectclass);
        };
    }
    tin_userfile_makestdhandles(state);
}


