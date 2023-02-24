
#include <stdarg.h>
#include <stdio.h>
#include "priv.h"
#include "sds.h"

static void litwr_cb_writebyte(TinWriter* wr, int byte)
{
    TinString* ds;
    if(wr->stringmode)
    {
        ds = (TinString*)wr->uptr;
        tin_string_appendchar(ds, byte);        
    }
    else
    {
        fputc(byte, (FILE*)wr->uptr);
    }
}

static void litwr_cb_writestring(TinWriter* wr, const char* string, size_t len)
{
    TinString* ds;
    if(wr->stringmode)
    {
        ds = (TinString*)wr->uptr;
        tin_string_appendlen(ds, string, len);
    }
    else
    {
        fwrite(string, sizeof(char), len, (FILE*)wr->uptr);
    }
}

static void litwr_cb_writeformat(TinWriter* wr, const char* fmt, va_list va)
{
    TinString* ds;
    if(wr->stringmode)
    {
        ds = (TinString*)wr->uptr;
        ds->chars = sdscatvprintf(ds->chars, fmt, va);
    }
    else
    {
        vfprintf((FILE*)wr->uptr, fmt, va);
    }
}

static void tin_writer_init_default(TinState* state, TinWriter* wr)
{
    wr->state = state;
    wr->forceflush = false;
    wr->stringmode = false;
    wr->fnbyte = litwr_cb_writebyte;
    wr->fnstring = litwr_cb_writestring;
    wr->fnformat = litwr_cb_writeformat;
}

void tin_writer_init_file(TinState* state, TinWriter* wr, FILE* fh, bool forceflush)
{
    tin_writer_init_default(state, wr);
    wr->uptr = fh;
    wr->forceflush = forceflush;
}

void tin_writer_init_string(TinState* state, TinWriter* wr)
{
    tin_writer_init_default(state, wr);
    wr->stringmode = true;
    wr->uptr = tin_string_makeempty(state, 0, false);
}

void tin_writer_writebyte(TinWriter* wr, int byte)
{
    wr->fnbyte(wr, byte);
}

void tin_writer_writestringl(TinWriter* wr, const char* str, size_t len)
{
    wr->fnstring(wr, str, len);
}

void tin_writer_writestring(TinWriter* wr, const char* str)
{
    wr->fnstring(wr, str, strlen(str));
}

void tin_writer_writeformat(TinWriter* wr, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    wr->fnformat(wr, fmt, va);
    va_end(va);
}


void tin_writer_writeescapedbyte(TinWriter* wr, int ch)
{
    switch(ch)
    {
        case '\'':
            {
                tin_writer_writestring(wr, "\\\'");
            }
            break;
        case '\"':
            {
                tin_writer_writestring(wr, "\\\"");
            }
            break;
        case '\\':
            {
                tin_writer_writestring(wr, "\\\\");
            }
            break;
        case '\b':
            {
                tin_writer_writestring(wr, "\\b");
            }
            break;
        case '\f':
            {
                tin_writer_writestring(wr, "\\f");
            }
            break;
        case '\n':
            {
                tin_writer_writestring(wr, "\\n");
            }
            break;
        case '\r':
            {
                tin_writer_writestring(wr, "\\r");
            }
            break;
        case '\t':
            {
                tin_writer_writestring(wr, "\\t");
            }
            break;
        default:
            {
                tin_writer_writeformat(wr, "\\x%02x", (unsigned char)ch);
            }
            break;
    }
}

void tin_writer_writeescapedstring(TinWriter* wr, const char* str, size_t len, bool withquot)
{
    size_t i;
    int bch;
    int quotch;
    quotch = '"';
    if(withquot)
    {
        tin_writer_writebyte(wr, quotch);
        for(i=0; i<len; i++)
        {
            bch = str[i];
            if((bch < 32) || (bch > 127) || (bch == '\"') || (bch == '\\'))
            {
                tin_writer_writeescapedbyte(wr, bch);
            }
            else
            {
                tin_writer_writebyte(wr, bch);
            }
        }
        tin_writer_writebyte(wr, quotch);
    }
    else
    {
        tin_writer_writestringl(wr, str, len);
    }
}

TinString* tin_writer_get_string(TinWriter* wr)
{
    if(wr->stringmode)
    {
        return (TinString*)wr->uptr;
    }
    return NULL;
}

static const char* tin_object_type_names[] =
{
    "string",
    "function",
    "nativefunction",
    "nativeprimitive",
    "nativemethod",
    "primitivemethod",
    "fiber",
    "module",
    "closure",
    "upvalue",
    "class",
    "instance",
    "boundmethod",
    "array",
    "map",
    "userdata",
    "range",
    "field",
    "reference"
};

void tin_towriter_array(TinState* state, TinWriter* wr, TinArray* array, size_t size)
{
    size_t i;
    tin_writer_writeformat(wr, "(%u) [", (unsigned int)size);
    if(size > 0)
    {
        tin_writer_writestring(wr, " ");
        for(i = 0; i < size; i++)
        {
            if(tin_value_isarray(tin_vallist_get(&array->list, i)) && (array == tin_value_asarray(tin_vallist_get(&array->list,i))))
            {
                tin_writer_writestring(wr, "(recursion)");
            }
            else
            {
                tin_towriter_value(state, wr, tin_vallist_get(&array->list, i), true);
            }
            if(i + 1 < size)
            {
                tin_writer_writestring(wr, ", ");
            }
            else
            {
                tin_writer_writestring(wr, " ");
            }
        }
    }
    tin_writer_writestring(wr, "]");
}

void tin_towriter_map(TinState* state, TinWriter* wr, TinMap* map, size_t size)
{
    bool hadbefore;
    size_t i;
    TinTableEntry* entry;
    tin_writer_writeformat(wr, "(%u) {", (unsigned int)size);
    hadbefore = false;
    if(size > 0)
    {
        for(i = 0; i < (size_t)map->values.capacity; i++)
        {
            entry = &map->values.entries[i];
            if(entry->key != NULL)
            {
                if(hadbefore)
                {
                    tin_writer_writestring(wr, ", ");
                }
                else
                {
                    tin_writer_writestring(wr, " ");
                }
                tin_writer_writeescapedstring(wr, entry->key->chars, tin_string_getlength(entry->key), true);
                tin_writer_writestring(wr, ": ");
                if(tin_value_ismap(entry->value) && (map == tin_value_asmap(entry->value)))
                {
                    tin_writer_writestring(wr, "(recursion)");
                }
                else
                {
                    tin_towriter_value(state, wr, entry->value, true);
                }
                hadbefore = true;
            }
        }
    }
    if(hadbefore)
    {
        tin_writer_writestring(wr, " }");
    }
    else
    {
        tin_writer_writestring(wr, "}");
    }
}

void tin_towriter_functail(TinState* state, TinWriter* wr, TinString* name, TinModule* mod, const char* suffix)
{
    (void)state;
    (void)mod;
    tin_writer_writestring(wr, "<function ");
    tin_writer_writeescapedstring(wr, name->chars, tin_string_getlength(name), true);
    tin_writer_writeformat(wr, " %s>", suffix);
}


void tin_towriter_object(TinState* state, TinWriter* wr, TinValue value, bool withquot)
{
    size_t size;
    TinMap* map;
    TinArray* array;
    TinRange* range;
    TinValue* slot;
    TinObject* obj;
    TinUpvalue* upvalue;
    obj = tin_value_asobject(value);
    if(obj != NULL)
    {
        switch(obj->type)
        {
            case TINTYPE_STRING:
                {
                    TinString* s;
                    s = tin_value_asstring(value);
                    tin_writer_writeescapedstring(wr, s->chars, tin_string_getlength(s), withquot);
                }
                break;
            case TINTYPE_FUNCTION:
                {
                    TinFunction* fn;
                    fn = tin_value_asfunction(value);
                    tin_towriter_functail(state, wr, fn->name, fn->module, "script");
                }
                break;
            case TINTYPE_CLOSURE:
                {
                    tin_writer_writeformat(wr, "<closure %s>", tin_value_asclosure(value)->function->name->chars);
                }
                break;
            case TINTYPE_NATIVE_PRIMITIVE:
                {
                    TinNativePrimFunction* fn;
                    fn = tin_value_asnativeprimitive(value);
                    tin_towriter_functail(state, wr, fn->name, NULL, "natprimitive");
                }
                break;
            case TINTYPE_NATIVE_FUNCTION:
                {
                    TinNativeFunction* fn;
                    fn = tin_value_asnativefunction(value);
                    tin_towriter_functail(state, wr, fn->name, NULL, "native");
                }
                break;
            case TINTYPE_PRIMITIVE_METHOD:
                {
                    TinPrimitiveMethod* fn;
                    fn = tin_value_asprimitivemethod(value);
                    tin_towriter_functail(state, wr, fn->name, NULL, "primmethod");
                }
                break;
            case TINTYPE_NATIVE_METHOD:
                {
                    TinNativeMethod* fn;
                    fn = tin_value_asnativemethod(value);
                    tin_towriter_functail(state, wr, fn->name, NULL, "natmethod");
                }
                break;
            case TINTYPE_FIBER:
                {
                    tin_writer_writeformat(wr, "<fiber>");
                }
                break;
            case TINTYPE_MODULE:
                {
                    TinModule* mod;
                    mod = tin_value_asmodule(value);
                    tin_writer_writestring(wr, "<module ");
                    tin_writer_writeescapedstring(wr, mod->name->chars, tin_string_getlength(mod->name), true);
                    tin_writer_writestring(wr, ">");
                }
                break;

            case TINTYPE_UPVALUE:
                {
                    upvalue = tin_value_asupvalue(value);
                    if(upvalue->location == NULL)
                    {
                        tin_towriter_value(state, wr, upvalue->closed, withquot);
                    }
                    else
                    {
                        tin_towriter_object(state, wr, *upvalue->location, withquot);
                    }
                }
                break;
            case TINTYPE_CLASS:
                {
                    TinClass* klass;
                    klass = tin_value_asclass(value);
                    tin_writer_writeformat(wr, "<class ");
                    tin_writer_writeescapedstring(wr, klass->name->chars, tin_string_getlength(klass->name), true);
                    tin_writer_writestring(wr, ">");
                }
                break;
            case TINTYPE_INSTANCE:
                {
                    TinInstance* inst;
                    inst = tin_value_asinstance(value);
                    tin_writer_writestring(wr, "<instance ");
                    tin_writer_writeescapedstring(wr, inst->klass->name->chars, tin_string_getlength(inst->klass->name), true);
                    tin_writer_writestring(wr, ">");
                }
                break;
            case TINTYPE_BOUND_METHOD:
                {
                    tin_towriter_value(state, wr, tin_value_asboundmethod(value)->method, withquot);
                    return;
                }
                break;
            case TINTYPE_ARRAY:
                {
                    #ifdef TIN_MINIMIZE_CONTAINERS
                        tin_writer_writestring(wr, "<array>");
                    #else
                        array = tin_value_asarray(value);
                        size = tin_vallist_count(&array->list);
                        tin_towriter_array(state, wr, array, size);
                    #endif
                }
                break;
            case TINTYPE_MAP:
                {
                    #ifdef TIN_MINIMIZE_CONTAINERS
                        tin_writer_writeformat(wr, "<map>");
                    #else
                        map = tin_value_asmap(value);
                        size = map->values.count;
                        tin_towriter_map(state, wr, map, size);
                    #endif
                }
                break;
            case TINTYPE_USERDATA:
                {
                    tin_writer_writeformat(wr, "<userdata>");
                }
                break;
            case TINTYPE_RANGE:
                {
                    range = tin_value_asrange(value);
                    tin_writer_writeformat(wr, "<range %g .. %g>", range->from, range->to);
                }
                break;
            case TINTYPE_FIELD:
                {
                    tin_writer_writeformat(wr, "<field>");
                }
                break;
            case TINTYPE_REFERENCE:
                {
                    tin_writer_writeformat(wr, "<reference => ");
                    slot = tin_value_asreference(value)->slot;
                    if(slot == NULL)
                    {
                        tin_writer_writestring(wr, "null");
                    }
                    else
                    {
                        tin_towriter_value(state, wr, *slot, withquot);
                    }
                    tin_writer_writestring(wr, ">");
                }
                break;
            default:
                {
                }
                break;
        }
    }
    else
    {
        tin_writer_writestring(wr, "!nullpointer!");
    }
}

void tin_towriter_value(TinState* state, TinWriter* wr, TinValue value, bool withquot)
{
    if(tin_value_isbool(value))
    {
        tin_writer_writestring(wr, tin_value_asbool(value) ? "true" : "false");
    }
    else if(tin_value_isnull(value))
    {
        tin_writer_writestring(wr, "null");
    }
    else if(tin_value_isnumber(value))
    {
        if(value.isfixednumber)
        {
            tin_writer_writeformat(wr, "%ld", tin_value_asfixednumber(value));
        }
        else
        {
            tin_writer_writeformat(wr, "%g", tin_value_asfloatnumber(value));
        }
    }
    else if(tin_value_isobject(value))
    {
        tin_towriter_object(state, wr, value, withquot);
    }
}

const char* tin_tostring_typename(TinValue value)
{
    if(tin_value_isbool(value))
    {
        return "bool";
    }
    else if(tin_value_isnull(value))
    {
        return "null";
    }
    else if(tin_value_isnumber(value))
    {
        return "number";
    }
    else if(tin_value_isobject(value))
    {
        return tin_object_type_names[tin_value_type(value)];
    }
    return "unknown";
}

const char* tin_tostring_exprtype(TinAstExprType t)
{
    switch(t)
    {
        case TINEXPR_LITERAL: return "TINERAL";
        case TINEXPR_BINARY: return "BINARY";
        case TINEXPR_UNARY: return "UNARY";
        case TINEXPR_VAREXPR: return "VAREXPR";
        case TINEXPR_ASSIGN: return "ASSIGN";
        case TINEXPR_CALL: return "CALL";
        case TINEXPR_SET: return "SET";
        case TINEXPR_GET: return "GET";
        case TINEXPR_LAMBDA: return "LAMBDA";
        case TINEXPR_ARRAY: return "ARRAY";
        case TINEXPR_OBJECT: return "OBJECT";
        case TINEXPR_SUBSCRIPT: return "SUBSCRIPT";
        case TINEXPR_THIS: return "THIS";
        case TINEXPR_SUPER: return "SUPER";
        case TINEXPR_RANGE: return "RANGE";
        case TINEXPR_TERNARY: return "TERNARY";
        case TINEXPR_INTERPOLATION: return "INTERPOLATION";
        case TINEXPR_REFERENCE: return "REFERENCE";
        case TINEXPR_EXPRESSION: return "EXPRESSION";
        case TINEXPR_BLOCK: return "BLOCK";
        case TINEXPR_IFSTMT: return "IFSTMT";
        case TINEXPR_WHILE: return "WHILE";
        case TINEXPR_FOR: return "FOR";
        case TINEXPR_VARSTMT: return "VARSTMT";
        case TINEXPR_CONTINUE: return "CONTINUE";
        case TINEXPR_BREAK: return "BREAK";
        case TINEXPR_FUNCTION: return "FUNCTION";
        case TINEXPR_RETURN: return "RETURN";
        case TINEXPR_METHOD: return "METHOD";
        case TINEXPR_CLASS: return "CLASS";
        case TINEXPR_FIELD: return "FIELD";
        default:
            break;
    }
    return "unknown";
}

const char* tin_tostring_optok(TinAstTokType t)
{
    switch(t)
    {
        case TINTOK_NEW_LINE: return "NEW_LINE";
        case TINTOK_LEFT_PAREN: return "(";
        case TINTOK_RIGHT_PAREN: return ")";
        case TINTOK_LEFT_BRACE: return "{";
        case TINTOK_RIGHT_BRACE: return "}";
        case TINTOK_LEFT_BRACKET: return "[";
        case TINTOK_RIGHT_BRACKET: return "]";
        case TINTOK_COMMA: return ",";
        case TINTOK_SEMICOLON: return ";";
        case TINTOK_COLON: return ":";
        case TINTOK_BAR_EQUAL: return "|=";
        case TINTOK_BAR: return "|";
        case TINTOK_BAR_BAR: return "||";
        case TINTOK_AMPERSAND_EQUAL: return "&=";
        case TINTOK_AMPERSAND: return "&";
        case TINTOK_AMPERSAND_AMPERSAND: return "&&";
        case TINTOK_BANG: return "!";
        case TINTOK_BANG_EQUAL: return "!=";
        case TINTOK_EQUAL: return "=";
        case TINTOK_EQUAL_EQUAL: return "==";
        case TINTOK_GREATER: return ">";
        case TINTOK_GREATER_EQUAL: return ">=";
        case TINTOK_GREATER_GREATER: return ">>";
        case TINTOK_LESS: return "<";
        case TINTOK_LESS_EQUAL: return "<=";
        case TINTOK_LESS_LESS: return "<<";
        case TINTOK_PLUS: return "+";
        case TINTOK_PLUS_EQUAL: return "+=";
        case TINTOK_PLUS_PLUS: return "++";
        case TINTOK_MINUS: return "-";
        case TINTOK_MINUS_EQUAL: return "-=";
        case TINTOK_MINUS_MINUS: return "--";
        case TINTOK_STAR: return "*";
        case TINTOK_STAR_EQUAL: return "*=";
        case TINTOK_STAR_STAR: return "**";
        case TINTOK_SLASH: return "/";
        case TINTOK_SLASH_EQUAL: return "/=";
        case TINTOK_QUESTION: return "?";
        case TINTOK_QUESTION_QUESTION: return "??";
        case TINTOK_PERCENT: return "%";
        case TINTOK_PERCENT_EQUAL: return "%=";
        case TINTOK_ARROW: return "=>";
        case TINTOK_SMALL_ARROW: return "->";
        case TINTOK_TILDE: return "~";
        case TINTOK_CARET: return "^";
        case TINTOK_CARET_EQUAL: return "^=";
        case TINTOK_DOT: return ".";
        case TINTOK_DOT_DOT: return "..";
        case TINTOK_DOT_DOT_DOT: return "...";
        case TINTOK_SHARP: return "#";
        case TINTOK_SHARP_EQUAL: return "#=";
        case TINTOK_IDENTIFIER: return "IDENTIFIER";
        case TINTOK_STRING: return "STRING";
        case TINTOK_INTERPOLATION: return "INTERPOLATION";
        case TINTOK_NUMBER: return "NUMBER";
        case TINTOK_CLASS: return "CLASS";
        case TINTOK_ELSE: return "ELSE";
        case TINTOK_FALSE: return "FALSE";
        case TINTOK_FOR: return "FOR";
        case TINTOK_FUNCTION: return "FUNCTION";
        case TINTOK_IF: return "IF";
        case TINTOK_NULL: return "NULL";
        case TINTOK_RETURN: return "RETURN";
        case TINTOK_SUPER: return "SUPER";
        case TINTOK_THIS: return "THIS";
        case TINTOK_TRUE: return "TRUE";
        case TINTOK_VAR: return "VAR";
        case TINTOK_WHILE: return "WHILE";
        case TINTOK_CONTINUE: return "CONTINUE";
        case TINTOK_BREAK: return "BREAK";
        case TINTOK_NEW: return "NEW";
        case TINTOK_EXPORT: return "EXPORT";
        case TINTOK_IS: return "IS";
        case TINTOK_STATIC: return "STATIC";
        case TINTOK_OPERATOR: return "OPERATOR";
        case TINTOK_GET: return "GET";
        case TINTOK_SET: return "SET";
        case TINTOK_IN: return "IN";
        case TINTOK_CONST: return "CONST";
        case TINTOK_REF: return "REF";
        case TINTOK_ERROR: return "ERROR";
        case TINTOK_EOF: return "EOF";
        default:
            break;
    }
    return "unknown";
}

#define as_type(varname, fromname, tname) \
    tname* varname = (tname*)fromname

typedef struct TinAstWriterState TinAstWriterState;

struct TinAstWriterState
{
    int indent;
    TinState* state;
    TinWriter* writer;
};

void tin_astwriter_expr(TinAstWriterState* aw, TinAstExpression* expr);

void tin_astwriter_init(TinState* state, TinAstWriterState* aw, TinWriter* wr)
{
    aw->indent = 0;
    aw->state = state;
    aw->writer = wr;
}

void tin_astwriter_putindent(TinAstWriterState* aw)
{
    int i;
    for(i=0; i<aw->indent; i++)
    {
        tin_writer_writestring(aw->writer, "    ");
    }
}

void tin_astwriter_funcdecl(TinAstWriterState* aw, TinAstExpression* expr, bool islambda)
{
    size_t i;
    TinWriter* wr;
    wr = aw->writer;
    as_type(exfun, expr, TinAstFunctionExpr);
    if(islambda)
    {
        tin_writer_writestring(wr, "function");
    }
    else
    {
        tin_writer_writeformat(wr, "function %.*s", exfun->length, exfun->name);
    }
    tin_writer_writeformat(wr, "(");
    for(i=0; i<exfun->parameters.count; i++)
    {
        tin_writer_writeformat(wr, "%.*s", exfun->parameters.values[i].length, exfun->parameters.values[i].name);
        if(exfun->parameters.values[i].defaultexpr)
        {
            tin_writer_writestring(wr, "=");
            tin_astwriter_expr(aw, exfun->parameters.values[i].defaultexpr);
        }
        if((i+1) < exfun->parameters.count)
        {
            tin_writer_writestring(wr, ", ");
        }
    }
    tin_writer_writeformat(wr, ")");
    tin_astwriter_expr(aw, exfun->body);
}

void tin_astwriter_expr(TinAstWriterState* aw, TinAstExpression* expr)
{
    size_t i;
    TinAstExpression* uex;
    TinWriter* wr;
    TinState* state;
    if(expr == NULL)
    {
        return;
    }
    state = aw->state;
    wr = aw->writer;
    switch(expr->type)
    {
        case TINEXPR_LITERAL:
            {
                as_type(exlit, expr, TinAstLiteralExpr);
                tin_towriter_value(state, wr, exlit->value, true);
            }
            break;
        case TINEXPR_BINARY:
            {
                as_type(exbin, expr, TinAstBinaryExpr);
                if(!exbin->ignore_left)
                {
                    tin_astwriter_expr(aw, exbin->left);
                }
                tin_writer_writestring(wr, tin_tostring_optok(exbin->op));
                tin_astwriter_expr(aw, exbin->right);
            }
            break;
        case TINEXPR_UNARY:
            {
                as_type(exun, expr, TinAstUnaryExpr);
                tin_writer_writestring(wr, tin_tostring_optok(exun->op));
                tin_astwriter_expr(aw, exun->right);
                /*
                if(exun->op == TINTOK_SLASH_SLASH)
                {
                    tin_writer_writestring(wr, "\n");
                }
                */
            }
            break;
        case TINEXPR_VAREXPR:
            {
                as_type(exvarex, expr, TinAstVarExpr);
                tin_writer_writeformat(wr, "%.*s", exvarex->length, exvarex->name);
            }
            break;
        case TINEXPR_ASSIGN:
            {
                as_type(exassign, expr, TinAstAssignExpr);
                tin_astwriter_expr(aw, exassign->to);
                tin_writer_writestring(wr, " = ");
                tin_astwriter_expr(aw, exassign->value);
            }
            break;
        case TINEXPR_CALL:
            {
                as_type(excall, expr, TinAstCallExpr);
                tin_astwriter_expr(aw, excall->callee);
                tin_writer_writestring(wr, "(");
                for(i=0; i<excall->args.count; i++)
                {
                    tin_astwriter_expr(aw, excall->args.values[i]);
                    if((i+1) < excall->args.count)
                    {
                        tin_writer_writestring(wr, ", ");
                    }
                }
                tin_writer_writestring(wr, ")");
            }
            break;
        case TINEXPR_SET:
            {
                as_type(exset, expr, TinAstSetExpr);
                tin_astwriter_expr(aw, exset->where);
                tin_writer_writeformat(wr, ".%.*s = ", exset->length, exset->name);
                tin_astwriter_expr(aw, exset->value);
            }
            break;
        case TINEXPR_GET:
            {
                as_type(exget, expr, TinAstGetExpr);
                tin_astwriter_expr(aw, exget->where);
                tin_writer_writeformat(wr, ".%.*s", exget->length, exget->name);
            }
            break;
        case TINEXPR_LAMBDA:
            {
                tin_astwriter_funcdecl(aw, expr, true);
            }
            break;
        case TINEXPR_ARRAY:
            {
                as_type(exarr, expr, TinAstArrayExpr);
                tin_writer_writeformat(wr, "[");
                for(i=0; i<exarr->values.count; i++)
                {
                    tin_astwriter_expr(aw, exarr->values.values[i]);
                    if((i+1) < exarr->values.count)
                    {
                        tin_writer_writeformat(wr, ", ");
                    }
                }
                tin_writer_writeformat(wr, "]");
            }
            break;
        case TINEXPR_OBJECT:
            {
                as_type(exobj, expr, TinAstObjectExpr);
                tin_writer_writeformat(wr, "{");
                for(i=0; i<tin_vallist_count(&exobj->keys); i++)
                {
                    tin_towriter_value(state, wr, tin_vallist_get(&exobj->keys, i), true);
                    tin_writer_writeformat(wr, ": ");
                    tin_astwriter_expr(aw, exobj->values.values[i]);
                    if((i+1) < tin_vallist_count(&exobj->keys))
                    {
                        tin_writer_writeformat(wr, ", ");
                    }
                }
                tin_writer_writeformat(wr, "}");
            }
            break;
        case TINEXPR_SUBSCRIPT:
            {
                as_type(exsub, expr, TinAstIndexExpr);
                tin_astwriter_expr(aw, exsub->array);
                tin_writer_writestring(wr, "[");
                tin_astwriter_expr(aw, exsub->index);
                tin_writer_writestring(wr, "]");
            }
            break;
        case TINEXPR_THIS:
            {
                tin_writer_writestring(wr, "this");
            }
            break;
        case TINEXPR_SUPER:
            {
                as_type(exsuper, expr, TinAstSuperExpr);
                tin_writer_writeformat(wr, "super(%.*s)", tin_string_getlength(exsuper->method), exsuper->method->chars);
            }
            break;
        case TINEXPR_RANGE:
            {
                as_type(exrange, expr, TinAstRangeExpr);
                tin_writer_writestring(wr, "[");
                tin_astwriter_expr(aw, exrange->from);
                tin_writer_writestring(wr, " .. ");
                tin_astwriter_expr(aw, exrange->to);
                tin_writer_writestring(wr, "]");
            }
            break;
        case TINEXPR_TERNARY:
            {
                as_type(exif, expr, TinAstTernaryExpr);
                tin_writer_writestring(wr, "(");
                tin_astwriter_expr(aw, exif->condition);
                tin_writer_writestring(wr, ") ? (");
                tin_astwriter_expr(aw, exif->ifbranch);
                tin_writer_writestring(wr, ")");
                if(exif->elsebranch != NULL)
                {
                    tin_writer_writestring(wr, " : (");
                    tin_astwriter_expr(aw, exif->elsebranch);
                    tin_writer_writestring(wr, ")");
                }
            }
            break;
        case TINEXPR_INTERPOLATION:
            {
                as_type(exint, expr, TinAstStrInterExpr);
                tin_writer_writestring(wr, "\"\"+");
                for(i=0; i<exint->expressions.count; i++)
                {
                    tin_writer_writestring(wr, "(");
                    tin_astwriter_expr(aw, exint->expressions.values[i]);
                    tin_writer_writestring(wr, ")");
                    if((i+1) < exint->expressions.count)
                    {
                        tin_writer_writestring(wr, "+");
                    }
                }
                tin_writer_writestring(wr, "+\"\"");
            }
            break;
        case TINEXPR_REFERENCE:
            {
            
            }
            break;
        case TINEXPR_EXPRESSION:
            {
                as_type(exexpr, expr, TinAstExprExpr);
                tin_astwriter_expr(aw, exexpr->expression);
            }
            break;
        case TINEXPR_BLOCK:
            {
                as_type(exblock, expr, TinAstBlockExpr);
                tin_astwriter_putindent(aw);
                tin_writer_writestring(wr, "{\n");
                aw->indent++;
                for(i=0; i<exblock->statements.count; i++)
                {
                    uex = exblock->statements.values[i];
                    if(uex != NULL)
                    {
                        //fprintf(stderr, "in block: expression type: %d %s\n", uex->type, tin_tostring_exprtype(uex->type));
                        tin_astwriter_putindent(aw);
                        tin_astwriter_expr(aw, uex);
                        tin_writer_writestring(wr, ";\n");
                    }
                }
                aw->indent--;
                tin_astwriter_putindent(aw);
                tin_writer_writestring(wr, "}");
            }
            break;
        case TINEXPR_IFSTMT:
            {
                as_type(exif, expr, TinAstIfExpr);
                tin_writer_writestring(wr, "if(");
                tin_astwriter_expr(aw, exif->condition);
                tin_writer_writestring(wr, ")\n");
                tin_astwriter_expr(aw, exif->ifbranch);
                if(exif->elseifconds && exif->elseifbranches)
                {
                    for(i=0; i<exif->elseifconds->count; i++)
                    {
                        tin_writer_writestring(wr, "else if(");
                        tin_astwriter_expr(aw, exif->elseifconds->values[i]);
                        tin_writer_writestring(wr, ")\n");
                        tin_astwriter_expr(aw, exif->elseifbranches->values[i]);
                    }
                }
                if(exif->elsebranch)
                {
                    tin_writer_writestring(wr, "else\n");
                    tin_astwriter_expr(aw, exif->elsebranch);
                }
            }
            break;
        case TINEXPR_WHILE:
            {
                as_type(exwhile, expr, TinAstWhileExpr);
                tin_writer_writeformat(wr, "while(");
                tin_astwriter_expr(aw, exwhile->condition);
                tin_writer_writeformat(wr, ")\n");
                tin_astwriter_expr(aw, exwhile->body);
            }
            break;
        case TINEXPR_FOR:
            {
                as_type(exfor, expr, TinAstForExpr);
                tin_writer_writeformat(wr, "for(");
                if(exfor->cstyle)
                {
                    if(exfor->init)
                    {
                        if(exfor->var)
                        {
                            tin_writer_writeformat(wr, "var ");
                            tin_astwriter_expr(aw, exfor->var);
                        }
                        tin_astwriter_expr(aw, exfor->init);
                    }
                    tin_writer_writeformat(wr, ";");
                    if(exfor->condition)
                    {
                        tin_astwriter_expr(aw, exfor->condition);
                    }
                    tin_writer_writeformat(wr, ";");
                    if(exfor->increment)
                    {
                        tin_astwriter_expr(aw, exfor->increment);
                    }
                }
                else
                {
                    
                }
                tin_writer_writeformat(wr, ")\n");
                tin_astwriter_expr(aw, exfor->body);

            }
            break;
        case TINEXPR_VARSTMT:
            {
                as_type(exvdec, expr, TinAstAssignVarExpr);
                if(exvdec->constant)
                {
                    tin_writer_writeformat(wr, "const");
                }
                else
                {
                    tin_writer_writeformat(wr, "var");
                }
                tin_writer_writeformat(wr, " %.*s", exvdec->length, exvdec->name);
                if(exvdec->init)
                {
                    tin_writer_writeformat(wr, " = ");
                    tin_astwriter_expr(aw, exvdec->init);
                }
            }
            break;
        case TINEXPR_CONTINUE:
            {
                tin_writer_writeformat(wr, "continue");
            }
            break;
        case TINEXPR_BREAK:
            {
                tin_writer_writeformat(wr, "break");
            }
            break;
        case TINEXPR_FUNCTION:
            {
                tin_astwriter_funcdecl(aw, expr, false);
            }
            break;
        case TINEXPR_RETURN:
            {
                as_type(exret, expr, TinAstReturnExpr);
                tin_writer_writestring(wr, "return ");
                tin_astwriter_expr(aw, exret->expression);

            }
            break;
        /*case TINEXPR_METHOD:
            {
            
            }
            break;
        case TINEXPR_CLASS:
            {
            
            }
            break;
        case TINEXPR_FIELD:
            {
            
            }
            break;
            */
        default:
            {
                tin_writer_writeformat(wr, "(unhandled expression type %d %s)", expr->type, tin_tostring_exprtype(expr->type));
            }
            break;
    }
}

void tin_towriter_ast(TinState* state, TinWriter* wr, TinAstExprList* exlist)
{
    size_t i;
    TinAstWriterState aw;
    tin_astwriter_init(state, &aw, wr);
    tin_writer_writeformat(wr, "/* begin AST dump (list of %d expressions): */\n", exlist->count);
    for(i=0; i<exlist->count; i++)
    {
        tin_astwriter_expr(&aw, exlist->values[i]);
    }
    tin_writer_writeformat(wr, "\n/* end AST dump */\n");
}

