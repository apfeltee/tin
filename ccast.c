
#include "priv.h"

void tin_exprlist_init(TinAstExprList* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void tin_exprlist_destroy(TinState* state, TinAstExprList* array)
{
    TIN_FREE_ARRAY(state, sizeof(TinAstExpression*), array->values, array->capacity);
    tin_exprlist_init(array);
}

void tin_exprlist_push(TinState* state, TinAstExprList* array, TinAstExpression* value)
{
    size_t oldcapacity;
    if(array->capacity < array->count + 1)
    {
        oldcapacity = array->capacity;
        array->capacity = TIN_GROW_CAPACITY(oldcapacity);
        array->values = TIN_GROW_ARRAY(state, array->values, sizeof(TinAstExpression*), oldcapacity, array->capacity);
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
    TIN_FREE_ARRAY(state, sizeof(TinAstParameter), array->values, array->capacity);
    tin_paramlist_init(array);
}

void tin_paramlist_push(TinState* state, TinAstParamList* array, TinAstParameter value)
{
    if(array->capacity < array->count + 1)
    {
        size_t oldcapacity = array->capacity;
        array->capacity = TIN_GROW_CAPACITY(oldcapacity);
        array->values = TIN_GROW_ARRAY(state, array->values, sizeof(TinAstParameter), oldcapacity, array->capacity);
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

void tin_ast_destroyexpression(TinState* state, TinAstExpression* expression)
{
    if(expression == NULL)
    {
        return;
    }
    switch(expression->type)
    {
        case TINEXPR_LITERAL:
            {
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstLiteralExpr), 0);
            }
            break;
        case TINEXPR_BINARY:
            {
                TinAstBinaryExpr* expr = (TinAstBinaryExpr*)expression;
                if(!expr->ignore_left)
                {
                    tin_ast_destroyexpression(state, expr->left);
                }
                tin_ast_destroyexpression(state, expr->right);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstBinaryExpr), 0);
            }
            break;

        case TINEXPR_UNARY:
            {
                tin_ast_destroyexpression(state, ((TinAstUnaryExpr*)expression)->right);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstUnaryExpr), 0);
            }
            break;
        case TINEXPR_VAREXPR:
            {
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstVarExpr), 0);
            }
            break;
        case TINEXPR_ASSIGN:
            {
                TinAstAssignExpr* expr = (TinAstAssignExpr*)expression;
                tin_ast_destroyexpression(state, expr->to);
                tin_ast_destroyexpression(state, expr->value);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstAssignExpr), 0);
            }
            break;
        case TINEXPR_CALL:
            {
                TinAstCallExpr* expr = (TinAstCallExpr*)expression;
                tin_ast_destroyexpression(state, expr->callee);
                tin_ast_destroyexpression(state, expr->init);
                tin_ast_destroyexprlist(state, &expr->args);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstCallExpr), 0);
            }
            break;

        case TINEXPR_GET:
        {
            tin_ast_destroyexpression(state, ((TinAstGetExpr*)expression)->where);
            tin_gcmem_memrealloc(state, expression, sizeof(TinAstGetExpr), 0);
            break;
        }

        case TINEXPR_SET:
        {
            TinAstSetExpr* expr = (TinAstSetExpr*)expression;

            tin_ast_destroyexpression(state, expr->where);
            tin_ast_destroyexpression(state, expr->value);

            tin_gcmem_memrealloc(state, expression, sizeof(TinAstSetExpr), 0);
            break;
        }

        case TINEXPR_LAMBDA:
            {
                TinAstFunctionExpr* expr = (TinAstFunctionExpr*)expression;
                tin_paramlist_destroyvalues(state, &expr->parameters);
                tin_ast_destroyexpression(state, expr->body);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstFunctionExpr), 0);
            }
            break;
        case TINEXPR_ARRAY:
            {
                tin_ast_destroyexprlist(state, &((TinAstArrayExpr*)expression)->values);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstArrayExpr), 0);
            }
            break;
        case TINEXPR_OBJECT:
            {
                TinAstObjectExpr* map = (TinAstObjectExpr*)expression;
                tin_ast_destroyexprlist(state, &map->keys);
                tin_ast_destroyexprlist(state, &map->values);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstObjectExpr), 0);
            }
            break;
        case TINEXPR_SUBSCRIPT:
            {
                TinAstIndexExpr* expr = (TinAstIndexExpr*)expression;
                tin_ast_destroyexpression(state, expr->array);
                tin_ast_destroyexpression(state, expr->index);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstIndexExpr), 0);
            }
            break;
        case TINEXPR_THIS:
            {
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstThisExpr), 0);
            }
            break;
        case TINEXPR_SUPER:
            {
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstSuperExpr), 0);
            }
            break;
        case TINEXPR_RANGE:
            {
                TinAstRangeExpr* expr = (TinAstRangeExpr*)expression;
                tin_ast_destroyexpression(state, expr->from);
                tin_ast_destroyexpression(state, expr->to);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstRangeExpr), 0);
            }
            break;
        case TINEXPR_TERNARY:
            {
                TinAstTernaryExpr* expr = (TinAstTernaryExpr*)expression;
                tin_ast_destroyexpression(state, expr->condition);
                tin_ast_destroyexpression(state, expr->ifbranch);
                tin_ast_destroyexpression(state, expr->elsebranch);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstTernaryExpr), 0);
            }
            break;
        case TINEXPR_INTERPOLATION:
            {
                tin_ast_destroyexprlist(state, &((TinAstStrInterExpr*)expression)->expressions);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstStrInterExpr), 0);
            }
            break;
        case TINEXPR_REFERENCE:
            {
                tin_ast_destroyexpression(state, ((TinAstRefExpr*)expression)->to);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstRefExpr), 0);
            }
            break;
        case TINEXPR_EXPRESSION:
            {
                tin_ast_destroyexpression(state, ((TinAstExprExpr*)expression)->expression);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstExprExpr), 0);
            }
            break;
        case TINEXPR_BLOCK:
            {
                tin_ast_destroystmtlist(state, &((TinAstBlockExpr*)expression)->statements);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstBlockExpr), 0);
            }
            break;
        case TINEXPR_VARSTMT:
            {
                tin_ast_destroyexpression(state, ((TinAstAssignVarExpr*)expression)->init);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstAssignVarExpr), 0);
            }
            break;
        case TINEXPR_IFSTMT:
            {
                TinAstIfExpr* stmt = (TinAstIfExpr*)expression;
                tin_ast_destroyexpression(state, stmt->condition);
                tin_ast_destroyexpression(state, stmt->ifbranch);
                tin_ast_destroy_allocdexprlist(state, stmt->elseifconds);
                tin_ast_destry_allocdstmtlist(state, stmt->elseifbranches);
                tin_ast_destroyexpression(state, stmt->elsebranch);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstIfExpr), 0);
            }
            break;
        case TINEXPR_WHILE:
            {
                TinAstWhileExpr* stmt = (TinAstWhileExpr*)expression;
                tin_ast_destroyexpression(state, stmt->condition);
                tin_ast_destroyexpression(state, stmt->body);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstWhileExpr), 0);
            }
            break;
        case TINEXPR_FOR:
            {
                TinAstForExpr* stmt = (TinAstForExpr*)expression;
                tin_ast_destroyexpression(state, stmt->increment);
                tin_ast_destroyexpression(state, stmt->condition);
                tin_ast_destroyexpression(state, stmt->init);

                tin_ast_destroyexpression(state, stmt->var);
                tin_ast_destroyexpression(state, stmt->body);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstForExpr), 0);
            }
            break;
        case TINEXPR_CONTINUE:
            {
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstContinueExpr), 0);
            }
            break;
        case TINEXPR_BREAK:
            {
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstBreakExpr), 0);
            }
            break;
        case TINEXPR_FUNCTION:
            {
                TinAstFunctionExpr* stmt = (TinAstFunctionExpr*)expression;
                tin_ast_destroyexpression(state, stmt->body);
                tin_paramlist_destroyvalues(state, &stmt->parameters);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstFunctionExpr), 0);
            }
            break;
        case TINEXPR_RETURN:
            {
                tin_ast_destroyexpression(state, ((TinAstReturnExpr*)expression)->expression);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstReturnExpr), 0);
            }
            break;
        case TINEXPR_METHOD:
            {
                TinAstMethodExpr* stmt = (TinAstMethodExpr*)expression;
                tin_paramlist_destroyvalues(state, &stmt->parameters);
                tin_ast_destroyexpression(state, stmt->body);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstMethodExpr), 0);
            }
            break;
        case TINEXPR_CLASS:
            {
                tin_ast_destroystmtlist(state, &((TinAstClassExpr*)expression)->fields);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstClassExpr), 0);
            }
            break;
        case TINEXPR_FIELD:
            {
                TinAstFieldExpr* stmt = (TinAstFieldExpr*)expression;
                tin_ast_destroyexpression(state, stmt->getter);
                tin_ast_destroyexpression(state, stmt->setter);
                tin_gcmem_memrealloc(state, expression, sizeof(TinAstFieldExpr), 0);
            }
            break;
        default:
            {
                tin_state_raiseerror(state, COMPILE_ERROR, "Unknown expression type %d", (int)expression->type);
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
    TinAstLiteralExpr* expression;
    expression = (TinAstLiteralExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstLiteralExpr), TINEXPR_LITERAL);
    expression->value = value;
    return expression;
}

TinAstBinaryExpr* tin_ast_make_binaryexpr(TinState* state, size_t line, TinAstExpression* left, TinAstExpression* right, TinAstTokType op)
{
    TinAstBinaryExpr* expression;
    expression = (TinAstBinaryExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstBinaryExpr), TINEXPR_BINARY);
    expression->left = left;
    expression->right = right;
    expression->op = op;
    expression->ignore_left = false;
    return expression;
}

TinAstUnaryExpr* tin_ast_make_unaryexpr(TinState* state, size_t line, TinAstExpression* right, TinAstTokType op)
{
    TinAstUnaryExpr* expression;
    expression = (TinAstUnaryExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstUnaryExpr), TINEXPR_UNARY);
    expression->right = right;
    expression->op = op;
    return expression;
}

TinAstVarExpr* tin_ast_make_varexpr(TinState* state, size_t line, const char* name, size_t length)
{
    TinAstVarExpr* expression;
    expression = (TinAstVarExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstVarExpr), TINEXPR_VAREXPR);
    expression->name = name;
    expression->length = length;
    return expression;
}

TinAstAssignExpr* tin_ast_make_assignexpr(TinState* state, size_t line, TinAstExpression* to, TinAstExpression* value)
{
    TinAstAssignExpr* expression;
    expression = (TinAstAssignExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstAssignExpr), TINEXPR_ASSIGN);
    expression->to = to;
    expression->value = value;
    return expression;
}

TinAstCallExpr* tin_ast_make_callexpr(TinState* state, size_t line, TinAstExpression* callee)
{
    TinAstCallExpr* expression;
    expression = (TinAstCallExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstCallExpr), TINEXPR_CALL);
    expression->callee = callee;
    expression->init = NULL;
    tin_exprlist_init(&expression->args);
    return expression;
}

TinAstGetExpr* tin_ast_make_getexpr(TinState* state, size_t line, TinAstExpression* where, const char* name, size_t length, bool questionable, bool ignoreresult)
{
    TinAstGetExpr* expression;
    expression = (TinAstGetExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstGetExpr), TINEXPR_GET);
    expression->where = where;
    expression->name = name;
    expression->length = length;
    expression->ignemit = false;
    expression->jump = questionable ? 0 : -1;
    expression->ignresult = ignoreresult;
    return expression;
}

TinAstSetExpr* tin_ast_make_setexpr(TinState* state, size_t line, TinAstExpression* where, const char* name, size_t length, TinAstExpression* value)
{
    TinAstSetExpr* expression;
    expression = (TinAstSetExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstSetExpr), TINEXPR_SET);
    expression->where = where;
    expression->name = name;
    expression->length = length;
    expression->value = value;
    return expression;
}

TinAstFunctionExpr* tin_ast_make_lambdaexpr(TinState* state, size_t line)
{
    TinAstFunctionExpr* expression;
    expression = (TinAstFunctionExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstFunctionExpr), TINEXPR_LAMBDA);
    expression->body = NULL;
    tin_paramlist_init(&expression->parameters);
    return expression;
}

TinAstArrayExpr* tin_ast_make_arrayexpr(TinState* state, size_t line)
{
    TinAstArrayExpr* expression;
    expression = (TinAstArrayExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstArrayExpr), TINEXPR_ARRAY);
    tin_exprlist_init(&expression->values);
    return expression;
}

TinAstObjectExpr* tin_ast_make_objectexpr(TinState* state, size_t line)
{
    TinAstObjectExpr* expression;
    expression = (TinAstObjectExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstObjectExpr), TINEXPR_OBJECT);
    tin_exprlist_init(&expression->keys);
    tin_exprlist_init(&expression->values);
    return expression;
}

TinAstIndexExpr* tin_ast_make_subscriptexpr(TinState* state, size_t line, TinAstExpression* array, TinAstExpression* index)
{
    TinAstIndexExpr* expression;
    expression = (TinAstIndexExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstIndexExpr), TINEXPR_SUBSCRIPT);
    expression->array = array;
    expression->index = index;
    return expression;
}

TinAstThisExpr* tin_ast_make_thisexpr(TinState* state, size_t line)
{
    return (TinAstThisExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstThisExpr), TINEXPR_THIS);
}

TinAstSuperExpr* tin_ast_make_superexpr(TinState* state, size_t line, TinString* method, bool ignoreresult)
{
    TinAstSuperExpr* expression;
    expression = (TinAstSuperExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstSuperExpr), TINEXPR_SUPER);
    expression->method = method;
    expression->ignemit = false;
    expression->ignresult = ignoreresult;
    return expression;
}

TinAstRangeExpr* tin_ast_make_rangeexpr(TinState* state, size_t line, TinAstExpression* from, TinAstExpression* to)
{
    TinAstRangeExpr* expression;
    expression = (TinAstRangeExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstRangeExpr), TINEXPR_RANGE);
    expression->from = from;
    expression->to = to;
    return expression;
}

TinAstTernaryExpr* tin_ast_make_ternaryexpr(TinState* state, size_t line, TinAstExpression* condition, TinAstExpression* ifbranch, TinAstExpression* elsebranch)
{
    TinAstTernaryExpr* expression;
    expression = (TinAstTernaryExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstTernaryExpr), TINEXPR_TERNARY);
    expression->condition = condition;
    expression->ifbranch = ifbranch;
    expression->elsebranch = elsebranch;

    return expression;
}

TinAstStrInterExpr* tin_ast_make_strinterpolexpr(TinState* state, size_t line)
{
    TinAstStrInterExpr* expression;
    expression = (TinAstStrInterExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstStrInterExpr), TINEXPR_INTERPOLATION);
    tin_exprlist_init(&expression->expressions);
    return expression;
}

TinAstRefExpr* tin_ast_make_referenceexpr(TinState* state, size_t line, TinAstExpression* to)
{
    TinAstRefExpr* expression;
    expression = (TinAstRefExpr*)tin_ast_allocexpr(state, line, sizeof(TinAstRefExpr), TINEXPR_REFERENCE);
    expression->to = to;
    return expression;
}



static TinAstExpression* tin_ast_allocstmt(TinState* state, uint64_t line, size_t size, TinAstExprType type)
{
    TinAstExpression* object;
    object = (TinAstExpression*)tin_gcmem_memrealloc(state, NULL, 0, size);
    object->type = type;
    object->line = line;
    return object;
}

TinAstExprExpr* tin_ast_make_exprstmt(TinState* state, size_t line, TinAstExpression* expression)
{
    TinAstExprExpr* statement;
    statement = (TinAstExprExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstExprExpr), TINEXPR_EXPRESSION);
    statement->expression = expression;
    statement->pop = true;
    return statement;
}

TinAstBlockExpr* tin_ast_make_blockexpr(TinState* state, size_t line)
{
    TinAstBlockExpr* statement;
    statement = (TinAstBlockExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstBlockExpr), TINEXPR_BLOCK);
    tin_exprlist_init(&statement->statements);
    return statement;
}

TinAstAssignVarExpr* tin_ast_make_assignvarexpr(TinState* state, size_t line, const char* name, size_t length, TinAstExpression* init, bool constant)
{
    TinAstAssignVarExpr* statement;
    statement = (TinAstAssignVarExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstAssignVarExpr), TINEXPR_VARSTMT);
    statement->name = name;
    statement->length = length;
    statement->init = init;
    statement->constant = constant;
    return statement;
}

TinAstIfExpr* tin_ast_make_ifexpr(TinState* state,
                                        size_t line,
                                        TinAstExpression* condition,
                                        TinAstExpression* ifbranch,
                                        TinAstExpression* elsebranch,
                                        TinAstExprList* elseifconditions,
                                        TinAstExprList* elseifbranches)
{
    TinAstIfExpr* statement;
    statement = (TinAstIfExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstIfExpr), TINEXPR_IFSTMT);
    statement->condition = condition;
    statement->ifbranch = ifbranch;
    statement->elsebranch = elsebranch;
    statement->elseifconds = elseifconditions;
    statement->elseifbranches = elseifbranches;
    return statement;
}

TinAstWhileExpr* tin_ast_make_whileexpr(TinState* state, size_t line, TinAstExpression* condition, TinAstExpression* body)
{
    TinAstWhileExpr* statement;
    statement = (TinAstWhileExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstWhileExpr), TINEXPR_WHILE);
    statement->condition = condition;
    statement->body = body;
    return statement;
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
    TinAstForExpr* statement;
    statement = (TinAstForExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstForExpr), TINEXPR_FOR);
    statement->init = init;
    statement->var = var;
    statement->condition = condition;
    statement->increment = increment;
    statement->body = body;
    statement->cstyle = cstyle;
    return statement;
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

TinAstReturnExpr* tin_ast_make_returnexpr(TinState* state, size_t line, TinAstExpression* expression)
{
    TinAstReturnExpr* statement;
    statement = (TinAstReturnExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstReturnExpr), TINEXPR_RETURN);
    statement->expression = expression;
    return statement;
}

TinAstMethodExpr* tin_ast_make_methodexpr(TinState* state, size_t line, TinString* name, bool isstatic)
{
    TinAstMethodExpr* statement;
    statement = (TinAstMethodExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstMethodExpr), TINEXPR_METHOD);
    statement->name = name;
    statement->body = NULL;
    statement->isstatic = isstatic;
    tin_paramlist_init(&statement->parameters);
    return statement;
}

TinAstClassExpr* tin_ast_make_classexpr(TinState* state, size_t line, TinString* name, TinString* parent)
{
    TinAstClassExpr* statement;
    statement = (TinAstClassExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstClassExpr), TINEXPR_CLASS);
    statement->name = name;
    statement->parent = parent;
    tin_exprlist_init(&statement->fields);
    return statement;
}

TinAstFieldExpr* tin_ast_make_fieldexpr(TinState* state, size_t line, TinString* name, TinAstExpression* getter, TinAstExpression* setter, bool isstatic)
{
    TinAstFieldExpr* statement;
    statement = (TinAstFieldExpr*)tin_ast_allocstmt(state, line, sizeof(TinAstFieldExpr), TINEXPR_FIELD);
    statement->name = name;
    statement->getter = getter;
    statement->setter = setter;
    statement->isstatic = isstatic;
    return statement;
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
