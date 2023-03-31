
#include "priv.h"


#define TIN_CCAST_GROWCAPACITY(cap) \
    (((cap) < 8) ? (8) : ((cap) * 2))


void tin_exprlist_init(TinAstExprList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void tin_exprlist_destroy(TinState* state, TinAstExprList* array)
{
    tin_gcmem_freearray(state, sizeof(TinAstExpression*), array->values, array->capacity);
    tin_exprlist_init(array);
}

void tin_exprlist_push(TinState* state, TinAstExprList* array, TinAstExpression* value)
{
    size_t oldcapacity;
    if(array->capacity < array->count + 1)
    {
        oldcapacity = array->capacity;
        array->capacity = TIN_CCAST_GROWCAPACITY(oldcapacity);
        array->values = (TinAstExpression**)tin_gcmem_growarray(state, array->values, sizeof(TinAstExpression*), oldcapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

size_t tin_exprlist_count(TinAstExprList* array)
{
    return array->count;
}


TinAstExpression* tin_exprlist_get(TinAstExprList* array, size_t i)
{
    return array->values[i];
}

void tin_paramlist_init(TinAstParamList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void tin_paramlist_destroy(TinState* state, TinAstParamList* array)
{
    tin_gcmem_freearray(state, sizeof(TinAstParameter), array->values, array->capacity);
    tin_paramlist_init(array);
}

void tin_paramlist_push(TinState* state, TinAstParamList* array, TinAstParameter value)
{
    if(array->capacity < array->count + 1)
    {
        size_t oldcapacity = array->capacity;
        array->capacity = TIN_CCAST_GROWCAPACITY(oldcapacity);
        array->values = (TinAstParameter*)tin_gcmem_growarray(state, array->values, sizeof(TinAstParameter), oldcapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void tin_paramlist_destroyvalues(TinState* state, TinAstParamList* parameters)
{
    size_t i;
    for(i = 0; i < parameters->count; i++)
    {
        tin_ast_destroyexpression(state, parameters->values[i].defaultexpr);
    }
    tin_paramlist_destroy(state, parameters);
}

void tin_ast_destroyexprlist(TinState* state, TinAstExprList* expressions)
{
    size_t i;
    if(expressions == NULL)
    {
        return;
    }
    for(i = 0; i < expressions->count; i++)
    {
        tin_ast_destroyexpression(state, expressions->values[i]);
    }
    tin_exprlist_destroy(state, expressions);
}

void tin_ast_destroystmtlist(TinState* state, TinAstExprList* statements)
{
    size_t i;
    if(statements == NULL)
    {
        return;
    }
    for(i = 0; i < statements->count; i++)
    {
        tin_ast_destroyexpression(state, statements->values[i]);
    }
    tin_exprlist_destroy(state, statements);
}

void tin_ast_destroyexpression(TinState* state, TinAstExpression* expr)
{
    if(expr == NULL)
    {
        return;
    }
    switch(expr->type)
    {
        case TINEXPR_LITERAL:
            {
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstLiteralExpr), 0);
            }
            break;
        case TINEXPR_BINARY:
            {
                TinAstBinaryExpr* herex = (TinAstBinaryExpr*)expr;
                if(!herex->ignore_left)
                {
                    tin_ast_destroyexpression(state, herex->left);
                }
                tin_ast_destroyexpression(state, herex->right);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstBinaryExpr), 0);
            }
            break;

        case TINEXPR_UNARY:
            {
                tin_ast_destroyexpression(state, ((TinAstUnaryExpr*)expr)->right);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstUnaryExpr), 0);
            }
            break;
        case TINEXPR_VAREXPR:
            {
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstVarExpr), 0);
            }
            break;
        case TINEXPR_ASSIGN:
            {
                TinAstAssignExpr* herex = (TinAstAssignExpr*)expr;
                tin_ast_destroyexpression(state, herex->to);
                tin_ast_destroyexpression(state, herex->value);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstAssignExpr), 0);
            }
            break;
        case TINEXPR_CALL:
            {
                TinAstCallExpr* herex = (TinAstCallExpr*)expr;
                tin_ast_destroyexpression(state, herex->callee);
                tin_ast_destroyexpression(state, herex->init);
                tin_ast_destroyexprlist(state, &herex->args);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstCallExpr), 0);
            }
            break;

        case TINEXPR_GET:
        {
            tin_ast_destroyexpression(state, ((TinAstGetExpr*)expr)->where);
            tin_gcmem_memrealloc(state, expr, sizeof(TinAstGetExpr), 0);
            break;
        }

        case TINEXPR_SET:
        {
            TinAstSetExpr* herex = (TinAstSetExpr*)expr;

            tin_ast_destroyexpression(state, herex->where);
            tin_ast_destroyexpression(state, herex->value);

            tin_gcmem_memrealloc(state, expr, sizeof(TinAstSetExpr), 0);
            break;
        }

        case TINEXPR_LAMBDA:
            {
                TinAstFunctionExpr* herex = (TinAstFunctionExpr*)expr;
                tin_paramlist_destroyvalues(state, &herex->parameters);
                tin_ast_destroyexpression(state, herex->body);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstFunctionExpr), 0);
            }
            break;
        case TINEXPR_ARRAY:
            {
                tin_ast_destroyexprlist(state, &((TinAstArrayExpr*)expr)->values);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstArrayExpr), 0);
            }
            break;
        case TINEXPR_OBJECT:
            {
                TinAstObjectExpr* map = (TinAstObjectExpr*)expr;
                tin_ast_destroyexprlist(state, &map->keys);
                tin_ast_destroyexprlist(state, &map->values);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstObjectExpr), 0);
            }
            break;
        case TINEXPR_SUBSCRIPT:
            {
                TinAstIndexExpr* herex = (TinAstIndexExpr*)expr;
                tin_ast_destroyexpression(state, herex->array);
                tin_ast_destroyexpression(state, herex->index);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstIndexExpr), 0);
            }
            break;
        case TINEXPR_THIS:
            {
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstThisExpr), 0);
            }
            break;
        case TINEXPR_SUPER:
            {
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstSuperExpr), 0);
            }
            break;
        case TINEXPR_RANGE:
            {
                TinAstRangeExpr* herex = (TinAstRangeExpr*)expr;
                tin_ast_destroyexpression(state, herex->from);
                tin_ast_destroyexpression(state, herex->to);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstRangeExpr), 0);
            }
            break;
        case TINEXPR_TERNARY:
            {
                TinAstTernaryExpr* herex = (TinAstTernaryExpr*)expr;
                tin_ast_destroyexpression(state, herex->condition);
                tin_ast_destroyexpression(state, herex->ifbranch);
                tin_ast_destroyexpression(state, herex->elsebranch);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstTernaryExpr), 0);
            }
            break;
        case TINEXPR_INTERPOLATION:
            {
                tin_ast_destroyexprlist(state, &((TinAstStrInterExpr*)expr)->expressions);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstStrInterExpr), 0);
            }
            break;
        case TINEXPR_REFERENCE:
            {
                tin_ast_destroyexpression(state, ((TinAstRefExpr*)expr)->to);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstRefExpr), 0);
            }
            break;
        case TINEXPR_EXPRESSION:
            {
                tin_ast_destroyexpression(state, ((TinAstExprExpr*)expr)->expression);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstExprExpr), 0);
            }
            break;
        case TINEXPR_BLOCK:
            {
                tin_ast_destroystmtlist(state, &((TinAstBlockExpr*)expr)->statements);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstBlockExpr), 0);
            }
            break;
        case TINEXPR_VARSTMT:
            {
                tin_ast_destroyexpression(state, ((TinAstAssignVarExpr*)expr)->init);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstAssignVarExpr), 0);
            }
            break;
        case TINEXPR_IFSTMT:
            {
                TinAstIfExpr* stmt = (TinAstIfExpr*)expr;
                tin_ast_destroyexpression(state, stmt->condition);
                tin_ast_destroyexpression(state, stmt->ifbranch);
                tin_ast_destroy_allocdexprlist(state, stmt->elseifconds);
                tin_ast_destry_allocdstmtlist(state, stmt->elseifbranches);
                tin_ast_destroyexpression(state, stmt->elsebranch);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstIfExpr), 0);
            }
            break;
        case TINEXPR_WHILE:
            {
                TinAstWhileExpr* stmt = (TinAstWhileExpr*)expr;
                tin_ast_destroyexpression(state, stmt->condition);
                tin_ast_destroyexpression(state, stmt->body);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstWhileExpr), 0);
            }
            break;
        case TINEXPR_FOR:
            {
                TinAstForExpr* stmt = (TinAstForExpr*)expr;
                tin_ast_destroyexpression(state, stmt->increment);
                tin_ast_destroyexpression(state, stmt->condition);
                tin_ast_destroyexpression(state, stmt->init);

                tin_ast_destroyexpression(state, stmt->var);
                tin_ast_destroyexpression(state, stmt->body);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstForExpr), 0);
            }
            break;
        case TINEXPR_CONTINUE:
            {
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstContinueExpr), 0);
            }
            break;
        case TINEXPR_BREAK:
            {
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstBreakExpr), 0);
            }
            break;
        case TINEXPR_FUNCTION:
            {
                TinAstFunctionExpr* stmt = (TinAstFunctionExpr*)expr;
                tin_ast_destroyexpression(state, stmt->body);
                tin_paramlist_destroyvalues(state, &stmt->parameters);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstFunctionExpr), 0);
            }
            break;
        case TINEXPR_RETURN:
            {
                tin_ast_destroyexpression(state, ((TinAstReturnExpr*)expr)->expression);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstReturnExpr), 0);
            }
            break;
        case TINEXPR_METHOD:
            {
                TinAstMethodExpr* stmt = (TinAstMethodExpr*)expr;
                tin_paramlist_destroyvalues(state, &stmt->parameters);
                tin_ast_destroyexpression(state, stmt->body);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstMethodExpr), 0);
            }
            break;
        case TINEXPR_CLASS:
            {
                tin_ast_destroystmtlist(state, &((TinAstClassExpr*)expr)->fields);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstClassExpr), 0);
            }
            break;
        case TINEXPR_FIELD:
            {
                TinAstFieldExpr* stmt = (TinAstFieldExpr*)expr;
                tin_ast_destroyexpression(state, stmt->getter);
                tin_ast_destroyexpression(state, stmt->setter);
                tin_gcmem_memrealloc(state, expr, sizeof(TinAstFieldExpr), 0);
            }
            break;
        default:
            {
                tin_state_raiseerror(state, COMPILE_ERROR, "Unknown expression type %d", (int)expr->type);
            }
            break;
    }
}

static TinAstExpression* tin_ast_allocexpr(TinState* state, uint64_t line, size_t size, TinAstExprType type)
{
    TinAstExpression* object;
    object = (TinAstExpression*)tin_gcmem_memrealloc(state, NULL, 0, size);
    object->type = type;
    object->line = line;
    return object;
}

TinAstLiteralExpr* tin_ast_make_literalexpr(TinState* state, size_t line, TinValue value)
{
    TinAstLiteralExpr* expr;
    expr = (TinAstLiteralExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstLiteralExpr), TINEXPR_LITERAL);
    expr->value = value;
    return expr;
}

TinAstBinaryExpr* tin_ast_make_binaryexpr(TinState* state, size_t line, TinAstExpression* left, TinAstExpression* right, TinAstTokType op)
{
    TinAstBinaryExpr* expr;
    expr = (TinAstBinaryExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstBinaryExpr), TINEXPR_BINARY);
    expr->left = left;
    expr->right = right;
    expr->op = op;
    expr->ignore_left = false;
    return expr;
}

TinAstUnaryExpr* tin_ast_make_unaryexpr(TinState* state, size_t line, TinAstExpression* right, TinAstTokType op)
{
    TinAstUnaryExpr* expr;
    expr = (TinAstUnaryExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstUnaryExpr), TINEXPR_UNARY);
    expr->right = right;
    expr->op = op;
    return expr;
}

TinAstVarExpr* tin_ast_make_varexpr(TinState* state, size_t line, const char* name, size_t length)
{
    TinAstVarExpr* expr;
    expr = (TinAstVarExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstVarExpr), TINEXPR_VAREXPR);
    expr->name = name;
    expr->length = length;
    return expr;
}

TinAstAssignExpr* tin_ast_make_assignexpr(TinState* state, size_t line, TinAstExpression* to, TinAstExpression* value)
{
    TinAstAssignExpr* expr;
    expr = (TinAstAssignExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstAssignExpr), TINEXPR_ASSIGN);
    expr->to = to;
    expr->value = value;
    return expr;
}

TinAstCallExpr* tin_ast_make_callexpr(TinState* state, size_t line, TinAstExpression* callee, TinString* name)
{
    TinAstCallExpr* expr;
    expr = (TinAstCallExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstCallExpr), TINEXPR_CALL);
    expr->callee = callee;
    expr->init = NULL;
    expr->name = name;
    tin_exprlist_init(&expr->args);
    return expr;
}

TinAstGetExpr* tin_ast_make_getexpr(TinState* state, size_t line, TinAstExpression* where, const char* name, size_t length, bool questionable, bool ignoreresult)
{
    TinAstGetExpr* expr;
    expr = (TinAstGetExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstGetExpr), TINEXPR_GET);
    expr->where = where;
    expr->name = name;
    expr->length = length;
    expr->ignemit = false;
    expr->jump = questionable ? 0 : -1;
    expr->ignresult = ignoreresult;
    return expr;
}

TinAstSetExpr* tin_ast_make_setexpr(TinState* state, size_t line, TinAstExpression* where, const char* name, size_t length, TinAstExpression* value)
{
    TinAstSetExpr* expr;
    expr = (TinAstSetExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstSetExpr), TINEXPR_SET);
    expr->where = where;
    expr->name = name;
    expr->length = length;
    expr->value = value;
    return expr;
}

TinAstFunctionExpr* tin_ast_make_lambdaexpr(TinState* state, size_t line)
{
    TinAstFunctionExpr* expr;
    expr = (TinAstFunctionExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstFunctionExpr), TINEXPR_LAMBDA);
    expr->body = NULL;
    tin_paramlist_init(&expr->parameters);
    return expr;
}

TinAstArrayExpr* tin_ast_make_arrayexpr(TinState* state, size_t line)
{
    TinAstArrayExpr* expr;
    expr = (TinAstArrayExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstArrayExpr), TINEXPR_ARRAY);
    tin_exprlist_init(&expr->values);
    return expr;
}

TinAstObjectExpr* tin_ast_make_objectexpr(TinState* state, size_t line)
{
    TinAstObjectExpr* expr;
    expr = (TinAstObjectExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstObjectExpr), TINEXPR_OBJECT);
    tin_exprlist_init(&expr->keys);
    tin_exprlist_init(&expr->values);
    return expr;
}

TinAstIndexExpr* tin_ast_make_subscriptexpr(TinState* state, size_t line, TinAstExpression* array, TinAstExpression* index)
{
    TinAstIndexExpr* expr;
    expr = (TinAstIndexExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstIndexExpr), TINEXPR_SUBSCRIPT);
    expr->array = array;
    expr->index = index;
    return expr;
}

TinAstThisExpr* tin_ast_make_thisexpr(TinState* state, size_t line)
{
    return (TinAstThisExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstThisExpr), TINEXPR_THIS);
}

TinAstSuperExpr* tin_ast_make_superexpr(TinState* state, size_t line, TinString* method, bool ignoreresult)
{
    TinAstSuperExpr* expr;
    expr = (TinAstSuperExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstSuperExpr), TINEXPR_SUPER);
    expr->method = method;
    expr->ignemit = false;
    expr->ignresult = ignoreresult;
    return expr;
}

TinAstRangeExpr* tin_ast_make_rangeexpr(TinState* state, size_t line, TinAstExpression* from, TinAstExpression* to)
{
    TinAstRangeExpr* expr;
    expr = (TinAstRangeExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstRangeExpr), TINEXPR_RANGE);
    expr->from = from;
    expr->to = to;
    return expr;
}

TinAstTernaryExpr* tin_ast_make_ternaryexpr(TinState* state, size_t line, TinAstExpression* condition, TinAstExpression* ifbranch, TinAstExpression* elsebranch)
{
    TinAstTernaryExpr* expr;
    expr = (TinAstTernaryExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstTernaryExpr), TINEXPR_TERNARY);
    expr->condition = condition;
    expr->ifbranch = ifbranch;
    expr->elsebranch = elsebranch;
    return expr;
}

TinAstStrInterExpr* tin_ast_make_strinterpolexpr(TinState* state, size_t line)
{
    TinAstStrInterExpr* expr;
    expr = (TinAstStrInterExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstStrInterExpr), TINEXPR_INTERPOLATION);
    tin_exprlist_init(&expr->expressions);
    return expr;
}

TinAstRefExpr* tin_ast_make_referenceexpr(TinState* state, size_t line, TinAstExpression* to)
{
    TinAstRefExpr* expr;
    expr = (TinAstRefExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstRefExpr), TINEXPR_REFERENCE);
    expr->to = to;
    return expr;
}

static TinAstExpression* tin_ast_allocstmt(TinState* state, uint64_t line, size_t size, TinAstExprType type)
{
    TinAstExpression* object;
    object = (TinAstExpression*)tin_gcmem_memrealloc(state, NULL, 0, size);
    object->type = type;
    object->line = line;
    return object;
}

TinAstExprExpr* tin_ast_make_exprstmt(TinState* state, size_t line, TinAstExpression* expr)
{
    TinAstExprExpr* stmt;
    stmt = (TinAstExprExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstExprExpr), TINEXPR_EXPRESSION);
    stmt->expression = expr;
    stmt->pop = true;
    return stmt;
}

TinAstBlockExpr* tin_ast_make_blockexpr(TinState* state, size_t line)
{
    TinAstBlockExpr* stmt;
    stmt = (TinAstBlockExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstBlockExpr), TINEXPR_BLOCK);
    tin_exprlist_init(&stmt->statements);
    return stmt;
}

TinAstAssignVarExpr* tin_ast_make_assignvarexpr(TinState* state, size_t line, const char* name, size_t length, TinAstExpression* init, bool constant)
{
    TinAstAssignVarExpr* stmt;
    stmt = (TinAstAssignVarExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstAssignVarExpr), TINEXPR_VARSTMT);
    stmt->name = name;
    stmt->length = length;
    stmt->init = init;
    stmt->constant = constant;
    return stmt;
}

TinAstIfExpr* tin_ast_make_ifexpr(TinState* state,
                                        size_t line,
                                        TinAstExpression* condition,
                                        TinAstExpression* ifbranch,
                                        TinAstExpression* elsebranch,
                                        TinAstExprList* elseifconditions,
                                        TinAstExprList* elseifbranches)
{
    TinAstIfExpr* stmt;
    stmt = (TinAstIfExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstIfExpr), TINEXPR_IFSTMT);
    stmt->condition = condition;
    stmt->ifbranch = ifbranch;
    stmt->elsebranch = elsebranch;
    stmt->elseifconds = elseifconditions;
    stmt->elseifbranches = elseifbranches;
    return stmt;
}

TinAstWhileExpr* tin_ast_make_whileexpr(TinState* state, size_t line, TinAstExpression* condition, TinAstExpression* body)
{
    TinAstWhileExpr* stmt;
    stmt = (TinAstWhileExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstWhileExpr), TINEXPR_WHILE);
    stmt->condition = condition;
    stmt->body = body;
    return stmt;
}

TinAstForExpr* tin_ast_make_forexpr(TinState* state,
                                          size_t line,
                                          TinAstExpression* init,
                                          TinAstExpression* var,
                                          TinAstExpression* condition,
                                          TinAstExpression* increment,
                                          TinAstExpression* body,
                                          bool cstyle)
{
    TinAstForExpr* stmt;
    stmt = (TinAstForExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstForExpr), TINEXPR_FOR);
    stmt->init = init;
    stmt->var = var;
    stmt->condition = condition;
    stmt->increment = increment;
    stmt->body = body;
    stmt->cstyle = cstyle;
    return stmt;
}

TinAstContinueExpr* tin_ast_make_continueexpr(TinState* state, size_t line)
{
    return (TinAstContinueExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstContinueExpr), TINEXPR_CONTINUE);
}

TinAstBreakExpr* tin_ast_make_breakexpr(TinState* state, size_t line)
{
    return (TinAstBreakExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstBreakExpr), TINEXPR_BREAK);
}

TinAstFunctionExpr* tin_ast_make_funcexpr(TinState* state, size_t line, const char* name, size_t length)
{
    TinAstFunctionExpr* function;
    function = (TinAstFunctionExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstFunctionExpr), TINEXPR_FUNCTION);
    function->name = name;
    function->length = length;
    function->body = NULL;
    tin_paramlist_init(&function->parameters);
    return function;
}

TinAstReturnExpr* tin_ast_make_returnexpr(TinState* state, size_t line, TinAstExpression* expr)
{
    TinAstReturnExpr* stmt;
    stmt = (TinAstReturnExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstReturnExpr), TINEXPR_RETURN);
    stmt->expression = expr;
    return stmt;
}

TinAstMethodExpr* tin_ast_make_methodexpr(TinState* state, size_t line, TinString* name, bool isstatic)
{
    TinAstMethodExpr* stmt;
    stmt = (TinAstMethodExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstMethodExpr), TINEXPR_METHOD);
    stmt->name = name;
    stmt->body = NULL;
    stmt->isstatic = isstatic;
    tin_paramlist_init(&stmt->parameters);
    return stmt;
}

TinAstClassExpr* tin_ast_make_classexpr(TinState* state, size_t line, TinString* name, TinString* parent)
{
    TinAstClassExpr* stmt;
    stmt = (TinAstClassExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstClassExpr), TINEXPR_CLASS);
    stmt->name = name;
    stmt->parent = parent;
    tin_exprlist_init(&stmt->fields);
    return stmt;
}

TinAstFieldExpr* tin_ast_make_fieldexpr(TinState* state, size_t line, TinString* name, TinAstExpression* getter, TinAstExpression* setter, bool isstatic)
{
    TinAstFieldExpr* stmt;
    stmt = (TinAstFieldExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstFieldExpr), TINEXPR_FIELD);
    stmt->name = name;
    stmt->getter = getter;
    stmt->setter = setter;
    stmt->isstatic = isstatic;
    return stmt;
}

TinAstExprList* tin_ast_allocexprlist(TinState* state)
{
    TinAstExprList* expressions;
    expressions = (TinAstExprList*)tin_gcmem_memrealloc(state, NULL, 0, sizeof(TinAstExprList));
    tin_exprlist_init(expressions);
    return expressions;
}

void tin_ast_destroy_allocdexprlist(TinState* state, TinAstExprList* expressions)
{
    size_t i;
    if(expressions == NULL)
    {
        return;
    }
    for(i = 0; i < expressions->count; i++)
    {
        tin_ast_destroyexpression(state, expressions->values[i]);
    }
    tin_exprlist_destroy(state, expressions);
    tin_gcmem_memrealloc(state, expressions, sizeof(TinAstExprList), 0);
}

TinAstExprList* tin_ast_allocate_stmtlist(TinState* state)
{
    TinAstExprList* statements;
    statements = (TinAstExprList*)tin_gcmem_memrealloc(state, NULL, 0, sizeof(TinAstExprList));
    tin_exprlist_init(statements);
    return statements;
}

void tin_ast_destry_allocdstmtlist(TinState* state, TinAstExprList* statements)
{
    size_t i;
    if(statements == NULL)
    {
        return;
    }
    for(i = 0; i < statements->count; i++)
    {
        tin_ast_destroyexpression(state, statements->values[i]);
    }
    tin_exprlist_destroy(state, statements);
    tin_gcmem_memrealloc(state, statements, sizeof(TinAstExprList), 0);
}
